#include "MainView.h"

namespace ifx
{

namespace
{
    float rawValue (InterferenceProcessor& p, int param) { return p.apvts.getRawParameterValue (paramInfo (param).id)->load(); }
    const char* modeNames[] = { "Hardware", "Play", "Create" };
    const char* tabNames[] = { "Gesture Lane", "Studio", "Create", "Routing", "Environment" };

    juce::Path gearPath (juce::Rectangle<float> r)
    {
        juce::Path p;
        const auto c = r.getCentre();
        const float ro = r.getWidth() * 0.5f, ri = ro * 0.72f;
        for (int i = 0; i < 16; ++i)
        {
            const float a = (float) i / 16.0f * juce::MathConstants<float>::twoPi;
            const float rr = (i % 2 == 0) ? ro : ri;
            const juce::Point<float> pt (c.x + std::sin (a) * rr, c.y - std::cos (a) * rr);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        p.addEllipse (juce::Rectangle<float> (ro * 0.7f, ro * 0.7f).withCentre (c));
        p.setUsingNonZeroWinding (false);
        return p;
    }
}

MainView::MainView (InterferenceProcessor& p)
    : proc (p), palette (p), policy (p, P::policy),
      cards { { p, 0 }, { p, 1 } }, context (p), orderToggle (p, P::swapOrder, "A > B"),
      lane (p), studio (p), create (p), routing (p), environment (p), output (p)
{
    setWantsKeyboardFocus (true);

    // ---- header
    addAndMakeVisible (led);
    led.setTooltip ("Output level: dark = silence, brighter towards 0 dB, red above 0 dBFS.");
    menuButton.setTooltip ("Save, Save As, Open, Options, open the preset folder.");
    menuButton.onClick = [this] { showMainMenu(); };
    presetBox.setTooltip ("Factory and user presets. A * after the name means the sound was changed since it was loaded.");
    presetBox.setTitle ("Preset");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < 1000) proc.loadFactoryPreset (id - 1);
        else if (juce::isPositiveAndBelow (id - 1000, userPresets.size()))
        {
            juce::String err;
            if (! proc.loadPresetFile (userPresets[id - 1000], err))
                showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                                  .withTitle ("Could not load preset").withMessage (err).withButton ("OK"));
        }
        grabKeyboardFocus();
    };
    prevPreset.setTooltip ("Previous preset");
    nextPreset.setTooltip ("Next preset");
    prevPreset.onClick = [this] { const int n = presetBox.getNumItems(); if (n > 0) presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() - 1 + n) % n); };
    nextPreset.onClick = [this] { const int n = presetBox.getNumItems(); if (n > 0) presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + 1) % n); };
    aButton.setTooltip ("Compare: settings A");
    bButton.setTooltip ("Compare: settings B");
    copyAB.setTooltip ("Copy the current settings to the other compare slot.");
    aButton.setClickingTogglesState (true);
    bButton.setClickingTogglesState (true);
    aButton.setRadioGroupId (0xAB);
    bButton.setRadioGroupId (0xAB);
    aButton.onClick = [this] { proc.selectAB (0); };
    bButton.onClick = [this] { proc.selectAB (1); };
    copyAB.onClick = [this] { proc.copyABTo (1 - proc.getABSlot()); flash (copyAB); };
    resetButton.setTooltip ("Reset every sound setting to its default (MIDI mappings are kept). Undo with A/B or reload the preset.");
    resetButton.onClick = [this] { proc.resetToDefaults(); flash (resetButton); };
    randomButton.setTooltip ("Randomize: resets everything, then picks a new set of safe, Natural settings. Each press is a new sound.");
    randomButton.onClick = [this] { proc.randomize(); flash (randomButton); };
    helpButton.setTooltip ("Help: every feature, workflows, troubleshooting, version and licence (F1).");
    helpButton.onClick = [this] { showHelp(); };
    gearButton.setTooltip ("Options: tooltips, audio / MIDI devices, MIDI mappings, folders.");
    gearButton.onClick = [this] { showOptions(); };
    for (auto* c : std::initializer_list<juce::Component*> { &menuButton, &presetBox, &prevPreset, &nextPreset, &aButton, &bButton, &copyAB,
                                                             &resetButton, &randomButton, &helpButton, &gearButton })
        addAndMakeVisible (c);

    // ---- mode row
    for (int i = 0; i < 3; ++i)
    {
        modeButtons[i].setButtonText (modeNames[i]);
        modeButtons[i].setClickingTogglesState (true);
        modeButtons[i].setRadioGroupId (0x30DE);
        modeButtons[i].setTooltip (i == 0 ? "Hardware: an insert effect with subtle, asynchronous faults."
                                 : i == 1 ? "Play: MIDI, GUI gestures, automation or the Gesture Lane disturb the hardware in time."
                                          : "Create: internal excitation reveals faults on a silent track; render one-shots and loops.");
        modeButtons[i].onClick = [this, i]
        {
            auto* prm = proc.param (P::mode);
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
            prm->endChangeGesture();
        };
        addAndMakeVisible (modeButtons[i]);
    }
    addAndMakeVisible (palette);
    seedLabel.setFont (Theme::monoFont (12.0f));
    seedLabel.setColour (juce::Label::textColourId, Theme::text);
    seedLabel.setColour (juce::Label::outlineColourId, Theme::edge);
    seedLabel.setEditable (false, true, false);
    seedLabel.setTooltip ("Seed (hex). Double-click to type one. Fault maps (worn arcs, weak spots) depend on the Seed only.");
    seedLabel.onTextChange = [this]
    {
        const auto t = seedLabel.getText().trim().retainCharacters ("0123456789abcdefABCDEF");
        if (t.isNotEmpty()) proc.setSeed ((uint64_t) t.getHexValue64());
    };
    addAndMakeVisible (seedLabel);
    lockButton.setClickingTogglesState (true);
    lockButton.setTooltip ("Lock: identical audio for the same host position and state (bounce, loop, render). Unlocked: each transport start is a new take.");
    lockButton.onClick = [this] { proc.setSeedLock (lockButton.getToggleState()); };
    takeButton.setTooltip ("New Take: keeps timing, positions and fault maps; draws different micro-events.");
    takeButton.onClick = [this] { proc.newTake(); flash (takeButton); };
    addAndMakeVisible (lockButton);
    addAndMakeVisible (takeButton);
    addAndMakeVisible (policy);

    // ---- slots
    for (int s = 0; s < 2; ++s)
    {
        cards[s].onSelect = [this] (int slot) { selectSlot (slot); };
        cards[s].onChooseSource = [this] (int slot, SourceType t) { proc.setSlotSource (slot, t); selectSlot (slot); };
        cards[s].onGuideToWear = [this] (int slot) { selectSlot (slot); context.moveTowardWornArea(); };
        addAndMakeVisible (cards[s]);
    }
    orderToggle.setTooltip ("Processing order of the two slots (Swap Order). A -> B: slot A's output feeds slot B, so a "
                            "downstream open contact interrupts the upstream source's artifacts. Click to swap; settings stay in their slots.\n"
                            "Right-click: MIDI learn, reset, type a value.");
    addAndMakeVisible (orderToggle);
    addAndMakeVisible (context);
    context.onLayoutChanged = [this] { layoutStream(); };
    rebuildIllustrations();

    // ---- bottom tabs
    for (int i = 0; i < 5; ++i)
    {
        tabButtons[i].setButtonText (tabNames[i]);
        tabButtons[i].setClickingTogglesState (true);
        tabButtons[i].setRadioGroupId (0x7AB);
        tabButtons[i].onClick = [this, i] { selectTab (i); };
        addAndMakeVisible (tabButtons[i]);
    }
    excBadge.setClickingTogglesState (false);
    excBadge.setToggleState (true, juce::dontSendNotification); // accent styling
    excBadge.setTooltip ("Internal excitation (a labelled studio source) is feeding the modeled circuit: part of what you hear is "
                         "this test signal revealing the faults, not the hardware on its own. Click to open Create.");
    excBadge.onClick = [this] { selectTab (2); };
    addChildComponent (excBadge);
    addChildComponent (lane);
    addChildComponent (studio);
    addChildComponent (create);
    addChildComponent (routing);
    addChildComponent (environment);
    addAndMakeVisible (output);
    create.onRender = [this] { showRender(); };
    create.onRecord = [this] { toggleRecord(); };
    addAndMakeVisible (stream);

    refreshPresetBox();
    selectTab ((int) rawValue (proc, P::mode) == 1 ? 0 : (int) rawValue (proc, P::mode) == 2 ? 2 : 1);
    startTimerHz (30);
}

MainView::~MainView()
{
    stopTimer();
}

//==============================================================================
void MainView::selectTab (int index)
{
    tab = juce::jlimit (0, 4, index);
    tabButtons[tab].setToggleState (true, juce::dontSendNotification);
    lane.setVisible (tab == 0);
    studio.setVisible (tab == 1);
    create.setVisible (tab == 2);
    routing.setVisible (tab == 3);
    environment.setVisible (tab == 4);
}

void MainView::selectSlot (int s)
{
    selectedSlot = juce::jlimit (0, 1, s);
    context.setSlot (selectedSlot, proc.slotType (selectedSlot));
    layoutStream();
}

void MainView::rebuildIllustrations()
{
    for (int s = 0; s < 2; ++s)
    {
        const auto t = proc.slotType (s);
        if (illus[s] != nullptr && illusType[s] == t)
            continue;
        illus[s] = createIllustration (proc, s, t);
        illusType[s] = t;
        addAndMakeVisible (*illus[s]);
    }
    context.setSlot (selectedSlot, proc.slotType (selectedSlot));
    resized();
}

void MainView::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& bank = FactoryPresets::all();
    for (const auto& cat : FactoryPresets::categories())
    {
        presetBox.addSectionHeading (cat);
        for (int i = 0; i < bank.size(); ++i)
            if (bank[i].category == cat)
                presetBox.addItem (bank[i].name, i + 1);
    }
    userPresets.clear();
    const auto folder = StateIO::userPresetFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + StateIO::kPresetExtension);
        files.sort();
        if (! files.isEmpty())
        {
            presetBox.addSectionHeading ("User");
            for (int j = 0; j < files.size() && j < 900; ++j)
            {
                userPresets.add (files[j]);
                presetBox.addItem (files[j].getFileNameWithoutExtension(), 1000 + j);
            }
        }
    }
    updateHeader();
}

void MainView::updateHeader()
{
    const auto name = proc.getPresetName();
    modified = proc.isModified();
    const juce::String shown = name + (modified ? " *" : "");
    // select the matching entry without triggering a load
    int id = 0;
    const auto& bank = FactoryPresets::all();
    for (int i = 0; i < bank.size(); ++i)
        if (bank[i].name == name) { id = i + 1; break; }
    for (int j = 0; j < userPresets.size() && id == 0; ++j)
        if (userPresets[j].getFileNameWithoutExtension() == name) id = 1000 + j;
    if (presetBox.getSelectedId() != id || presetBox.getText() != shown)
    {
        presetBox.setSelectedId (id, juce::dontSendNotification);
        presetBox.setText (shown, juce::dontSendNotification);
    }
    aButton.setToggleState (proc.getABSlot() == 0, juce::dontSendNotification);
    bButton.setToggleState (proc.getABSlot() == 1, juce::dontSendNotification);
    copyAB.setButtonText (proc.getABSlot() == 0 ? "A>B" : "B>A");
    lockButton.setToggleState (proc.getSeedLock(), juce::dontSendNotification);
    const auto seedText = juce::String::toHexString ((juce::int64) proc.getSeed()).toUpperCase() + (proc.getTake() > 0 ? " t" + juce::String ((int) proc.getTake()) : juce::String());
    if (! seedLabel.isBeingEdited() && seedLabel.getText() != seedText)
        seedLabel.setText (seedText, juce::dontSendNotification);
}

void MainView::showMainMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Save preset");
    m.addItem (2, "Save preset as...");
    m.addItem (3, "Open preset...");
    m.addSeparator();
    m.addItem (4, "Options...");
    m.addItem (5, "Open preset folder in Explorer");
    m.addSeparator();
    m.addItem (6, "Render WAV...");
    m.addItem (7, proc.recorder().isRecording() ? "Stop recording" : "Record output...");
    m.addSeparator();
    m.addItem (8, "Reset to defaults");
    m.addItem (9, "Randomize");
    m.addItem (10, "Help");
    juce::Component::SafePointer<MainView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton), [safe] (int r)
    {
        if (safe == nullptr) return;
        auto& v = *safe;
        switch (r)
        {
            case 1:
            {
                // Save overwrites only a user preset that is already the current one; factory presets go to Save As
                const auto f = StateIO::userPresetFolder().getChildFile (v.proc.getPresetName() + StateIO::kPresetExtension);
                const bool isFactory = v.proc.getPresetCategory().isNotEmpty() && FactoryPresets::categories().contains (v.proc.getPresetCategory());
                if (isFactory || v.proc.getPresetName() == "Init" || v.proc.getPresetName() == "Random" || ! f.existsAsFile())
                    v.savePresetAs();
                else
                {
                    juce::String err;
                    if (! v.proc.savePresetFile (f, err))
                        v.showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                                          .withTitle ("Save failed").withMessage (err).withButton ("OK"));
                    v.refreshPresetBox();
                }
                break;
            }
            case 2: v.savePresetAs(); break;
            case 3: v.openPreset(); break;
            case 4: v.showOptions(); break;
            case 5: { const auto f = StateIO::userPresetFolder(); f.createDirectory(); f.revealToUser(); break; }
            case 6: v.showRender(); break;
            case 7: v.toggleRecord(); break;
            case 8: v.proc.resetToDefaults(); break;
            case 9: v.proc.randomize(); break;
            case 10: v.showHelp(); break;
            default: break;
        }
    });
}

void MainView::showMessage (const juce::MessageBoxOptions& options)
{
    // scoped: the box closes with the editor instead of outliving it (plug-in windows can be closed any time)
    messageBox = juce::AlertWindow::showScopedAsync (options.withAssociatedComponent (this), {});
}

void MainView::savePresetAs()
{
    const auto folder = StateIO::userPresetFolder();
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save preset", folder.getChildFile (proc.getPresetName() + StateIO::kPresetExtension),
                                                   juce::String ("*") + StateIO::kPresetExtension);
    juce::Component::SafePointer<MainView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension (StateIO::kPresetExtension);
        juce::String err;
        if (! safe->proc.savePresetFile (f, err))
            safe->showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Save failed").withMessage (err).withButton ("OK"));
        else if (safe->proc.getCurrentState().isExpressive())
            safe->showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::InfoIcon)
                                              .withTitle ("Saved (Expressive)")
                                              .withMessage ("This preset uses the Expressive range policy: an artistic extension, not measured hardware behaviour. "
                                                            "The label is stored in the preset.")
                                              .withButton ("OK"));
        safe->refreshPresetBox();
    });
}

void MainView::openPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Open preset", StateIO::userPresetFolder(), juce::String ("*") + StateIO::kPresetExtension);
    juce::Component::SafePointer<MainView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        juce::String err;
        if (! safe->proc.loadPresetFile (f, err))
            safe->showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Could not open preset").withMessage (err).withButton ("OK"));
        safe->refreshPresetBox();
    });
}

void MainView::toggleRecord()
{
    auto& rec = proc.recorder();
    if (rec.isRecording())
    {
        const auto file = rec.getFile();
        const auto n = rec.stop();
        const double sr = std::max (1.0, proc.currentSampleRate());
        showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::InfoIcon)
                                          .withTitle ("Recording saved")
                                          .withMessage ("Saved " + file.getFileName() + "\n\nLocation: " + file.getParentDirectory().getFullPathName()
                                                        + "\nLength: " + juce::String ((double) n / sr, 2) + " s"
                                                        + "\nQuality: " + juce::String (sr / 1000.0, 1) + " kHz / 24-bit PCM / "
                                                        + (proc.currentChannels() == 1 ? "mono" : "stereo")
                                                        + (rec.samplesDropped() > 0 ? "\nWarning: " + juce::String ((int) rec.samplesDropped()) + " samples were dropped (disk too slow)." : juce::String()))
                                          .withButton ("OK"));
        return;
    }
    const auto dir = juce::File (proc.settings().getValue ("renderFolder", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getFullPathName()));
    const auto name = "Interference_Record_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".wav";
    chooser = std::make_unique<juce::FileChooser> ("Record to WAV", dir.getChildFile (name), "*.wav");
    juce::Component::SafePointer<MainView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("wav");
        juce::String err;
        if (! safe->proc.recorder().start (f, safe->proc.currentSampleRate(), safe->proc.currentChannels(), 24, true, err))
            safe->showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Cannot record").withMessage (err).withButton ("OK"));
        else
            safe->proc.settings().setValue ("renderFolder", f.getParentDirectory().getFullPathName());
    });
}

//==============================================================================
void MainView::setOverlay (std::unique_ptr<Overlay> o)
{
    overlay = std::move (o);
    if (overlay != nullptr)
    {
        overlay->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainView> (this)] { if (safe) safe->closeOverlay(); }); };
        addAndMakeVisible (*overlay);
        overlay->setBounds (getLocalBounds());
        overlay->grabKeyboardFocus();
    }
}

void MainView::closeOverlay()
{
    overlay.reset();
    grabKeyboardFocus();
}

void MainView::showHelp()
{
    auto h = std::make_unique<HelpOverlay>();
    h->onDebug = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainView> (this)] { if (safe) safe->showDebug(); }); };
    setOverlay (std::move (h));
}

void MainView::showOptions() { setOverlay (std::make_unique<OptionsOverlay> (proc)); }
void MainView::showRender()  { setOverlay (std::make_unique<RenderOverlay> (proc)); }
void MainView::showDebug()   { setOverlay (std::make_unique<DebugOverlay> (proc)); }
void MainView::showGhost()   { setOverlay (std::make_unique<GhostOverlay> (proc)); }

void MainView::mouseDown (const juce::MouseEvent& e)
{
    // easter egg: one exact pixel (one logical pixel of slack for high-DPI rounding)
    if (overlay == nullptr && std::abs (e.x - kSecretX) <= 1 && std::abs (e.y - kSecretY) <= 1)
        showGhost();
}

bool MainView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::F1Key) { showHelp(); return true; }
    if (k == juce::KeyPress::escapeKey && overlay != nullptr) { closeOverlay(); return true; }
    return false;
}

//==============================================================================
void MainView::timerCallback()
{
    const auto& t = proc.telemetry();
    led.setLevelDb (t.outPeakDb.load());

    const int dirty = proc.uiDirty.load();
    if (dirty != lastDirty)
    {
        lastDirty = dirty;
        rebuildIllustrations();
        palette.refresh();
        updateHeader();
        if (onTooltipsChanged) onTooltipsChanged (proc.tooltipsEnabled());
    }
    static int slow = 0;
    if (++slow % 8 == 0)
        updateHeader(); // modified marker

    const int mode = (int) rawValue (proc, P::mode);
    if (mode != lastMode)
    {
        if (lastMode >= 0)
            selectTab (mode == 1 ? 0 : mode == 2 ? 2 : 1);
        lastMode = mode;
        for (int i = 0; i < 3; ++i)
            modeButtons[i].setToggleState (i == mode, juce::dontSendNotification);
    }

    // processing order between the cards
    const bool swapped = rawValue (proc, P::swapOrder) > 0.5f;
    const juce::String orderText (juce::CharPointer_UTF8 (swapped ? "B \xe2\x86\x92 A" : "A \xe2\x86\x92 B"));
    if (orderToggle.getButtonText() != orderText)
        orderToggle.setButtonText (orderText);

    // realism spec 8: make it obvious when the internal excitation is part of the sound
    const bool excOn = t.excitationActive.load();
    if (excOn)
    {
        const auto excName = juce::StringArray::fromTokens (paramInfo (P::excType).choices, "|", "")[(int) rawValue (proc, P::excType)];
        juce::String badge = "Excitation on: " + excName + " " + juce::String (t.excitationEffectiveDb.load(), 0) + " dB";
        if (t.excitationLimited.load())
            badge << " (capped)";
        if (excBadge.getButtonText() != badge)
            excBadge.setButtonText (badge);
    }
    if (excBadge.isVisible() != excOn)
        excBadge.setVisible (excOn);

    for (int s = 0; s < 2; ++s)
    {
        const auto v = SlotView::read (t.slot[s]);
        cards[s].refresh (v, s == selectedSlot);
        if (illus[s] != nullptr) illus[s]->update (v);
        if (s == selectedSlot) context.refresh (v);
    }
    if (lane.isVisible()) lane.refresh();
    if (studio.isVisible()) studio.refresh();
    if (create.isVisible()) create.refresh();
    if (routing.isVisible()) routing.refresh();
    if (environment.isVisible()) environment.refresh();
    output.refresh();
    stream.addLines (proc.drainActivity());
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
    // header strip
    auto header = getLocalBounds().removeFromTop (Theme::headerHeight);
    g.setColour (Theme::panel);
    g.fillRect (header);
    g.setColour (Theme::edge);
    g.fillRect (header.removeFromBottom (1));
    // signature notch: 2 px accent diagonal, 12 px, top-left corner
    g.setColour (Theme::accent);
    g.drawLine (2.0f, 14.0f, 14.0f, 2.0f, 2.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::headerFont (14.0f));
    g.drawText ("Interference", juce::Rectangle<int> (44, 0, 120, Theme::headerHeight), juce::Justification::centredLeft);
    // gear icon
    g.setColour (gearButton.isMouseOver() ? Theme::text : Theme::textMuted);
    g.fillPath (gearPath (gearButton.getBounds().toFloat().reduced (6.0f)));

    // processing order: caption above the toggle between the slot cards
    if (! orderArea.isEmpty())
        Theme::drawLabel (g, "order", orderArea.withHeight (orderArea.getHeight() / 2 - 14).removeFromBottom (14),
                          juce::Justification::centred, Theme::textMuted, 9.0f);

    // section separators (1 px lines, never boxes in boxes)
    g.setColour (Theme::edge);
    g.fillRect (0, tabButtons[0].getY() - 6, getWidth(), 1);
    g.fillRect (0, modeButtons[0].getBottom() + 5, getWidth(), 1);
    // footer version (9 px muted mono)
    g.setFont (Theme::monoFont (9.0f));
    g.setColour (Theme::textMuted);
    g.drawText (juce::String ("v") + IFX_VERSION_STRING, getLocalBounds().removeFromBottom (12).removeFromRight (80).translated (-8, 0), juce::Justification::centredRight);
}

void MainView::layoutStream()
{
    const auto free = context.getFreeArea();
    if (free.getHeight() >= 80 && free.getWidth() > 160)
    {
        stream.setBounds (free.translated (context.getX(), context.getY()).reduced (4));
        stream.numVisibleLines = 10;
        stream.setVisible (true);
    }
    else
        stream.setVisible (false);
}

void MainView::resized()
{
    auto r = getLocalBounds();
    const int W = r.getWidth();

    // header (32 px)
    auto h = r.removeFromTop (Theme::headerHeight);
    led.setBounds (juce::Rectangle<int> (18, 9, 14, 14));
    h.removeFromLeft (170);
    menuButton.setBounds (h.removeFromLeft (64).reduced (2, 3));
    gearButton.setBounds (h.removeFromRight (32).reduced (2));
    helpButton.setBounds (h.removeFromRight (30).reduced (2, 3));
    h.removeFromRight (6);
    randomButton.setBounds (h.removeFromRight (92).reduced (2, 3));
    resetButton.setBounds (h.removeFromRight (64).reduced (2, 3));
    h.removeFromRight (6);
    copyAB.setBounds (h.removeFromRight (44).reduced (2, 3));
    bButton.setBounds (h.removeFromRight (30).reduced (2, 3));
    aButton.setBounds (h.removeFromRight (30).reduced (2, 3));
    h.removeFromRight (8);
    nextPreset.setBounds (h.removeFromRight (26).reduced (2, 3));
    auto pb = h.removeFromRight (std::min (300, h.getWidth() - 30));
    presetBox.setBounds (pb.reduced (2, 3));
    prevPreset.setBounds (h.removeFromRight (26).reduced (2, 3));

    r.reduce (16, 0);
    r.removeFromTop (8);
    // mode + source row
    auto row = r.removeFromTop (28);
    for (auto& b : modeButtons)
        b.setBounds (row.removeFromLeft (84).reduced (1, 0));
    row.removeFromLeft (12);
    auto right = row.removeFromRight (std::min (420, W / 3));
    policy.setBounds (right.removeFromRight (120).reduced (2, 0));
    takeButton.setBounds (right.removeFromRight (82).reduced (2, 0));
    lockButton.setBounds (right.removeFromRight (52).reduced (2, 0));
    seedLabel.setBounds (right.removeFromRight (150).reduced (2, 1));
    palette.setBounds (row);
    r.removeFromTop (12);

    // bottom area
    auto bottom = r.removeFromBottom (juce::jlimit (240, 320, getHeight() * 36 / 100));
    r.removeFromBottom (8);
    auto tabs = bottom.removeFromTop (26);
    auto outArea = bottom.removeFromRight (250);
    tabs.removeFromRight (250);
    for (auto& t : tabButtons)
        t.setBounds (tabs.removeFromLeft (104).reduced (1, 0));
    excBadge.setBounds (tabs.removeFromRight (std::min (230, std::max (0, tabs.getWidth() - 8))).reduced (1, 0));
    bottom.removeFromTop (6);
    bottom.removeFromBottom (12);
    for (auto* c : std::initializer_list<juce::Component*> { &lane, &studio, &create, &routing, &environment })
        c->setBounds (bottom.withTrimmedRight (12));
    output.setBounds (outArea.withTrimmedTop (-26).withTrimmedBottom (12));

    // middle: two slot columns + context panel
    auto ctx = r.removeFromRight (juce::jlimit (320, 420, W * 30 / 100));
    context.setBounds (ctx.withTrimmedLeft (12));
    // the processing-order toggle sits in a narrow column between the two slot cards
    constexpr int orderW = 64;
    const int colW = (r.getWidth() - orderW) / 2;
    for (int s = 0; s < 2; ++s)
    {
        auto col = r.removeFromLeft (colW);
        cards[s].setBounds (col.removeFromTop (118));
        col.removeFromTop (6);
        if (illus[s] != nullptr) illus[s]->setBounds (col);
        if (s == 0)
            orderArea = r.removeFromLeft (orderW).withHeight (118);
    }
    orderToggle.setBounds (orderArea.withSizeKeepingCentre (orderW - 8, 26));
    layoutStream();
    if (overlay != nullptr)
        overlay->setBounds (getLocalBounds());
}

} // namespace ifx
