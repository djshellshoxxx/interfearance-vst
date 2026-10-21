// Host-contract and GUI tests: builds the real processor + editor headlessly
// (master spec 10/11: buses, state, MIDI, concurrent GUI actions, resizing, teardown).
#include "TestFramework.h"
#include "Plugin/PluginProcessor.h"
#include "Plugin/PluginEditor.h"
#include "Plugin/Diagnostics.h"
#include <thread>

using namespace ifx;
using namespace ifxtest;

namespace
{
/** Every test in this binary works on a throw-away data root: presets, support files (crash
    logs, troubleshooting files) and the options file never touch the user's real folders.
    Set before main() runs (so before any processor exists), restored and deleted at exit. */
struct TempDataRoot
{
    const juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("InterferenceGuiTests_" + juce::String (juce::Time::currentTimeMillis()));
    TempDataRoot()
    {
        dir.createDirectory();
        StateIO::setDataRootOverride (dir);
    }
    ~TempDataRoot()
    {
        StateIO::setDataRootOverride ({});
        dir.deleteRecursively();
    }
    TempDataRoot (const TempDataRoot&) = delete;
    TempDataRoot& operator= (const TempDataRoot&) = delete;
};
const TempDataRoot tempDataRoot;

juce::File snapshotDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getChildFile ("gui-snapshots");
    d.createDirectory();
    return d;
}

void pump (int ms = 60)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
}

/** Pump the message loop until `done()` holds or `timeoutMs` passes (timer cadence varies by platform and load). */
template <typename Fn>
bool pumpUntil (Fn&& done, int timeoutMs = 3000)
{
    for (int waited = 0; waited < timeoutMs && ! done(); waited += 20)
        pump (20);
    return done();
}

void saveSnapshot (juce::Component& c, const juce::String& name)
{
    const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png;
    auto f = snapshotDir().getChildFile (name + ".png");
    f.deleteFile();
    if (auto os = f.createOutputStream())
        png.writeImageToStream (img, *os);
}

struct Rig
{
    std::unique_ptr<InterferenceProcessor> proc = std::make_unique<InterferenceProcessor>();
    Rig (double sr = 48000.0, int block = 512)
    {
        proc->setPlayConfigDetails (2, 2, sr, block);
        proc->prepareToPlay (sr, block);
    }
    juce::AudioBuffer<float> process (int n, float amp = 0.3f)
    {
        juce::AudioBuffer<float> b (proc->getTotalNumInputChannels(), n);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < n; ++i)
                b.setSample (ch, i, amp * (float) std::sin (0.05 * i));
        juce::MidiBuffer m;
        proc->processBlock (b, m);
        return b;
    }
};

/** A host transport for tests: playing from a given position, advanced by the caller. */
struct TestPlayHead final : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0, seconds = 0.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        p.setTimeInSeconds (seconds);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
    void advance (int n, double sr) { seconds += n / sr; ppq += n / sr * bpm / 60.0; }
};

/** Plays `blocks` blocks of a fixed test signal from ppq 0 through the processor, as a host
    would, and returns the output. */
juce::AudioBuffer<float> renderFromStart (InterferenceProcessor& p, int blocks, int block, double sr)
{
    TestPlayHead ph;
    p.setPlayHead (&ph);
    juce::AudioBuffer<float> all (2, blocks * block);
    for (int b = 0; b < blocks; ++b)
    {
        juce::AudioBuffer<float> buf (std::max (2, p.getTotalNumInputChannels()), block);
        buf.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                buf.setSample (ch, i, 0.3f * (float) std::sin (0.05 * (b * block + i) + 0.3 * ch));
        juce::MidiBuffer m;
        p.processBlock (buf, m);
        for (int ch = 0; ch < 2; ++ch)
            all.copyFrom (ch, b * block, buf, ch, 0, block);
        ph.advance (block, sr);
    }
    p.setPlayHead (nullptr);
    return all;
}

bool allChildrenInside (juce::Component& c, juce::StringArray& bad)
{
    bool ok = true;
    // a Viewport's content is meant to be larger than its visible area (it scrolls)
    const bool scrolledContent = dynamic_cast<juce::Viewport*> (c.getParentComponent()) != nullptr;
    for (auto* ch : c.getChildren())
    {
        if (! ch->isVisible() || ch->getWidth() <= 0) continue;
        if (! scrolledContent && ! c.getLocalBounds().contains (ch->getBounds()) && dynamic_cast<Overlay*> (ch) == nullptr)
        {
            ok = false;
            bad.add (ch->getName() + " " + juce::String (typeid (*ch).name()) + " " + ch->getBounds().toString());
        }
        ok &= allChildrenInside (*ch, bad);
    }
    return ok;
}

template <typename T>
T* findDescendant (juce::Component& c)
{
    for (auto* ch : c.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (ch))
            return t;
        if (auto* t = findDescendant<T> (*ch))
            return t;
    }
    return nullptr;
}
}

IFX_TEST (Host, busLayoutsMonoStereoOnly)
{
    InterferenceProcessor p;
    auto layout = [] (juce::AudioChannelSet in, juce::AudioChannelSet out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.inputBuses.add (juce::AudioChannelSet::disabled());
        l.outputBuses.add (out);
        return l;
    };
    IFX_CHECK (p.checkBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())));
    IFX_CHECK (p.checkBusesLayoutSupported (layout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())));
    IFX_CHECK (! p.checkBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())));
    IFX_CHECK (! p.checkBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())));
    auto withSc = layout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo());
    withSc.inputBuses.set (1, juce::AudioChannelSet::stereo());
    IFX_CHECK (p.checkBusesLayoutSupported (withSc));
    IFX_CHECK (p.getLatencySamples() == 0);
}

IFX_TEST (Host, parameterIdsAreUniqueAndStable)
{
    InterferenceProcessor p;
    juce::StringArray ids;
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            ids.add (r->getParameterID());
    IFX_CHECK (ids.size() == kNumParams);
    juce::StringArray unique (ids);
    unique.removeDuplicates (false);
    IFX_CHECK (unique.size() == ids.size());
    // a few shipped IDs that must never change
    for (auto* id : { "jk_insert", "tc_touch", "cb_bend", "pt_turn", "mix", "bypass", "lane_on", "studio_on" })
        IFX_CHECK_MSG (ids.contains (id), id);
    IFX_CHECK (p.getBypassParameter() == p.param (P::bypass));
}

IFX_TEST (Host, silenceZeroLengthAndSampleRates)
{
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        Rig r (sr, 256);
        juce::AudioBuffer<float> empty (2, 0);
        juce::MidiBuffer m;
        r.proc->processBlock (empty, m); // zero-length block
        juce::AudioBuffer<float> silent (2, 4096);
        silent.clear();
        r.proc->param (P::hum)->setValueNotifyingHost (0.0f);
        r.proc->processBlock (silent, m);
        r.proc->processBlock (silent, m);
        IFX_CHECK_MSG (silent.getMagnitude (0, 4096) < 1e-6f, juce::String (sr));
        auto out = r.process (4096);
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4096; ++i) finite &= std::isfinite (out.getSample (ch, i));
        IFX_CHECK (finite);
    }
}

IFX_TEST (Host, stateRoundTripThroughHostBlob)
{
    Rig a;
    a.proc->loadFactoryPreset (10);
    a.proc->setSeed (0xABCDEFull);
    a.proc->setSeedLock (false);
    a.proc->newTake();
    a.proc->param (P::jkWear)->setValueNotifyingHost (0.123f);
    juce::MemoryBlock blob;
    a.proc->getStateInformation (blob);
    Rig b;
    b.proc->setStateInformation (blob.getData(), (int) blob.getSize());
    IFX_CHECK (b.proc->getCurrentState().soundEquals (a.proc->getCurrentState()));
    IFX_CHECK (b.proc->getPresetName() == a.proc->getPresetName());
    // garbage leaves the state intact
    const auto before = b.proc->getCurrentState();
    b.proc->setStateInformation ("garbage", 7);
    IFX_CHECK (b.proc->getCurrentState().soundEquals (before));
}

IFX_TEST (Host, midiNotesTriggerGesturesAndCcLearn)
{
    Rig r;
    r.proc->setSlots (SourceType::LooseJack, SourceType::None);
    juce::AudioBuffer<float> b (2, 512);
    b.clear();
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 10);
    r.proc->processBlock (b, m);
    IFX_CHECK (r.proc->telemetry().slot[0].gestures.load() == 1);
    // MIDI learn: next CC maps to the learning parameter
    r.proc->startMidiLearn (P::jkWiggle);
    juce::MidiBuffer cc;
    cc.addEvent (juce::MidiMessage::controllerEvent (2, 20, 64), 0);
    r.proc->processBlock (b, cc);
    // the processor's timer applies it on the message thread
    IFX_CHECK (pumpUntil ([&] { return r.proc->describeMidiMapping (P::jkWiggle).contains ("CC 20"); }));
    juce::MidiBuffer cc2;
    cc2.addEvent (juce::MidiMessage::controllerEvent (2, 20, 127), 0);
    r.proc->processBlock (b, cc2);
    pumpUntil ([&] { return r.proc->apvts.getRawParameterValue ("jk_wiggle")->load() > 0.999f; });
    IFX_CLOSE (r.proc->apvts.getRawParameterValue ("jk_wiggle")->load(), 1.0f, 1e-4f);
}

IFX_TEST (Host, slotRulesNeverDuplicate)
{
    InterferenceProcessor p;
    p.setSlots (SourceType::DirtyPot, SourceType::DirtyPot);
    IFX_CHECK (p.slotType (0) == SourceType::DirtyPot && p.slotType (1) == SourceType::None);
    p.setSlots (SourceType::DirtyPot, SourceType::PlugTouch);
    p.setSlotSource (0, SourceType::PlugTouch); // move B's source to A: they swap
    IFX_CHECK (p.slotType (0) == SourceType::PlugTouch && p.slotType (1) == SourceType::DirtyPot);
}

IFX_TEST (Host, resetRandomizeAndAB)
{
    Rig r;
    r.proc->randomize();
    const auto s1 = r.proc->getCurrentState();
    r.proc->randomize();
    const auto s2 = r.proc->getCurrentState();
    IFX_CHECK (! s1.soundEquals (s2));
    IFX_CHECK (! s2.isExpressive()); // randomize stays Natural
    r.proc->resetToDefaults();
    const auto init = FactoryPresets::initState();
    for (int i = 0; i < kNumParams; ++i)
        IFX_CHECK_MSG (std::abs (r.proc->getCurrentState().params[i] - init.params[i]) < 1e-4f * std::max (1.0f, std::abs (init.params[i])),
                       paramInfo (i).id); // relative: log-skewed ranges round-trip through float normalisation
    IFX_CHECK (! r.proc->isModified());
    // A/B
    r.proc->loadFactoryPreset (3);
    const auto a = r.proc->getCurrentState();
    r.proc->selectAB (1);
    r.proc->loadFactoryPreset (7);
    const auto b = r.proc->getCurrentState();
    r.proc->selectAB (0);
    IFX_CHECK (r.proc->getCurrentState().soundEquals (a));
    r.proc->selectAB (1);
    IFX_CHECK (r.proc->getCurrentState().soundEquals (b));
}

IFX_TEST (Gui, editorOpensRendersAndResizes)
{
    Rig r;
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc->createEditor());
    IFX_CHECK (ed != nullptr);
    auto* e = dynamic_cast<InterferenceEditor*> (ed.get());
    IFX_CHECK (e != nullptr);
    if (e == nullptr) return;
    e->setSize (InterferenceEditor::kDefaultW, InterferenceEditor::kDefaultH);
    r.process (2048);
    pump (200);
    saveSnapshot (*e, "main_default");
    for (auto [w, h] : { std::pair<int, int> { InterferenceEditor::kMinW, InterferenceEditor::kMinH }, { 1600, 1000 }, { InterferenceEditor::kMaxW, InterferenceEditor::kMaxH } })
    {
        e->setSize (w, h);
        for (int tab = 0; tab < 5; ++tab) // every bottom panel must fit at every size
        {
            e->view().selectTab (tab);
            pump (30);
            juce::StringArray bad;
            const bool inside = allChildrenInside (*e, bad);
            IFX_CHECK_MSG (inside, juce::String (w) + "x" + juce::String (h) + " tab " + juce::String (tab) + ": " + bad.joinIntoString ("; "));
        }
        e->view().selectTab (1);
        pump (30);
        saveSnapshot (*e, "main_" + juce::String (w) + "x" + juce::String (h));
    }
}

IFX_TEST (Gui, everyTabOverlayPresetAndSourcePair)
{
    Rig r;
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc->createEditor());
    auto* e = dynamic_cast<InterferenceEditor*> (ed.get());
    if (e == nullptr) { IFX_CHECK (false); return; }
    e->setSize (InterferenceEditor::kDefaultW, InterferenceEditor::kDefaultH);
    auto& v = e->view();
    const char* tabNames[] = { "lane", "studio", "create", "routing", "environment" };
    for (int t = 0; t < 5; ++t)
    {
        v.selectTab (t);
        pump (40);
        saveSnapshot (*e, juce::String ("tab_") + tabNames[t]);
        IFX_CHECK (v.getSelectedTab() == t);
    }
    v.showHelp();   pump (40); IFX_CHECK (v.currentOverlay() != nullptr); saveSnapshot (*e, "overlay_help");   v.closeOverlay();
    v.showOptions(); pump (40); IFX_CHECK (v.currentOverlay() != nullptr); saveSnapshot (*e, "overlay_options"); v.closeOverlay();
    v.showRender(); pump (40); IFX_CHECK (v.currentOverlay() != nullptr); saveSnapshot (*e, "overlay_render");  v.closeOverlay();
    IFX_CHECK (v.currentOverlay() == nullptr);

    const auto& bank = FactoryPresets::all();
    for (int i = 0; i < bank.size(); ++i)
    {
        r.proc->loadFactoryPreset (i);
        r.process (1024);
        pump (35);
        IFX_CHECK (r.proc->getPresetName() == bank[i].name);
    }
    const SourceType all[5] = { SourceType::None, SourceType::LooseJack, SourceType::PlugTouch, SourceType::CableHandling, SourceType::DirtyPot };
    int n = 0;
    for (auto a : all)
        for (auto b : all)
        {
            if (a == b && a != SourceType::None) continue;
            r.proc->setSlots (a, b);
            r.process (512);
            pump (35);
            for (int s = 0; s < 2; ++s)
            {
                v.selectSlot (s);
                pump (5);
            }
            if (a != SourceType::None && b != SourceType::None && n++ < 4)
                saveSnapshot (*e, juce::String ("pair_") + sourceName (a) + "_" + sourceName (b));
        }
    IFX_CHECK (true);
}

IFX_TEST (Gui, concurrentAudioAndGuiActions)
{
    Rig r (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc->createEditor());
    std::atomic<bool> run { true };
    std::atomic<int> blocks { 0 };
    std::atomic<bool> finite { true };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> b (2, 256);
        juce::MidiBuffer m;
        int k = 0;
        while (run.load())
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i) b.setSample (ch, i, 0.2f * (float) std::sin (0.03 * (k * 256 + i)));
            m.clear();
            if (k % 7 == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60 + k % 12, (juce::uint8) 90), 3);
            if (k % 7 == 3) m.addEvent (juce::MidiMessage::noteOff (1, 60 + (k - 3) % 12), 5);
            r.proc->processBlock (b, m);
            if (! std::isfinite (b.getSample (0, 17))) finite.store (false);
            ++k;
            ++blocks;
        }
    });
    auto& rnd = juce::Random::getSystemRandom();
    for (int i = 0; i < 60; ++i)
    {
        switch (i % 6)
        {
            case 0: r.proc->loadFactoryPreset (rnd.nextInt (FactoryPresets::all().size())); break;
            case 1: r.proc->randomize(); break;
            case 2: r.proc->setSlotSource (rnd.nextInt (2), (SourceType) rnd.nextInt (5)); break;
            case 3: r.proc->selectAB (rnd.nextInt (2)); break;
            case 4: { Pattern p; p.lanes[0].steps[rnd.nextInt (16)].active = 1; r.proc->setPattern (p); break; }
            default: r.proc->param (rnd.nextInt (kNumParams))->setValueNotifyingHost (rnd.nextFloat()); break;
        }
        pump (15);
    }
    run.store (false);
    audio.join();
    IFX_CHECK (blocks.load() > 10);
    IFX_CHECK (finite.load());
    ed.reset(); // editor teardown after heavy use
}

IFX_TEST (Gui, teardownWhileRenderIsRunning)
{
    Rig r;
    r.proc->loadFactoryPreset (FactoryPresets::all().size() - 2);
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc->createEditor());
    auto* e = dynamic_cast<InterferenceEditor*> (ed.get());
    if (e == nullptr) { IFX_CHECK (false); return; }
    e->setSize (InterferenceEditor::kDefaultW, InterferenceEditor::kDefaultH);
    e->view().showRender();
    pump (20);
    // press "Render..." inside the overlay
    if (auto* ov = e->view().currentOverlay())
        for (auto* c : ov->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getButtonText() == "Render...")
                    b->triggerClick();
    pump (30);
    ed.reset(); // must cancel and join the render thread without hanging or crashing
    IFX_CHECK (true);
}

IFX_TEST (Gui, recordWhileProcessingThenStop)
{
    Rig r;
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ifx_gui_rec.wav");
    f.deleteFile();
    juce::String err;
    IFX_CHECK (r.proc->recorder().start (f, 48000.0, 2, 24, false, err));
    for (int i = 0; i < 50; ++i) r.process (512);
    IFX_CHECK (r.proc->recorder().stop() == 50 * 512);
    IFX_CHECK (f.getSize() > 50 * 512 * 6);
    f.deleteFile();
}

IFX_TEST (Gui, debugToolsCrashLogHardResetAndSecretPixel)
{
    Rig r;
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc->createEditor());
    auto* e = dynamic_cast<InterferenceEditor*> (ed.get());
    if (e == nullptr) { IFX_CHECK (false); return; }
    e->setSize (InterferenceEditor::kDefaultW, InterferenceEditor::kDefaultH);
    auto& v = e->view();

    // the hidden panel opens only from its one pixel
    auto click = [&v] (int x, int y)
    {
        const juce::Point<float> p ((float) x, (float) y);
        const auto now = juce::Time::getCurrentTime();
        juce::MouseEvent me (juce::Desktop::getInstance().getMainMouseSource(), p, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &v, &v, now, p, now, 1, false);
        v.mouseDown (me);
    };
    click (MainView::kSecretX + 6, MainView::kSecretY + 6);
    pump (20);
    IFX_CHECK (v.currentOverlay() == nullptr);
    click (MainView::kSecretX, MainView::kSecretY);
    pump (40);
    IFX_CHECK (dynamic_cast<GhostOverlay*> (v.currentOverlay()) != nullptr);
    saveSnapshot (*e, "overlay_ghost");
    v.closeOverlay();

    // debug window: live data and misconfiguration checks
    r.proc->param (P::mix)->setValueNotifyingHost (0.0f);
    v.showDebug();
    pump (80);
    auto* d = dynamic_cast<DebugOverlay*> (v.currentOverlay());
    IFX_CHECK (d != nullptr);
    if (d == nullptr) return;
    saveSnapshot (*e, "overlay_debug");
    IFX_CHECK (d->currentLiveText().contains ("PARAMETERS"));
    IFX_CHECK (d->currentLiveText().contains ("Dry/Wet is 0 %"));
    r.proc->param (P::mix)->setValueNotifyingHost (1.0f);

    // troubleshooting file: self-test passes, carries version and settings
    juce::File tf;
    juce::String err;
    IFX_CHECK_MSG (Diagnostics::writeTroubleshootingFile (*r.proc, tf, err), err);
    const auto report = tf.loadFileAsString();
    IFX_CHECK (report.contains ("[PASS]") && ! report.contains ("[FAIL]"));
    IFX_CHECK (report.contains (IFX_VERSION_STRING) && report.contains ("jk_insert"));
    tf.deleteFile();

    // crash log: off on load, starts with a troubleshooting file, records activity, closes cleanly
    IFX_CHECK (! r.proc->isCrashLogging());
    d->setCrashLog (true);
    IFX_CHECK (r.proc->isCrashLogging());
    const auto logFile = CrashLog::currentFile();
    r.proc->param (P::hum)->setValueNotifyingHost (0.9f);
    pump (900); // the processor timer appends activity about twice a second
    d->setCrashLog (false);
    IFX_CHECK (! r.proc->isCrashLogging());
    const auto log = logFile.loadFileAsString();
    IFX_CHECK (log.contains ("troubleshooting file") && log.contains ("Hum =") && log.contains ("closed normally"));
    logFile.deleteFile();

    // hard reset clears MIDI mappings and options (the options file lives in the temp data root)
    IFX_CHECK (r.proc->settings().getFile().isAChildOf (tempDataRoot.dir));
    IFX_CHECK (tf.isAChildOf (tempDataRoot.dir) && logFile.isAChildOf (tempDataRoot.dir));
    r.proc->startMidiLearn (P::jkWiggle);
    juce::AudioBuffer<float> b (2, 256);
    b.clear();
    juce::MidiBuffer cc;
    cc.addEvent (juce::MidiMessage::controllerEvent (1, 21, 64), 0);
    r.proc->processBlock (b, cc);
    pump (120);
    IFX_CHECK (r.proc->getMidiMap().size() == 1);
    r.proc->setTooltipsEnabled (false);
    r.proc->param (P::ghostOn)->setValueNotifyingHost (1.0f);
    d->performHardReset();
    IFX_CHECK (r.proc->getMidiMap().isEmpty());
    IFX_CHECK (r.proc->tooltipsEnabled());
    IFX_CHECK (r.proc->apvts.getRawParameterValue ("ghost_on")->load() < 0.5f);
    // a MIDI learn left pending before a hard reset must not come back as a mapping
    r.proc->startMidiLearn (P::jkWiggle);
    r.proc->processBlock (b, cc);
    d->performHardReset();
    pump (120);
    IFX_CHECK (r.proc->getMidiMap().isEmpty() && r.proc->midiLearnTarget() < 0);
    v.closeOverlay();
}

IFX_TEST (Gui, detailsDrawerAndEnvironmentTabFitTheMinimumWindow)
{
    Rig r;
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc->createEditor());
    auto* e = dynamic_cast<InterferenceEditor*> (ed.get());
    if (e == nullptr) { IFX_CHECK (false); return; }
    e->setSize (InterferenceEditor::kMinW, InterferenceEditor::kMinH);
    auto& v = e->view();
    pump (60);

    // Details drawer: every control scrolls inside the viewport with a real size (nothing squeezed to 0 px)
    auto* ctx = findDescendant<ContextPanel> (v);
    IFX_CHECK (ctx != nullptr);
    if (ctx == nullptr) return;
    const SourceType sources[5] = { SourceType::LooseJack, SourceType::PlugTouch, SourceType::CableHandling,
                                    SourceType::DirtyPot, SourceType::None };
    for (auto s : sources)
    {
        r.proc->setSlots (s, SourceType::None);
        v.selectSlot (0);
        pump (60);
        ctx->setDetailsOpen (true);
        pump (30);
        const juce::String name (sourceName (s));
        auto& drawer = ctx->getDrawer();
        auto& content = ctx->getDrawerContent();
        IFX_CHECK_MSG (drawer.isVisible() && drawer.getHeight() >= 60 && ctx->getLocalBounds().contains (drawer.getBounds()),
                       name + ": drawer " + drawer.getBounds().toString());
        int shown = 0;
        for (auto* ch : content.getChildren())
        {
            if (! ch->isVisible())
                continue;
            ++shown;
            IFX_CHECK_MSG (ch->getWidth() > 0 && ch->getHeight() > 0 && content.getLocalBounds().contains (ch->getBounds()),
                           name + ": " + juce::String (typeid (*ch).name()) + " " + ch->getBounds().toString());
        }
        IFX_CHECK_MSG (shown >= 10, name + ": only " + juce::String (shown) + " drawer items");
        juce::StringArray bad;
        IFX_CHECK_MSG (allChildrenInside (*e, bad), name + ": " + bad.joinIntoString ("; "));
        if (s == SourceType::LooseJack)
            saveSnapshot (*e, "details_1000x700");
        ctx->setDetailsOpen (false);
        pump (20);
    }

    // Environment tab: every choice readable (>= 120 px) and every knob a real knob, at the minimum and default sizes
    v.selectTab (4);
    pump (40);
    auto* env = findDescendant<EnvironmentPanel> (v);
    IFX_CHECK (env != nullptr);
    if (env == nullptr) return;
    for (auto [w, h] : { std::pair<int, int> { InterferenceEditor::kMinW, InterferenceEditor::kMinH },
                         { InterferenceEditor::kDefaultW, InterferenceEditor::kDefaultH } })
    {
        e->setSize (w, h);
        pump (40);
        const juce::String size = juce::String (w) + "x" + juce::String (h);
        int choices = 0, knobs = 0;
        for (auto* ch : env->getChildren())
        {
            if (auto* pc = dynamic_cast<ParamChoice*> (ch))
            {
                ++choices;
                IFX_CHECK_MSG (pc->box.getWidth() >= 120 && pc->box.getHeight() >= 22 && env->getLocalBounds().contains (pc->getBounds()),
                               size + ": " + paramInfo (pc->getParamIndex()).name + " " + pc->box.getBounds().toString());
            }
            else if (auto* k = dynamic_cast<ParamKnob*> (ch))
            {
                ++knobs;
                IFX_CHECK_MSG (k->slider.getWidth() >= 30 && k->slider.getHeight() >= 20 && env->getLocalBounds().contains (k->getBounds()),
                               size + ": " + paramInfo (k->getParamIndex()).name + " " + k->slider.getBounds().toString());
            }
        }
        IFX_CHECK_MSG (choices == 9 && knobs == 6, size + ": " + juce::String (choices) + " choices, " + juce::String (knobs) + " knobs");
        juce::StringArray bad;
        IFX_CHECK_MSG (allChildrenInside (*e, bad), size + ": " + bad.joinIntoString ("; "));
        saveSnapshot (*e, "environment_" + size);
    }
}

// T13: a project saved and reopened (fresh processor, host blob) plays back the same audio,
// sample for sample, when the seed is locked and the transport starts from the same position.
IFX_TEST (Host, reopenedProjectRendersIdenticalAudio)
{
    constexpr double sr = 48000.0;
    constexpr int block = 512, blocks = 200;   // ~2.1 s
    auto setValue = [] (InterferenceProcessor& p, int index, float v)
    {
        auto* prm = p.param (index);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };

    auto a = std::make_unique<InterferenceProcessor>();
    a->setPlayConfigDetails (2, 2, sr, block);
    a->setSlots (SourceType::LooseJack, SourceType::PlugTouch);
    a->setSeed (0x5EED1234ull);
    a->setSeedLock (true);
    Pattern pat;
    for (int i = 0; i < 16; i += 2) pat.lanes[0].steps[i].active = 1;
    for (int i = 0; i < 12; i += 3) pat.lanes[1].steps[i].active = 1;
    pat.lanes[1].length = 12;
    a->setPattern (pat);
    // values chosen to be exact in the normalised parameter range (no float drift on reload)
    setValue (*a, P::laneOn, 1.0f);
    setValue (*a, P::studioOn, 1.0f);
    setValue (*a, P::studioSrc, 2.0f);
    setValue (*a, P::drOn, 1.0f);
    setValue (*a, P::ecOn, 1.0f);
    setValue (*a, P::hum, 0.5f);
    setValue (*a, P::buzz, 0.25f);
    setValue (*a, P::rf, 0.25f);
    setValue (*a, P::rfType, 2.0f);
    setValue (*a, P::jkInsert, 0.75f);
    setValue (*a, P::jkWear, 0.75f);
    setValue (*a, P::jkWiggle, 0.5f);
    setValue (*a, P::tcTouch, 0.5f);
    a->prepareToPlay (sr, block);   // the host (re)starts playback with the configured project
    const auto first = renderFromStart (*a, blocks, block, sr);

    juce::MemoryBlock blob;
    a->getStateInformation (blob);
    a.reset();                      // the project is closed

    auto b = std::make_unique<InterferenceProcessor>();
    b->setPlayConfigDetails (2, 2, sr, block);
    b->setStateInformation (blob.getData(), (int) blob.getSize());
    b->prepareToPlay (sr, block);
    const auto second = renderFromStart (*b, blocks, block, sr);

    double diff = 0.0, rms = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < first.getNumSamples(); ++i)
        {
            diff = std::max (diff, (double) std::abs (first.getSample (ch, i) - second.getSample (ch, i)));
            rms += (double) first.getSample (ch, i) * first.getSample (ch, i);
        }
    rms = std::sqrt (rms / (2.0 * first.getNumSamples()));
    IFX_CHECK (rms > 1.0e-3);
    IFX_CHECK (b->telemetry().slot[0].gestures.load() > 0);   // the lane really played
    IFX_CHECK_MSG (diff < 1.0e-7, "reopened project differs by " + juce::String (diff, 9));
}

namespace
{
void collectKnobs (juce::Component& c, std::vector<ParamKnob*>& out)
{
    for (auto* ch : c.getChildren())
    {
        if (auto* k = dynamic_cast<ParamKnob*> (ch))
            out.push_back (k);
        collectKnobs (*ch, out);
    }
}

/** The (first) knob bound to a parameter anywhere under `root`, visible or not. */
ParamKnob* findKnob (juce::Component& root, int param)
{
    std::vector<ParamKnob*> all;
    collectKnobs (root, all);
    for (auto* k : all)
        if (k->getParamIndex() == param)
            return k;
    return nullptr;
}

/** What a screen reader announces: the component title, or a button's text. */
juce::String accessibleName (juce::Component& c)
{
    if (c.getTitle().isNotEmpty())
        return c.getTitle();
    if (auto* b = dynamic_cast<juce::Button*> (&c))
        return b->getButtonText();
    return {};
}

/** Visible components that take keyboard focus (the editor's focus chain), without descending
    into the internal parts of JUCE sliders, combo boxes and scroll bars. */
void collectFocusable (juce::Component& c, std::vector<juce::Component*>& out)
{
    for (auto* ch : c.getChildren())
    {
        if (! ch->isVisible() || dynamic_cast<juce::ScrollBar*> (ch) != nullptr)
            continue;
        if (ch->getWantsKeyboardFocus())
            out.push_back (ch);
        if (dynamic_cast<juce::Slider*> (ch) != nullptr || dynamic_cast<juce::ComboBox*> (ch) != nullptr)
            continue;
        collectFocusable (*ch, out);
    }
}

bool isParameterControl (juce::Component& c)
{
    if (dynamic_cast<ParamToggle*> (&c) != nullptr || dynamic_cast<MomentaryParamButton*> (&c) != nullptr)
        return true;
    if (auto* p = c.getParentComponent())
        return dynamic_cast<ParamKnob*> (p) != nullptr || dynamic_cast<ParamChoice*> (p) != nullptr;
    return false;
}

juce::String tooltipOf (juce::Component& c)
{
    if (auto* t = dynamic_cast<juce::TooltipClient*> (&c))
        return t->getTooltip();
    return {};
}
}

// R10.5 / T31: the "Why is it quiet?" explanation and the dormant / limited flags follow the
// engine's actual dependencies: a still pot, a cable with Mic 0, a shielded touch, and a
// Buffered/Line cable with shield damage (and its Passive counterpart).
IFX_TEST (Gui, whyQuietAndDormantFlagsFollowTheEngine)
{
    Rig r;
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc->createEditor());
    auto* e = dynamic_cast<InterferenceEditor*> (ed.get());
    IFX_CHECK (e != nullptr);
    if (e == nullptr) return;
    e->setSize (InterferenceEditor::kDefaultW, InterferenceEditor::kDefaultH);

    auto setValue = [&r] (int index, float v)
    {
        auto* p = r.proc->param (index);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    auto loadSource = [&] (SourceType t)
    {
        r.proc->setSlots (t, SourceType::None);
        e->view().selectSlot (0);
    };
    // audio runs (engine telemetry), then the UI timers refresh (30 Hz view, 4 Hz knob tooltips)
    auto settle = [&]
    {
        for (int k = 0; k < 8; ++k) r.process (2048, 0.0f);
        pump (600);
    };
    auto why = [&r] { return explainQuiet (*r.proc, 0, SlotView::read (r.proc->telemetry().slot[0])); };
    auto tip = [] (ParamKnob* k) { return k != nullptr ? k->slider.getTooltip() : juce::String ("<no knob>"); };

    // 1. a still pot: no scratch, Scratch is limited, Vibration is dormant below 70 % Dirt
    loadSource (SourceType::DirtyPot);
    setValue (P::ptDirt, 0.5f);
    setValue (P::ptTurn, 0.5f);
    setValue (P::ptVibration, 0.0f);
    settle();
    IFX_CHECK_MSG (why().contains ("Pot is stationary"), why());
    IFX_CHECK_MSG (tip (findKnob (*e, P::ptScratch)).contains ("Limited: Pot is stationary"), tip (findKnob (*e, P::ptScratch)));
    if (auto* vib = findKnob (*e, P::ptVibration))
    {
        IFX_CHECK (! vib->slider.isEnabled());
        IFX_CHECK_MSG (tip (vib).contains ("Only a severely dirty pot"), tip (vib));
    }
    else
        IFX_CHECK_MSG (false, "no Vibration knob for the pot");

    // 2. a sound cable with Mic 0: handling needs the mic path, Shake is limited
    loadSource (SourceType::CableHandling);
    setValue (P::cbMic, 0.0f);
    setValue (P::cbDamage, 0.0f);
    setValue (P::cbShieldDmg, 0.0f);
    settle();
    IFX_CHECK_MSG (why().contains ("Cable mic is off"), why());
    IFX_CHECK_MSG (tip (findKnob (*e, P::cbShake)).contains ("Limited: Healthy cable, mic off"), tip (findKnob (*e, P::cbShake)));

    // 3. a shielded, terminated touch with no hum in the room: explained, Body dormant
    loadSource (SourceType::PlugTouch);
    setValue (P::tcTouch, 0.8f);
    setValue (P::tcShield, 1.0f);
    setValue (P::tcExposure, 0.1f);
    setValue (P::hum, 0.0f);
    setValue (P::buzz, 0.0f);
    setValue (P::rf, 0.0f);
    settle();
    IFX_CHECK_MSG (why().contains ("Circuit is shielded / terminated"), why());
    IFX_CHECK_MSG (why().contains ("There is no hum in the environment"), why());
    IFX_CHECK_MSG (! why().contains ("buffered / line source"), why());   // the touched tip is a floating node
    if (auto* body = findKnob (*e, P::tcBody))
    {
        IFX_CHECK (! body->slider.isEnabled());
        IFX_CHECK_MSG (tip (body).contains ("No hum in the environment to colour"), tip (body));
    }
    else
        IFX_CHECK_MSG (false, "no Body knob for the touch");

    // 4. a Buffered/Line source with a broken cable shield: ~34 dB less hum, explained and flagged;
    //    Cable Load is dormant. With a Passive Pickup source none of that applies.
    loadSource (SourceType::CableHandling);
    setValue (P::hum, 0.5f);
    setValue (P::cbShieldDmg, 0.6f);
    setValue (P::inputMode, 0.0f);
    settle();
    IFX_CHECK_MSG (why().contains ("buffered / line source holds the exposed conductor down"), why());
    IFX_CHECK_MSG (tip (findKnob (*e, P::cbShieldDmg)).contains ("Limited: Line source"), tip (findKnob (*e, P::cbShieldDmg)));
    if (auto* cable = findKnob (*e, P::cableLoad))
    {
        IFX_CHECK (! cable->slider.isEnabled());
        IFX_CHECK_MSG (tip (cable).contains ("Cable Load only matters with a Passive Pickup"), tip (cable));
    }
    else
        IFX_CHECK_MSG (false, "no Cable Load knob in the Details drawer");

    setValue (P::inputMode, 1.0f);
    settle();
    IFX_CHECK_MSG (! why().contains ("buffered / line source"), why());
    IFX_CHECK_MSG (! tip (findKnob (*e, P::cbShieldDmg)).contains ("Limited:"), tip (findKnob (*e, P::cbShieldDmg)));
    if (auto* cable = findKnob (*e, P::cableLoad))
        IFX_CHECK (cable->slider.isEnabled());
}

// Accessibility: every control in the focus chain (every tab, Details drawer open) has an
// accessible name, and every parameter control has a tooltip.
IFX_TEST (Gui, focusChainHasTitlesAndParameterTooltips)
{
    Rig r;
    std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc->createEditor());
    auto* e = dynamic_cast<InterferenceEditor*> (ed.get());
    IFX_CHECK (e != nullptr);
    if (e == nullptr) return;
    e->setSize (InterferenceEditor::kDefaultW, InterferenceEditor::kDefaultH);
    r.process (2048);
    pump (200);
    if (auto* ctx = findDescendant<ContextPanel> (*e))
        ctx->setDetailsOpen (true);

    juce::StringArray untitled, noTooltip;
    int checked = 0;
    for (int tab = 0; tab < 5; ++tab)
    {
        e->view().selectTab (tab);
        pump (300);                    // knob tooltips refresh at 4 Hz
        std::vector<juce::Component*> focusable;
        collectFocusable (*e, focusable);
        for (auto* c : focusable)
        {
            ++checked;
            auto* parent = c->getParentComponent();
            const juce::String where = juce::String (typeid (*c).name()) + " '" + c->getName() + "' " + c->getBounds().toString()
                                     + (parent != nullptr ? juce::String (" in ") + typeid (*parent).name() : juce::String());
            if (accessibleName (*c).trim().isEmpty())
                untitled.addIfNotAlreadyThere (where);
            if (isParameterControl (*c) && tooltipOf (*c).trim().isEmpty())
                noTooltip.addIfNotAlreadyThere (where + " title '" + accessibleName (*c) + "'");
        }
    }
    IFX_CHECK (checked > 20);
    IFX_CHECK_MSG (untitled.isEmpty(), juce::String (untitled.size()) + " focusable controls without a title: " + untitled.joinIntoString ("; "));
    IFX_CHECK_MSG (noTooltip.isEmpty(), juce::String (noTooltip.size()) + " parameter controls without a tooltip: " + noTooltip.joinIntoString ("; "));
}
