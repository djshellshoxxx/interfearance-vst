#include "Overlays.h"
#include "Plugin/Diagnostics.h"
#include <thread>
#if JucePlugin_Build_Standalone && ! IFX_HEADLESS
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace ifx
{

//==============================================================================
Overlay::Overlay (const juce::String& t) : title (t)
{
    setWantsKeyboardFocus (true);
    closeButton.onClick = [this] { close(); };
    closeButton.setTooltip ("Close (Esc)");
    addAndMakeVisible (closeButton);
}

juce::Rectangle<int> Overlay::panelBounds() const
{
    return getLocalBounds().reduced (std::max (16, getWidth() / 12), std::max (16, getHeight() / 14));
}

void Overlay::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background.withAlpha (0.82f));
    const auto p = panelBounds().toFloat();
    Theme::drawShadow (g, p, 6.0f);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (p, 6.0f);
    g.setColour (Theme::edge);
    g.drawRoundedRectangle (p.reduced (0.5f), 6.0f, 1.0f);
    Theme::drawSectionHeader (g, title, panelBounds().reduced (16, 12).removeFromTop (24));
}

void Overlay::resized()
{
    auto p = panelBounds().reduced (16, 12);
    auto head = p.removeFromTop (24);
    closeButton.setBounds (head.removeFromRight (80));
    p.removeFromTop (8);
    layoutContent (p);
}

bool Overlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}

void Overlay::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()))
        close(); // clicking outside dismisses
}

void Overlay::close()
{
    if (onClose) onClose();
}

void Overlay::showMessage (const juce::MessageBoxOptions& options, std::function<void (int)> callback)
{
    messageBox = juce::AlertWindow::showScopedAsync (options.withAssociatedComponent (this), std::move (callback));
}

//==============================================================================
HelpOverlay::HelpOverlay() : Overlay ("Help")
{
    const auto& secs = HelpContent::sections();
    for (int i = 0; i < secs.size(); ++i)
    {
        auto* b = tabs.add (new juce::TextButton (secs[i].title));
        b->setRadioGroupId (0x4E1);
        b->setClickingTogglesState (true);
        b->onClick = [this, i] { showSection (i); };
        addAndMakeVisible (b);
    }
    text.setMultiLine (true);
    text.setReadOnly (true);
    text.setScrollbarsShown (true);
    text.setCaretVisible (false);
    text.setFont (Theme::bodyFont (14.0f));
    text.setColour (juce::TextEditor::backgroundColourId, Theme::background);
    text.setColour (juce::TextEditor::outlineColourId, Theme::edge);
    addAndMakeVisible (text);
    debug.setTooltip ("Open the debug tools: live data window, crash log, troubleshooting export and hard reset.");
    debug.onClick = [this] { if (onDebug) onDebug(); };
    addAndMakeVisible (debug);
    home.setButtonText (juce::String ("Homepage: ") + Branding::homepage);
    home.setURL (juce::URL (Branding::homepage));
    mail.setButtonText (juce::String ("Support: ") + Branding::supportEmail);
    mail.setURL (juce::URL (juce::String ("mailto:") + Branding::supportEmail));
    for (auto* h : { &home, &mail })
    {
        h->setFont (Theme::bodyFont (12.0f), false, juce::Justification::centredLeft);
        h->setColour (juce::HyperlinkButton::textColourId, Theme::accent2);
        addAndMakeVisible (h);
    }
    showSection (0);
}

void HelpOverlay::showSection (int index)
{
    const auto& secs = HelpContent::sections();
    if (! juce::isPositiveAndBelow (index, secs.size())) return;
    tabs[index]->setToggleState (true, juce::dontSendNotification);
    text.setText (secs[index].body, false);
    text.moveCaretToTop (false);
}

void HelpOverlay::layoutContent (juce::Rectangle<int> r)
{
    auto bottom = r.removeFromBottom (28);
    debug.setBounds (bottom.removeFromRight (110));
    home.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2));
    mail.setBounds (bottom);
    r.removeFromBottom (8);
    auto side = r.removeFromLeft (220);
    const int h = std::max (20, std::min (28, side.getHeight() / std::max (1, tabs.size())));
    for (auto* b : tabs)
        b->setBounds (side.removeFromTop (h).reduced (0, 1));
    r.removeFromLeft (12);
    text.setBounds (r);
}

//==============================================================================
OptionsOverlay::OptionsOverlay (InterferenceProcessor& p) : Overlay ("Options"), proc (p)
{
    tooltips.setToggleState (proc.tooltipsEnabled(), juce::dontSendNotification);
    tooltips.onClick = [this] { proc.setTooltipsEnabled (tooltips.getToggleState()); };
    addAndMakeVisible (tooltips);

    const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    audioSettings.setEnabled (standalone);
    audioSettings.onClick = []
    {
#if JucePlugin_Build_Standalone && ! IFX_HEADLESS
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->showAudioSettingsDialog();
#endif
    };
    addAndMakeVisible (audioSettings);
    deviceNote.setText (standalone ? "Choose the audio interface, sample rate, buffer size and MIDI inputs of the stand-alone app."
                                   : "Inside a DAW the host owns the audio and MIDI devices: set them in your DAW. Route MIDI to this track to play gestures.",
                        juce::dontSendNotification);
    deviceNote.setColour (juce::Label::textColourId, Theme::textMuted);
    deviceNote.setFont (Theme::bodyFont (12.0f));
    addAndMakeVisible (deviceNote);

    openPresets.onClick = []
    {
        const auto f = StateIO::userPresetFolder();
        f.createDirectory();
        f.revealToUser();
    };
    openSettings.onClick = [this] { proc.settings().getFile().getParentDirectory().revealToUser(); };
    clearAll.onClick = [this]
    {
        for (int i = 0; i < kNumParams; ++i)
            if (proc.describeMidiMapping (i).isNotEmpty())
                proc.clearMidiMapping (i);
    };
    for (auto* b : { &openPresets, &openSettings, &clearAll })
        addAndMakeVisible (b);
    mapTitle.setText ("MIDI mappings (right-click any control > MIDI learn):", juce::dontSendNotification);
    mapTitle.setColour (juce::Label::textColourId, Theme::text);
    addAndMakeVisible (mapTitle);
    mappings.setMultiLine (true);
    mappings.setReadOnly (true);
    mappings.setFont (Theme::monoFont (12.0f));
    addAndMakeVisible (mappings);
    rebuildMappings();
    startTimerHz (4);
}

OptionsOverlay::~OptionsOverlay() { stopTimer(); }

void OptionsOverlay::timerCallback()
{
    if (proc.getMidiMap().size() != lastMapCount)
        rebuildMappings();
}

void OptionsOverlay::rebuildMappings()
{
    lastMapCount = proc.getMidiMap().size();
    juce::String s;
    for (const auto& m : proc.getMidiMap())
        s << "CC " << juce::String (m.cc).paddedLeft (' ', 3) << (m.channel > 0 ? "  ch " + juce::String (m.channel) : juce::String ("  any ch")) << "  ->  " << paramInfo (m.param).name << "\n";
    if (s.isEmpty())
        s = "(none)";
    mappings.setText (s, false);
}

void OptionsOverlay::layoutContent (juce::Rectangle<int> r)
{
    tooltips.setBounds (r.removeFromTop (28).removeFromLeft (220));
    r.removeFromTop (8);
    auto row = r.removeFromTop (28);
    audioSettings.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (12);
    deviceNote.setBounds (row);
    r.removeFromTop (8);
    row = r.removeFromTop (28);
    openPresets.setBounds (row.removeFromLeft (240));
    row.removeFromLeft (8);
    openSettings.setBounds (row.removeFromLeft (180));
    row.removeFromLeft (8);
    clearAll.setBounds (row.removeFromLeft (180));
    r.removeFromTop (12);
    mapTitle.setBounds (r.removeFromTop (22));
    mappings.setBounds (r);
}

//==============================================================================
DebugOverlay::DebugOverlay (InterferenceProcessor& p) : Overlay ("Debug"), proc (p)
{
    for (auto* t : { &live, &events })
    {
        t->setMultiLine (true);
        t->setReadOnly (true);
        t->setScrollbarsShown (true);
        t->setCaretVisible (false);
        t->setFont (Theme::monoFont (11.0f));
        t->setColour (juce::TextEditor::backgroundColourId, Theme::background);
        t->setColour (juce::TextEditor::outlineColourId, Theme::edge);
        t->setColour (juce::TextEditor::textColourId, Theme::text);
        addAndMakeVisible (t);
    }
    live.setTitle ("Live data");
    events.setTitle ("Recent events");
    freeze.setTooltip ("Pause the live data so it can be scrolled and read.");
    addAndMakeVisible (freeze);

    crashLog.setTooltip ("Write a time-stamped log to Documents\\Interference\\Support. It starts with a troubleshooting file, "
                         "records events while you work and, if the plug-in or host crashes, the error and a stack trace. "
                         "Some crashes cannot be logged: fast-fail aborts (security checks, heap corruption detected by "
                         "Windows) and hosts that catch plug-in crashes themselves. Off every time the plug-in loads.");
    crashLog.setToggleState (proc.isCrashLogging(), juce::dontSendNotification);
    crashLog.onClick = [this] { setCrashLog (crashLog.getToggleState()); };
    addAndMakeVisible (crashLog);

    exportButton.setTooltip ("Run a quick self-test and save it with every setting, audio / MIDI setup, version, licence and host "
                             "information to a .txt file in Documents\\Interference\\Support. The self-test renders one second "
                             "offline (the window pauses briefly) and checks that the preset folder is writable, creating it if "
                             "it is missing. The file contains folder paths, which include your user name.");
    exportButton.onClick = [this] { exportTroubleshooting (true); };
    openFolder.setTooltip ("Show the folder that holds crash logs and troubleshooting files.");
    openFolder.onClick = []
    {
        const auto f = Diagnostics::supportFolder();
        f.createDirectory();
        f.revealToUser();
    };
    hardReset.setTooltip ("Troubleshooting only: resets every setting, clears MIDI mappings, A/B and options, and deletes the "
                          "options (cache) file. Your preset and audio files are kept.");
    hardReset.onClick = [this] { confirmHardReset(); };
    for (auto* b : { &exportButton, &openFolder, &hardReset })
        addAndMakeVisible (b);

    for (auto* l : { &crashStatus, &info })
    {
        l->setFont (Theme::bodyFont (12.0f));
        l->setColour (juce::Label::textColourId, Theme::textMuted);
        l->setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (l);
    }
    info.setText (juce::String ("Troubleshooting file: a quick self-test plus every setting, for minor problems. "
                                "Crash log: for crashes; it also contains a troubleshooting file.\n"
                                "Both are saved in Documents\\Interference\\Support. If the plug-in hard-crashes, tick "
                                "'Create log file on crash', reproduce the problem, then send the newest .log and a troubleshooting "
                                "file to ") + Branding::supportEmail + " with a detailed description of what you did.\n"
                                "Some crashes cannot be logged (fast-fail aborts, or a host that catches plug-in crashes itself): "
                                "then the log simply ends without a crash report.",
                  juce::dontSendNotification);
    refreshCrashStatus();
    cursor = 0;
    recent = proc.activitySince (cursor);
    timerCallback();
    startTimerHz (2);
}

DebugOverlay::~DebugOverlay()
{
    stopTimer();
    if (confirmBox != nullptr && confirmBox->isCurrentlyModal (false))
        confirmBox->exitModalState (0); // the modal manager deletes it; the callback sees a dead overlay
}

void DebugOverlay::timerCallback()
{
    const auto fresh = proc.activitySince (cursor);
    if (! fresh.isEmpty() || events.isEmpty())
    {
        recent.addArray (fresh);
        if (recent.size() > 200)
            recent.removeRange (0, recent.size() - 200);
        events.setText (recent.isEmpty() ? juce::String ("(no events yet: move a control or play a gesture)") : recent.joinIntoString ("\n"), false);
        events.moveCaretToEnd();
    }
    if (! freeze.getToggleState())
    {
        // setText re-lays out the whole editor and resets its scroll position: only when it changed
        auto text = Diagnostics::liveData (proc);
        if (text != lastLive)
        {
            live.setText (text, false);
            lastLive = std::move (text);
        }
    }
    // another instance may have taken the (one per process) crash log over
    refreshCrashStatus();
}

void DebugOverlay::confirmHardReset()
{
    if (confirmBox != nullptr)
        return;
    auto* aw = new juce::AlertWindow ("Hard reset",
                                      "Reset ALL settings to default, clear MIDI mappings, A/B and options, and delete the "
                                      "options file?\n\nPresets and rendered audio are not touched. This cannot be undone.",
                                      juce::MessageBoxIconType::WarningIcon, this);
    // Cancel is the default: Return and Escape both cancel, only a click on "Hard reset" resets
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey), juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Hard reset", 1);
    confirmBox = aw;
    juce::Component::SafePointer<DebugOverlay> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe != nullptr && result == 1)
            safe->performHardReset();
    }), true);
}

void DebugOverlay::refreshCrashStatus()
{
    const auto f = CrashLog::currentFile();
    crashStatus.setText (proc.isCrashLogging() && f != juce::File() ? "Logging to " + f.getFullPathName()
                                                                    : juce::String ("Crash log off."),
                         juce::dontSendNotification);
    crashLog.setToggleState (proc.isCrashLogging(), juce::dontSendNotification);
}

void DebugOverlay::setCrashLog (bool on)
{
    if (on && ! proc.isCrashLogging())
    {
        juce::File f;
        juce::String err;
        if (! proc.startCrashLog (f, err))
            showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                             .withTitle ("Cannot create crash log").withMessage (err).withButton ("OK"));
    }
    else if (! on)
        proc.stopCrashLog();
    refreshCrashStatus();
}

bool DebugOverlay::exportTroubleshooting (bool showPopup)
{
    juce::File f;
    juce::String err;
    const bool ok = Diagnostics::writeTroubleshootingFile (proc, f, err);
    if (showPopup)
        showMessage (juce::MessageBoxOptions()
                         .withIconType (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon)
                         .withTitle (ok ? "Troubleshooting file saved" : "Could not save the troubleshooting file")
                         .withMessage (ok ? "Saved " + f.getFileName() + "\n\nLocation: " + f.getParentDirectory().getFullPathName()
                                            + "\n\nSend it to " + Branding::supportEmail + " with a description of the problem."
                                          : err)
                         .withButton ("OK"));
    return ok;
}

void DebugOverlay::performHardReset()
{
    proc.hardReset();
    refreshCrashStatus();
}

void DebugOverlay::layoutContent (juce::Rectangle<int> r)
{
    auto side = r.removeFromRight (juce::jlimit (260, 360, r.getWidth() / 3));
    r.removeFromRight (12);
    auto top = r.removeFromTop (24);
    freeze.setBounds (top.removeFromRight (90));
    r.removeFromTop (4);
    events.setBounds (r.removeFromBottom (juce::jmax (80, r.getHeight() / 4)));
    r.removeFromBottom (8);
    live.setBounds (r);

    crashLog.setBounds (side.removeFromTop (26));
    crashStatus.setBounds (side.removeFromTop (48));
    side.removeFromTop (6);
    for (auto* b : { &exportButton, &openFolder })
    {
        b->setBounds (side.removeFromTop (28));
        side.removeFromTop (6);
    }
    hardReset.setBounds (side.removeFromBottom (28));
    side.removeFromBottom (8);
    info.setBounds (side);
}

//==============================================================================
GhostOverlay::GhostOverlay (InterferenceProcessor& p)
    : Overlay ("Secret: Ghost Carrier"),
      enable (p, P::ghostOn, "Ghost Carrier On"),
      freq (p, P::ghostFreq), drift (p, P::ghostDrift), amount (p, P::ghostMix)
{
    closeButton.setTooltip ("Close the secret panel (Esc). The hidden effect keeps its settings.");
    blurb.setText ("You found a secret hidden effect. A stray radio carrier leaks into the wet signal and ring-modulates it: "
                   "low frequencies give tremolo-like beating, high ones metallic sidebands. It is an artistic Studio-style "
                   "effect, not part of the hardware model, and is saved with projects and presets like any other setting. "
                   "Bypass and Dry/Wet 0 % still give the untouched dry signal.",
                   juce::dontSendNotification);
    blurb.setFont (Theme::bodyFont (13.0f));
    blurb.setColour (juce::Label::textColourId, Theme::text);
    blurb.setJustificationType (juce::Justification::topLeft);
    blurb.setTooltip ("Secret hidden effect.");
    for (auto* c : std::initializer_list<juce::Component*> { &enable, &freq, &drift, &amount, &blurb })
        addAndMakeVisible (c);
}

void GhostOverlay::layoutContent (juce::Rectangle<int> r)
{
    blurb.setBounds (r.removeFromTop (72));
    r.removeFromTop (8);
    enable.setBounds (r.removeFromTop (28).removeFromLeft (180));
    r.removeFromTop (12);
    auto row = r.removeFromTop (juce::jmin (140, r.getHeight()));
    const int w = juce::jmin (140, row.getWidth() / 3);
    for (auto* k : { &freq, &drift, &amount })
        k->setBounds (row.removeFromLeft (w).reduced (6, 0));
}

//==============================================================================
RenderOverlay::RenderOverlay (InterferenceProcessor& p) : Overlay ("Render WAV (offline, cloned engine)"), proc (p)
{
    auto caption = [this] (const juce::String& t, juce::Component& c)
    {
        auto* l = captions.add (new juce::Label ({}, t));
        l->setFont (Theme::labelFont (11.0f));
        l->setColour (juce::Label::textColourId, Theme::textMuted);
        l->attachToComponent (&c, true);
        addAndMakeVisible (l);
        addAndMakeVisible (c);
    };
    lengthMode.addItemList ({ "Bars", "Seconds" }, 1);
    lengthMode.setSelectedId (1);
    for (int b = 1; b <= 16; ++b) bars.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);
    bars.setSelectedId (2);
    rate.addItemList ({ "44.1 kHz", "48 kHz", "96 kHz" }, 1);
    rate.setSelectedId (proc.currentSampleRate() == 44100.0 ? 1 : proc.currentSampleRate() == 96000.0 ? 3 : 2);
    depth.addItemList ({ "16-bit PCM", "24-bit PCM", "32-bit float" }, 1);
    depth.setSelectedId (2);
    channels.addItemList ({ "Mono", "Stereo" }, 1);
    channels.setSelectedId (2);
    tap.addItemList ({ "Full Output y", "Full Core C", "Injected Artifacts A", "Fault Difference D", "Studio Only P", "Full Wet W" }, 1);
    tap.setSelectedId (1);
    auto setupSlider = [] (juce::Slider& s, double lo, double hi, double step, double v, const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
        s.setRange (lo, hi, step);
        s.setValue (v, juce::dontSendNotification);
        s.setTextValueSuffix (suffix);
    };
    const double hostBpm = proc.telemetry().bpm.load();
    setupSlider (seconds, 0.1, 30.0, 0.1, 2.0, " s");
    setupSlider (bpm, 40.0, 240.0, 0.1, hostBpm > 1.0 ? hostBpm : 120.0, " BPM");
    setupSlider (tail, 0.0, 10.0, 0.1, 0.0, " s");
    setupSlider (headroom, 0.0, 12.0, 0.1, 1.0, " dB");
    caption ("Length", lengthMode);
    caption ("Bars", bars);
    caption ("Seconds", seconds);
    caption ("Tempo", bpm);
    caption ("Tail", tail);
    caption ("Sample rate", rate);
    caption ("Format", depth);
    caption ("Channels", channels);
    caption ("Tap", tap);
    caption ("Headroom", headroom);
    addAndMakeVisible (normalize);
    addAndMakeVisible (sidecar);
    addAndMakeVisible (go);
    addChildComponent (cancel);
    go.onClick = [this] { startRender(); };
    cancel.onClick = [this] { cancelFlag.store (true); };
    tail.setTooltip ("Extra time after the end with no new gestures, so Echo / Resonator tails can ring out.");
    tap.setTooltip ("Which bus to render. Fault Difference is signed (not rectified).");
    status = "Renders the internal excitation / gesture path (no host audio) with the current sound, seed and lane.";
}

RenderOverlay::~RenderOverlay()
{
    stopTimer();
    cancelFlag.store (true);
    if (worker != nullptr && worker->joinable())
        worker->join(); // teardown while the exporter is active: cancel and wait
}

RenderSettings RenderOverlay::gather() const
{
    RenderSettings s;
    s.useBars = lengthMode.getSelectedId() == 1;
    s.bars = bars.getSelectedId();
    s.seconds = seconds.getValue();
    s.bpm = bpm.getValue();
    s.tailSeconds = tail.getValue();
    s.sampleRate = rate.getSelectedId() == 1 ? 44100.0 : rate.getSelectedId() == 3 ? 96000.0 : 48000.0;
    s.bitDepth = depth.getSelectedId() == 1 ? 16 : depth.getSelectedId() == 3 ? 32 : 24;
    s.channels = channels.getSelectedId() == 1 ? 1 : 2;
    s.tap = (Tap) (tap.getSelectedId() - 1);
    s.normalize = normalize.getToggleState();
    s.headroomDb = (float) headroom.getValue();
    s.writeSidecar = sidecar.getToggleState();
    s.sanitise();
    return s;
}

void RenderOverlay::startRender()
{
    if (worker != nullptr) return;
    settings = gather();
    snapshot = proc.getCurrentState();
    cancelFlag.store (false);
    done.store (false);
    progress.store (0.0f);
    go.setEnabled (false);
    cancel.setVisible (true);
    status = "Rendering...";
    worker = std::make_unique<std::thread> ([this]
    {
        result = OfflineRenderer::render (snapshot, settings, &cancelFlag, &progress);
        done.store (true);
    });
    startTimerHz (20);
}

void RenderOverlay::timerCallback()
{
    if (done.load())
    {
        stopTimer();
        worker->join();
        worker.reset();
        finished();
    }
    repaint();
}

void RenderOverlay::finished()
{
    go.setEnabled (true);
    cancel.setVisible (false);
    if (result.cancelled) { status = "Render cancelled."; repaint(); return; }
    if (! result.ok) { status = "Render failed: " + result.error; repaint(); return; }
    if (result.silent)
    {
        juce::Component::SafePointer<RenderOverlay> safe (this);
        showMessage (juce::MessageBoxOptions()
                         .withIconType (juce::MessageBoxIconType::WarningIcon)
                         .withTitle ("The render is silent")
                         .withMessage (result.silentReason + "\n\nExport the silent file anyway?")
                         .withButton ("Export anyway")
                         .withButton ("Cancel"),
                     [safe] (int r) { if (safe != nullptr && r == 1) safe->chooseFileAndWrite(); });
        status = "The result is silent (see the explanation).";
        repaint();
        return;
    }
    chooseFileAndWrite();
}

void RenderOverlay::chooseFileAndWrite()
{
    const auto lastDir = juce::File (proc.settings().getValue ("renderFolder",
                                        juce::File::getSpecialLocation (juce::File::userMusicDirectory).getFullPathName()));
    const auto name = OfflineRenderer::suggestedFileName (snapshot, settings);
    chooser = std::make_unique<juce::FileChooser> ("Save rendered WAV", lastDir.getChildFile (name), "*.wav");
    juce::Component::SafePointer<RenderOverlay> safe (this);
    // warnAboutOverwriting: the chooser asks before replacing an existing file
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto f = fc.getResult();
        if (f == juce::File()) { safe->status = "Export cancelled (nothing written)."; safe->repaint(); return; }
        if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
        juce::String err;
        // the user confirmed the chooser's overwrite prompt (or the file is new)
        if (! OfflineRenderer::writeWav (safe->result.audio, safe->settings, f, true, err))
        {
            safe->status = "Export failed: " + err;
            safe->showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                   .withTitle ("Export failed").withMessage (err).withButton ("OK"));
            safe->repaint();
            return;
        }
        if (safe->settings.writeSidecar)
            OfflineRenderer::writeSidecar (safe->snapshot, safe->settings, f, err);
        safe->proc.settings().setValue ("renderFolder", f.getParentDirectory().getFullPathName());
        const juce::String msg = "Saved " + f.getFileName() + "\n\nLocation: " + f.getParentDirectory().getFullPathName()
                               + "\nLength: " + juce::String (safe->result.seconds, 2) + " s" + (safe->result.tailIncluded ? " (including tail)" : " (no tail)")
                               + "\nQuality: " + OfflineRenderer::describeFormat (safe->settings)
                               + "\nPeak: " + juce::String (juce::Decibels::gainToDecibels (safe->result.peak), 1) + " dBFS"
                               + (safe->settings.normalize ? " (normalized " + juce::String (safe->result.appliedGainDb, 1) + " dB)" : juce::String())
                               + "\nPolicy: " + (safe->snapshot.isExpressive() ? "Expressive" : "Natural")
                               + (safe->snapshot.usesStudio() ? ", Studio-processed" : "") + (safe->snapshot.usesExcitation() ? ", internal excitation" : "");
        safe->status = "Saved " + f.getFullPathName();
        safe->showMessage (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::InfoIcon)
                               .withTitle ("Render saved").withMessage (msg).withButton ("OK"));
        safe->repaint();
    });
}

void RenderOverlay::paint (juce::Graphics& g)
{
    Overlay::paint (g);
    auto r = panelBounds().reduced (16, 12);
    auto bar = r.removeFromBottom (58).removeFromTop (22);
    g.setFont (Theme::bodyFont (12.5f));
    g.setColour (Theme::textMuted);
    g.drawFittedText (status, bar, juce::Justification::centredLeft, 2);
    if (worker != nullptr)
    {
        auto pb = r.removeFromBottom (10).toFloat();
        g.setColour (Theme::edge);
        g.fillRoundedRectangle (pb, 3.0f);
        g.setColour (Theme::accent);
        g.fillRoundedRectangle (pb.withWidth (pb.getWidth() * progress.load()), 3.0f);
    }
}

void RenderOverlay::layoutContent (juce::Rectangle<int> r)
{
    auto buttons = r.removeFromBottom (30);
    go.setBounds (buttons.removeFromRight (120));
    buttons.removeFromRight (8);
    cancel.setBounds (buttons.removeFromRight (130));
    r.removeFromBottom (40);
    auto col1 = r.removeFromLeft (r.getWidth() / 2).withTrimmedLeft (90);
    auto col2 = r.withTrimmedLeft (90);
    auto place = [] (juce::Rectangle<int>& col, juce::Component& c) { c.setBounds (col.removeFromTop (28).withTrimmedRight (16)); col.removeFromTop (8); };
    place (col1, lengthMode);
    place (col1, bars);
    place (col1, seconds);
    place (col1, bpm);
    place (col1, tail);
    place (col2, rate);
    place (col2, depth);
    place (col2, channels);
    place (col2, tap);
    place (col2, headroom);
    normalize.setBounds (col1.removeFromTop (26).withX (col1.getX() - 90).withWidth (340));
    sidecar.setBounds (col2.removeFromTop (26).withX (col2.getX() - 90).withWidth (360));
}

} // namespace ifx
