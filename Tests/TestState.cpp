// Plugin state, presets and migration (master spec 10; WP6 / WP8).
#include "TestHelpers.h"
#include "Plugin/StateIO.h"
#include "Plugin/FactoryPresets.h"
#include "Render/OfflineRenderer.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
PluginState richState()
{
    PluginState s;
    s.seed = 0xDEADBEEFCAFEull;
    s.seedLock = false;
    s.take = 7;
    s.slotA = SourceType::DirtyPot;
    s.slotB = SourceType::PlugTouch;
    s.params[P::ptDirt] = 0.9f;
    s.params[P::policy] = 1.0f;
    s.params[P::laneDiv] = 5.0f;
    s.params[P::crRate] = 12345.0f;
    s.pattern.lanes[0].length = 24;
    s.pattern.lanes[0].steps[3].active = 1;
    s.pattern.lanes[0].steps[3].ratchet = 3;
    s.pattern.lanes[0].steps[3].target = 0.42f;
    s.pattern.lanes[1].length = 12;
    s.pattern.lanes[1].solo = 1;
    s.pattern.lanes[1].steps[11].active = 1;
    s.pattern.lanes[1].steps[11].probability = 0.25f;
    s.midiMap.add ({ 3, 74, P::ptTurn });
    s.midiMap.add ({ 0, 1, P::jkWiggle });
    s.presetName = "Round Trip";
    s.category = "Wild";
    return s;
}

juce::File tempDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("InterferenceTests_" + juce::String (juce::Time::currentTimeMillis()));
    d.createDirectory();
    return d;
}
}

IFX_TEST (State, binaryRoundTripPreservesEverything)
{
    const auto s = richState();
    juce::MemoryBlock mb;
    StateIO::toBinary (s, mb);
    PluginState r;
    IFX_CHECK (StateIO::fromBinary (mb.getData(), (int) mb.getSize(), r));
    IFX_CHECK (r.soundEquals (s));
    IFX_CHECK (r.presetName == s.presetName && r.category == s.category);
}

IFX_TEST (State, xmlBlobIsAccepted)
{
    const auto s = richState();
    const auto xml = StateIO::toValueTree (s).toXmlString();
    PluginState r;
    IFX_CHECK (StateIO::fromBinary (xml.toRawUTF8(), (int) xml.getNumBytesAsUTF8(), r));
    IFX_CHECK (r.soundEquals (s));
}

IFX_TEST (State, malformedDataIsRejectedWithoutTouchingState)
{
    PluginState r = richState();
    const auto before = r;
    const char junk[] = "this is not a plugin state \x01\x02\x03";
    IFX_CHECK (! StateIO::fromBinary (junk, (int) sizeof (junk), r));
    IFX_CHECK (! StateIO::fromBinary (nullptr, 0, r));
    juce::ValueTree other ("SomethingElse");
    IFX_CHECK (! StateIO::fromValueTree (other, r));
    IFX_CHECK (r.soundEquals (before));
}

IFX_TEST (State, outOfRangeAndNonFiniteValuesAreRepaired)
{
    auto t = StateIO::toValueTree (PluginState());
    auto p = t.getChildWithName ("Params");
    p.setProperty (paramInfo (P::jkWear).id, 17.0, nullptr);
    p.setProperty (paramInfo (P::crRate).id, -5.0, nullptr);
    p.setProperty (paramInfo (P::hum).id, std::numeric_limits<double>::quiet_NaN(), nullptr);
    p.setProperty (paramInfo (P::excType).id, 2.7, nullptr);
    t.setProperty ("slotA", 99, nullptr);
    t.setProperty ("seed", "zz-not-hex", nullptr);
    auto lane = t.getChildWithName ("Pattern").getChild (0);
    lane.setProperty ("length", 5000, nullptr);
    juce::ValueTree step ("Step");
    step.setProperty ("i", 2, nullptr);
    step.setProperty ("on", 1, nullptr);
    step.setProperty ("rat", 99, nullptr);
    step.setProperty ("prb", -3.0, nullptr);
    lane.appendChild (step, nullptr);
    juce::ValueTree bad ("Step");
    bad.setProperty ("i", 999, nullptr);
    lane.appendChild (bad, nullptr);

    PluginState r;
    juce::StringArray warnings;
    IFX_CHECK (StateIO::fromValueTree (t, r, &warnings));
    IFX_CHECK (r.params[P::jkWear] == 1.0f);
    IFX_CHECK (r.params[P::crRate] == paramInfo (P::crRate).min);
    IFX_CHECK (r.params[P::hum] == paramInfo (P::hum).def);
    IFX_CHECK (r.params[P::excType] == 3.0f);
    IFX_CHECK ((int) r.slotA == kNumSourceTypes);
    IFX_CHECK (r.seed == PluginState().seed);
    IFX_CHECK (r.pattern.lanes[0].length == kMaxSteps);
    IFX_CHECK (r.pattern.lanes[0].steps[2].ratchet == 4 && r.pattern.lanes[0].steps[2].probability == 0.0f);
    IFX_CHECK (warnings.size() >= 3);
}

IFX_TEST (State, olderStateMissingFieldsMigratesToDefaults)
{
    // a "version 0" state: only a few params, no pattern, no MIDI map, unknown extra fields
    juce::ValueTree t ("InterferenceState");
    t.setProperty ("version", 0, nullptr);
    t.setProperty ("slotA", 3, nullptr);
    t.setProperty ("someFutureField", "whatever", nullptr);
    juce::ValueTree p ("Params");
    p.setProperty ("cb_damage", 0.66, nullptr);
    p.setProperty ("a_param_that_was_removed", 0.5, nullptr);
    t.appendChild (p, nullptr);
    t.appendChild (juce::ValueTree ("UnknownChild"), nullptr);
    PluginState r;
    IFX_CHECK (StateIO::fromValueTree (t, r));
    IFX_CHECK (r.slotA == SourceType::CableHandling);
    IFX_CLOSE (r.params[P::cbDamage], 0.66f, 1e-6f);
    IFX_CHECK (r.params[P::hum] == paramInfo (P::hum).def);
    IFX_CHECK (! r.params.on (P::slotASolo) && ! r.params.on (P::slotBSolo));
    IFX_CHECK (r.params[P::slotALife] == 0.0f && r.params[P::slotBLife] == 0.0f);
    IFX_CHECK (r.pattern == Pattern());
    IFX_CHECK (r.midiMap.isEmpty());
}

IFX_TEST (State, newerVersionLoadsWithWarning)
{
    auto t = StateIO::toValueTree (richState());
    t.setProperty ("version", 99, nullptr);
    PluginState r;
    juce::StringArray w;
    IFX_CHECK (StateIO::fromValueTree (t, r, &w));
    IFX_CHECK (r.soundEquals (richState()));
    IFX_CHECK (w.size() == 1);
}

IFX_TEST (State, duplicateSourceIsRejected)
{
    auto t = StateIO::toValueTree (PluginState());
    t.setProperty ("slotA", 2, nullptr);
    t.setProperty ("slotB", 2, nullptr);
    PluginState r;
    IFX_CHECK (StateIO::fromValueTree (t, r));
    IFX_CHECK (r.slotA == SourceType::PlugTouch && r.slotB == SourceType::None);
}

IFX_TEST (State, midiMapIsStoredByStableIdNotIndex)
{
    const auto t = StateIO::toValueTree (richState());
    const auto xml = t.toXmlString();
    IFX_CHECK (xml.contains ("param=\"pt_turn\""));
    IFX_CHECK (! xml.containsIgnoreCase ("C:\\") && ! xml.contains ("/Users/")); // no machine paths
}

IFX_TEST (State, presetFileRoundTripAndErrors)
{
    const auto dir = tempDir();
    const auto f = dir.getChildFile ("Test Preset.ifxpreset");
    juce::String err;
    IFX_CHECK (StateIO::savePresetFile (richState(), f, err));
    PluginState r;
    IFX_CHECK (StateIO::loadPresetFile (f, r, err));
    IFX_CHECK (r.soundEquals (richState()));

    const auto junk = dir.getChildFile ("junk.ifxpreset");
    junk.replaceWithText ("<not closed");
    IFX_CHECK (! StateIO::loadPresetFile (junk, r, err) && err.isNotEmpty());
    IFX_CHECK (! StateIO::loadPresetFile (dir.getChildFile ("missing.ifxpreset"), r, err));
    dir.deleteRecursively();
}

IFX_TEST (Presets, factoryBankIsCompleteAndValid)
{
    const auto& bank = FactoryPresets::all();
    IFX_CHECK (bank.size() >= 20);
    juce::StringArray names;
    for (const auto& c : FactoryPresets::categories())
    {
        int n = 0;
        for (const auto& p : bank) n += p.category == c;
        IFX_CHECK_MSG (n >= 3, c + " has " + juce::String (n) + " presets");
    }
    int machine = 0;
    for (const auto& p : bank)
    {
        IFX_CHECK (FactoryPresets::categories().contains (p.category));
        IFX_CHECK (! names.contains (p.name));
        names.add (p.name);
        IFX_CHECK (p.description.length() > 20);
        IFX_CHECK (p.state.slotA != p.state.slotB || p.state.slotA == SourceType::None);
        for (int i = 0; i < kNumParams; ++i)
            IFX_CHECK (StateIO::sanitise (i, p.state.params[i]) == p.state.params[i]);
        // realistic categories use the Natural policy
        if (p.category == "Repair Faults" || p.category == "Subtle Texture")
            IFX_CHECK_MSG (! p.state.isExpressive() && ! p.state.usesStudio(), p.name);
        machine += p.category == "Machine Pattern";
    }
    IFX_CHECK (machine == 4);
}

IFX_TEST (Presets, everyFactoryPresetRendersFiniteWithSafePeaks)
{
    for (const auto& p : FactoryPresets::all())
    {
        RenderSettings rs;
        rs.useBars = false;
        rs.seconds = 1.5;
        rs.sampleRate = 48000.0;
        auto r = OfflineRenderer::render (p.state, rs);
        IFX_CHECK (r.ok);
        IFX_CHECK_MSG (allFinite (r.audio), p.name);
        IFX_CHECK_MSG (r.peak <= 1.0f, p.name + " peak " + juce::String (r.peak));
    }
}

IFX_TEST (Presets, factoryPresetsPlayOnMusicWithoutPeaksOrLevelJumps)
{
    for (const auto& p : FactoryPresets::all())
    {
        Harness h (48000.0, 2, 512);
        h.params = p.state.params;
        h.engine->setSeed (p.state.seed);
        h.setSlots (p.state.slotA, p.state.slotB);
        h.publish (p.state.pattern);
        h.transport.hasHostTransport = true;
        h.transport.playing = true;
        auto in = drums (2, 96000, 48000.0);
        auto out = h.run (in);
        IFX_CHECK_MSG (allFinite (out), p.name);
        IFX_CHECK_MSG (peakOf (out) < 1.2f, p.name + " peak " + juce::String (peakOf (out)));
        IFX_CHECK_MSG (rmsOf (out) < rmsOf (in) * 2.5, p.name + " level jump");
    }
}

IFX_TEST (State, patternTreeRoundTrip)
{
    Pattern p = richState().pattern;
    Pattern r;
    StateIO::patternFromTree (StateIO::patternToTree (p), r);
    IFX_CHECK (r == p);
}
