// Host-contract and GUI tests: builds the real processor + editor headlessly
// (master spec 10/11: buses, state, MIDI, concurrent GUI actions, resizing, teardown).
#include "TestFramework.h"
#include "Plugin/PluginProcessor.h"
#include "Plugin/PluginEditor.h"
#include <thread>

using namespace ifx;
using namespace ifxtest;

namespace
{
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

bool allChildrenInside (juce::Component& c, juce::StringArray& bad)
{
    bool ok = true;
    for (auto* ch : c.getChildren())
    {
        if (! ch->isVisible() || ch->getWidth() <= 0) continue;
        if (! c.getLocalBounds().contains (ch->getBounds()) && dynamic_cast<Overlay*> (ch) == nullptr)
        {
            ok = false;
            bad.add (ch->getName() + " " + juce::String (typeid (*ch).name()) + " " + ch->getBounds().toString());
        }
        ok &= allChildrenInside (*ch, bad);
    }
    return ok;
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
    pump (120); // the processor's timer applies it on the message thread
    IFX_CHECK (r.proc->describeMidiMapping (P::jkWiggle).contains ("CC 20"));
    juce::MidiBuffer cc2;
    cc2.addEvent (juce::MidiMessage::controllerEvent (2, 20, 127), 0);
    r.proc->processBlock (b, cc2);
    pump (120);
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
        IFX_CHECK_MSG (std::abs (r.proc->getCurrentState().params[i] - init.params[i]) < 1e-4f, paramInfo (i).id);
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
        pump (50);
        juce::StringArray bad;
        IFX_CHECK_MSG (allChildrenInside (*e, bad), juce::String (w) + "x" + juce::String (h) + ": " + bad.joinIntoString ("; "));
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
    const char* tabNames[] = { "lane", "studio", "create", "routing" };
    for (int t = 0; t < 4; ++t)
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
