#include "Overlays.h"
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
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::WarningIcon)
                                          .withTitle ("The render is silent")
                                          .withMessage (result.silentReason + "\n\nExport the silent file anyway?")
                                          .withButton ("Export anyway")
                                          .withButton ("Cancel")
                                          .withAssociatedComponent (this),
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
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Export failed").withMessage (err).withButton ("OK"), nullptr);
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
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::InfoIcon)
                                          .withTitle ("Render saved").withMessage (msg).withButton ("OK"), nullptr);
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
