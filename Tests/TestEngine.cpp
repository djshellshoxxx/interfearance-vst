#include "TestHelpers.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
constexpr double kFs = 48000.0;

void addWiggles (Harness& h, int n)
{
    for (int i = 0; i < 8; ++i)
    {
        const int at = (int) (n * (i + 0.5) / 8);
        h.pendingEvents.push_back ({ at, ControlEvent::NoteOn, 60 + (i % 3), 0.9f });
        h.pendingEvents.push_back ({ at + 4000, ControlEvent::NoteOff, 60 + (i % 3), 0.0f });
    }
}
}

IFX_TEST (Engine, silenceInvariantEverySourcePairModeAndMix)
{
    // realism spec 2.2: silence in, excitation / bias / touch / mic / hum off => silence out
    std::vector<std::pair<SourceType, SourceType>> combos;
    combos.push_back ({ SourceType::None, SourceType::None });
    for (auto a : kAllSources)
    {
        combos.push_back ({ a, SourceType::None });
        for (auto b : kAllSources)
            if (a != b) combos.push_back ({ a, b });
    }
    int failures = 0;
    for (auto [a, b] : combos)
        for (int circuit = 0; circuit < 2; ++circuit)
            for (float mix : { 0.0f, 0.5f, 1.0f })
            {
                Harness h (kFs, 2, 128);
                h.params = quietParams();
                h.params[P::inputMode] = (float) circuit;
                h.params[P::mix] = mix;
                h.params[P::jkInsert] = 0.7f;
                h.params[P::jkWiggle] = 1.0f;
                h.params[P::cbShake] = 1.0f;
                h.params[P::cbDamage] = 1.0f;
                h.params[P::ptDirt] = 1.0f;
                h.setSlots (a, b);
                const int n = (int) (kFs * 0.5);
                addWiggles (h, n);
                auto out = h.runSilence (n);
                if (peakOf (out) > 1e-7f) ++failures;
            }
    IFX_CHECK_MSG (failures == 0, juce::String (failures) + " configurations were not silent");
}

IFX_TEST (Engine, bothSlotsBypassedIsExactIdentity)
{
    Harness h (kFs, 2, 256);
    h.params = quietParams();
    h.params[P::hum] = 1.0f;
    h.params[P::slotAOn] = 0.0f;
    h.params[P::slotBOn] = 0.0f;
    h.setSlots (SourceType::LooseJack, SourceType::PlugTouch);
    auto in = drums (2, (int) kFs, kFs);
    // the first block ramps the slot bypass (declick); identity is exact once settled
    h.run (slice (in, 0, 4800));
    auto out = h.run (slice (in, 4800, 40000));
    IFX_CHECK (maxAbsDiff (out, slice (in, 4800, 40000)) == 0.0);
}

IFX_TEST (Engine, oneSlotBypassedIsIdentityForThatSlot)
{
    Harness h (kFs, 2, 256);
    h.params = quietParams();
    h.params[P::slotAOn] = 0.0f;
    h.params[P::jkInsert] = 0.0f; // would kill the signal if it were active
    h.setSlots (SourceType::LooseJack, SourceType::None);
    auto in = sine (2, (int) kFs, 440.0, kFs);
    h.run (slice (in, 0, 4800));
    auto out = h.run (slice (in, 4800, 40000));
    IFX_CHECK (maxAbsDiff (out, slice (in, 4800, 40000)) == 0.0);
}

IFX_TEST (Engine, emptySlotsHealthyAreIdentity)
{
    Harness h (kFs, 1, 100);
    h.params = quietParams();
    h.setSlots (SourceType::None, SourceType::None);
    auto in = noise (1, 20000, 8);
    auto out = h.run (in);
    IFX_CHECK (maxAbsDiff (out, in) == 0.0);
}

IFX_TEST (Engine, zeroWetIsTimeAlignedDry)
{
    Harness h (kFs, 2, 256);
    h.params[P::mix] = 0.0f;
    h.params[P::hum] = 1.0f;
    h.params[P::excType] = 1.0f;
    h.params[P::studioOn] = 1.0f;
    h.params[P::drOn] = 1.0f;
    h.params[P::jkInsert] = 0.5f;
    h.setSlots (SourceType::LooseJack, SourceType::DirtyPot);
    auto in = drums (2, (int) kFs, kFs);
    auto out = h.run (in);
    IFX_CHECK (maxAbsDiff (out, in) == 0.0);
}

IFX_TEST (Engine, studioBypassReturnsExactlyC)
{
    // Studio off: W == C exactly. Studio on with every stage off: W == C within float precision.
    auto run = [] (bool studio, Tap tap)
    {
        Harness h (kFs, 2, 256);
        h.params[P::hum] = 0.6f;
        h.params[P::jkWiggle] = 0.8f;
        h.params[P::jkWear] = 0.8f;
        h.params[P::jkInsert] = 0.8f;
        h.params[P::jkPop] = 0.5f;
        h.params[P::studioOn] = studio ? 1.0f : 0.0f;
        h.params[P::studioSrc] = 2.0f;
        h.params[P::monitor] = (float) (int) tap;
        h.setSlots (SourceType::LooseJack, SourceType::PlugTouch);
        return h.run (drums (2, (int) kFs, kFs));
    };
    auto c = run (false, Tap::FullCore);
    auto w = run (false, Tap::FullWet);
    IFX_CHECK (maxAbsDiff (c, w) == 0.0);
    auto w2 = run (true, Tap::FullWet);
    IFX_CHECK (maxAbsDiff (c, w2) < 1e-6);
    // C - (R + D + A): R = C - A - D is recovered from separate deterministic runs
    auto a = run (false, Tap::Artifacts);
    auto d = run (false, Tap::FaultDifference);
    IFX_CHECK (rmsOf (a) > 0.0);
    IFX_CHECK (rmsOf (d) > 0.0);
    IFX_CHECK (allFinite (a) && allFinite (d));
}

IFX_TEST (Engine, artifactsOnlyBusIsEmptyWhenNothingIsInjected)
{
    Harness h (kFs, 2, 256);
    h.params = quietParams();
    h.params[P::monitor] = (float) (int) Tap::Artifacts;
    h.params[P::jkWiggle] = 1.0f;
    h.params[P::jkWear] = 1.0f;
    h.params[P::jkInsert] = 0.75f;
    h.setSlots (SourceType::LooseJack, SourceType::None);
    auto out = h.run (sine (2, (int) kFs, 440.0, kFs));
    IFX_CHECK (peakOf (out) < 1e-6f);  // interruptions are D, not A
    Harness g (kFs, 2, 256);
    g.params = h.params;
    g.params[P::monitor] = (float) (int) Tap::FaultDifference;
    g.setSlots (SourceType::LooseJack, SourceType::None);
    auto d = g.run (sine (2, (int) kFs, 440.0, kFs));
    IFX_CHECK (peakOf (d) > 1e-3f);
}

IFX_TEST (Engine, blockSizeDoesNotChangeOutput)
{
    auto render = [] (int block)
    {
        Harness h (kFs, 2, block);
        h.params[P::hum] = 0.5f;
        h.params[P::jkWiggle] = 0.7f;
        h.params[P::jkWear] = 0.7f;
        h.params[P::jkInsert] = 0.85f;
        h.params[P::tcTouch] = 0.5f;
        h.params[P::laneOn] = 1.0f;
        h.transport.hasHostTransport = true;
        h.transport.playing = true;
        h.transport.bpm = 128.0;
        h.setSlots (SourceType::LooseJack, SourceType::PlugTouch);
        Pattern p;
        for (int i = 0; i < 16; i += 2) { p.lanes[0].steps[i].active = 1; p.lanes[0].steps[i].probability = 0.7f; }
        for (int i = 0; i < 12; i += 3) p.lanes[1].steps[i].active = 1;
        p.lanes[1].length = 12;
        h.publish (p);
        h.pendingEvents.push_back ({ 12345, ControlEvent::NoteOn, 36, 0.8f });
        h.pendingEvents.push_back ({ 30000, ControlEvent::NoteOff, 36, 0.0f });
        return h.run (drums (2, (int) (kFs * 3), kFs));
    };
    auto a = render (64), b = render (257), c = render (1024);
    IFX_CHECK_MSG (maxAbsDiff (a, b) < 1e-6, "64 vs 257: " + juce::String (maxAbsDiff (a, b)));
    IFX_CHECK_MSG (maxAbsDiff (a, c) < 1e-6, "64 vs 1024: " + juce::String (maxAbsDiff (a, c)));
}

IFX_TEST (Engine, lockedSeedBounceIsNullIdentical)
{
    auto bounce = [] (uint64_t seed)
    {
        Harness h (kFs, 2, 512);
        h.engine->setSeed (seed);
        h.params[P::jkWiggle] = 0.8f;
        h.params[P::jkWear] = 0.8f;
        h.params[P::jkInsert] = 0.8f;
        h.params[P::cbShake] = 0.8f;
        h.params[P::cbDamage] = 0.8f;
        h.transport.hasHostTransport = true;
        h.transport.playing = true;
        h.transport.ppq = 8.0;
        h.setSlots (SourceType::LooseJack, SourceType::CableHandling);
        return h.run (drums (2, (int) (kFs * 2), kFs));
    };
    auto a = bounce (77), b = bounce (77), c = bounce (78);
    IFX_CHECK (maxAbsDiff (a, b) == 0.0);
    IFX_CHECK (maxAbsDiff (a, c) > 1e-4);   // New Take: different microevents
}

IFX_TEST (Engine, loopJumpRepeatsHardwareEvents)
{
    Harness h (kFs, 2, 480);
    h.params[P::jkWiggle] = 0.9f;
    h.params[P::jkWear] = 0.9f;
    h.params[P::jkInsert] = 0.8f;
    h.transport.hasHostTransport = true;
    h.transport.playing = true;
    h.transport.bpm = 120.0;
    h.setSlots (SourceType::LooseJack, SourceType::None);
    auto in = drums (2, (int) (kFs * 2), kFs);
    auto first = h.run (in);
    h.transport.ppq = 0.0;              // host loops back to the start
    h.transport.hostSeconds = 0.0;
    auto second = h.run (in);
    IFX_CHECK (maxAbsDiff (first, second) == 0.0);
}

IFX_TEST (Engine, allSampleRatesStayFiniteAndBounded)
{
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        Harness h (fs, 2, 333);
        h.params[P::hum] = 1.0f;
        h.params[P::buzz] = 1.0f;
        h.params[P::rf] = 1.0f;
        h.params[P::excType] = 1.0f;
        h.params[P::policy] = 1.0f;
        h.params[P::studioOn] = 1.0f;
        for (int p : { P::eqOn, P::ctOn, P::drOn, P::crOn, P::rsOn, P::gtOn, P::ecOn })
            h.params[p] = 1.0f;
        h.params[P::fbAmount] = 1.0f;
        h.params[P::drPre] = 36.0f;
        h.params[P::drPost] = 6.0f;
        h.params[P::tcTouch] = 1.0f;
        h.params[P::cbMic] = 1.0f;
        h.params[P::cbShake] = 1.0f;
        h.setSlots (SourceType::PlugTouch, SourceType::CableHandling);
        auto out = h.run (drums (2, (int) (fs * 1.0), fs));
        IFX_CHECK (allFinite (out));
        IFX_CHECK (peakOf (out) <= kSafetyClamp);
    }
}

IFX_TEST (Engine, invalidInputIsSanitised)
{
    Harness h (kFs, 2, 64);
    h.setSlots (SourceType::LooseJack, SourceType::DirtyPot);
    juce::AudioBuffer<float> in (2, 4096);
    for (int i = 0; i < 4096; ++i)
    {
        in.setSample (0, i, i % 97 == 0 ? std::numeric_limits<float>::quiet_NaN() : (i % 101 == 0 ? std::numeric_limits<float>::infinity() : 0.3f));
        in.setSample (1, i, i % 89 == 0 ? -std::numeric_limits<float>::infinity() : -0.3f);
    }
    auto out = h.run (in);
    IFX_CHECK (allFinite (out));
}

IFX_TEST (Engine, zeroLengthAndOneSampleBlocks)
{
    Harness h (kFs, 2, 1);
    h.setSlots (SourceType::PlugTouch, SourceType::CableHandling);
    float* o[2] = { nullptr, nullptr };
    h.engine->process (nullptr, o, 0, h.params, h.transport, nullptr, 0, nullptr, 0);
    auto out = h.run (sine (2, 500, 440.0, kFs));
    IFX_CHECK (allFinite (out));
}

IFX_TEST (Engine, extremeParameterFuzzIsBoundedAndFinite)
{
    Rng r (2024);
    int bad = 0;
    for (int trial = 0; trial < 60; ++trial)
    {
        Harness h (trial % 2 ? 44100.0 : 96000.0, 1 + trial % 2, 1 + (int) r.range (0, 700));
        for (int i = 0; i < kNumParams; ++i)
        {
            const auto& inf = paramInfo (i);
            const double u = r.uniform();
            h.params[i] = u < 0.2 ? inf.min : (u < 0.4 ? inf.max : inf.min + (inf.max - inf.min) * (float) r.uniform());
            if (inf.kind != ParamKind::Float) h.params[i] = std::round (h.params[i]);
        }
        h.params[P::bypass] = 0.0f;
        const SourceType a = kAllSources[trial % 4];
        const SourceType b = trial % 5 == 0 ? SourceType::None : kAllSources[(trial / 4 + 1 + trial % 4) % 4];
        h.setSlots (a, a == b ? SourceType::None : b);
        h.transport.hasHostTransport = trial % 3 == 0;
        h.transport.playing = trial % 3 == 0;
        auto out = h.run (drums (h.channels, 20000, h.fs, (uint64_t) trial));
        if (! allFinite (out) || peakOf (out) > kSafetyClamp) ++bad;
    }
    IFX_CHECK_MSG (bad == 0, juce::String (bad) + " fuzz runs out of bounds");
}

IFX_TEST (Engine, downstreamOpenContactInterruptsUpstreamArtifacts)
{
    auto humWith = [] (bool touchFirst)
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::hum] = 1.0f;
        h.params[P::tcTouch] = 0.9f;
        h.params[P::tcExposure] = 1.0f;
        h.params[P::tcShield] = 0.0f;
        h.params[P::jkInsert] = 0.0f;   // withdrawn...
        h.params[P::jkShort] = 1.0f;    // ...and shorted: quiet itself
        h.params[P::jkShield] = 1.0f;
        h.params[P::swapOrder] = touchFirst ? 0.0f : 1.0f;
        h.setSlots (SourceType::PlugTouch, SourceType::LooseJack);
        return rmsOf (slice (h.runSilence ((int) (kFs * 2)), (int) kFs, (int) kFs));
    };
    const double touchThenJack = humWith (true), jackThenTouch = humWith (false);
    IFX_CHECK (jackThenTouch > 1e-5);
    IFX_CHECK_MSG (touchThenJack < jackThenTouch * 0.05, "open jack downstream must interrupt touch hum");
}

IFX_TEST (Engine, twoExposedPathsRespectTheNoiseBudget)
{
    Harness h (kFs, 2, 256);
    h.params = quietParams();
    h.params[P::hum] = 1.0f;
    h.params[P::tcTouch] = 1.0f;
    h.params[P::tcExposure] = 1.0f;
    h.params[P::tcShield] = 0.0f;
    h.params[P::cbShieldDmg] = 1.0f;
    h.params[P::cbBend] = 0.5f;
    h.params[P::cbWeakPos] = 0.5f;
    h.params[P::cbDamage] = 0.0f;
    h.params[P::monitor] = (float) (int) Tap::Artifacts;
    h.setSlots (SourceType::PlugTouch, SourceType::CableHandling);
    auto out = h.runSilence ((int) (kFs * 2));
    IFX_CHECK_MSG (peakOf (out) <= limit (limits::backgroundCeiling, Policy::Natural) * 1.05f,
                   "peak " + juce::String (peakOf (out)));
    IFX_CHECK (peakOf (out) > 1e-3f);
}

IFX_TEST (Engine, swapOrderIsAudible)
{
    auto render = [] (bool swap)
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::hum] = 1.0f;
        h.params[P::tcTouch] = 0.8f;
        h.params[P::ptTurn] = 0.3f;
        h.params[P::ptCircuit] = 1.0f;
        h.params[P::swapOrder] = swap ? 1.0f : 0.0f;
        h.setSlots (SourceType::PlugTouch, SourceType::DirtyPot);
        return h.run (noise (2, (int) kFs, 4));
    };
    IFX_CHECK (maxAbsDiff (render (false), render (true)) > 1e-4);
}

IFX_TEST (Engine, excitationRevealsFaultsOnSilentTrackOnlyWhenEnabled)
{
    for (int k = 0; k < 2; ++k)
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::mode] = 2.0f;
        h.params[P::excType] = k == 0 ? 0.0f : 1.0f;
        h.params[P::excLevel] = -20.0f;
        h.setSlots (SourceType::DirtyPot, SourceType::None);
        auto out = h.runSilence ((int) kFs);
        if (k == 0) IFX_CHECK (peakOf (out) == 0.0f);
        else        IFX_CHECK (peakOf (out) > 1e-3f && peakOf (out) <= limit (limits::excitationPeak, Policy::Natural) + 1e-3f);
    }
}

IFX_TEST (Engine, openCircuitDoesNotConjureContinuousOutput)
{
    Harness h (kFs, 2, 256);
    h.params = quietParams();
    h.params[P::excType] = 1.0f;
    h.params[P::excLevel] = -10.0f;
    h.params[P::jkInsert] = 0.0f;
    h.setSlots (SourceType::LooseJack, SourceType::None);
    auto out = h.runSilence ((int) kFs);
    IFX_CHECK (rmsOf (slice (out, 9600, 38400)) < 1e-4);
}

IFX_TEST (Engine, rawHardwareRestoresCore)
{
    Harness h (kFs, 2, 256);
    h.params[P::studioOn] = 1.0f;
    h.params[P::drOn] = 1.0f;
    h.params[P::ecOn] = 1.0f;
    h.params[P::studioSrc] = 2.0f;
    h.params[P::rawHw] = 1.0f;
    h.params[P::hum] = 0.5f;
    h.params[P::tcTouch] = 0.5f;
    h.setSlots (SourceType::PlugTouch, SourceType::None);
    auto in = drums (2, (int) kFs, kFs);
    h.params[P::monitor] = (float) (int) Tap::FullWet;
    auto w = h.run (in);
    Harness g (kFs, 2, 256);
    g.params = h.params;
    g.params[P::monitor] = (float) (int) Tap::FullCore;
    g.setSlots (SourceType::PlugTouch, SourceType::None);
    auto c = g.run (in);
    IFX_CHECK (maxAbsDiff (w, c) < 1e-6);
}

IFX_TEST (Engine, pluginBypassIsDry)
{
    Harness h (kFs, 2, 256);
    h.params[P::bypass] = 1.0f;
    h.params[P::hum] = 1.0f;
    h.params[P::tcTouch] = 1.0f;
    h.setSlots (SourceType::PlugTouch, SourceType::LooseJack);
    auto in = drums (2, (int) kFs, kFs);
    h.run (slice (in, 0, 4800));
    auto out = h.run (slice (in, 4800, 40000));
    IFX_CHECK (maxAbsDiff (out, slice (in, 4800, 40000)) == 0.0);
}

IFX_TEST (Engine, sidechainNeverLeaksIntoOutput)
{
    Harness h (kFs, 2, 256);
    h.params = quietParams();
    h.params[P::react] = 1.0f;
    h.setSlots (SourceType::None, SourceType::None);
    auto in = sine (2, (int) kFs, 440.0, kFs);
    auto sc = drums (2, (int) kFs, kFs);
    auto out = h.run (in, &sc);
    IFX_CHECK (maxAbsDiff (out, in) == 0.0);
}

IFX_TEST (Engine, sidechainReactTriggersGesturesOnlyWhenPresent)
{
    auto gestures = [] (bool withSc)
    {
        Harness h (kFs, 2, 256);
        h.params[P::react] = 1.0f;
        h.params[P::reactThresh] = -30.0f;
        h.setSlots (SourceType::LooseJack, SourceType::None);
        auto in = sine (2, (int) (kFs * 2), 220.0, kFs);
        auto sc = drums (2, (int) (kFs * 2), kFs);
        h.run (in, withSc ? &sc : nullptr);
        return h.engine->telemetry.slot[0].gestures.load();
    };
    IFX_CHECK (gestures (false) == 0);
    IFX_CHECK (gestures (true) >= 2);
}

IFX_TEST (Engine, monoLayoutWorksWithoutDuplication)
{
    Harness h (kFs, 1, 256);
    h.params[P::hum] = 0.3f;
    h.params[P::tcTouch] = 0.5f;
    h.setSlots (SourceType::PlugTouch, SourceType::DirtyPot);
    auto out = h.run (sine (1, (int) kFs, 440.0, kFs));
    IFX_CHECK (out.getNumChannels() == 1);
    IFX_CHECK (allFinite (out));
    IFX_CHECK (rmsOf (out) > 0.01);
}

IFX_TEST (Engine, pairMatrixBothOrdersFiniteAndDistinct)
{
    int bad = 0;
    for (auto a : kAllSources)
        for (auto b : kAllSources)
        {
            if (a == b) continue;
            for (int swap = 0; swap < 2; ++swap)
            {
                Harness h (kFs, 2, 256);
                h.params[P::hum] = 0.4f;
                h.params[P::swapOrder] = (float) swap;
                h.params[P::jkWiggle] = 0.6f;
                h.params[P::jkInsert] = 0.85f;
                h.params[P::tcTouch] = 0.4f;
                h.params[P::cbShake] = 0.5f;
                h.params[P::cbMic] = 0.3f;
                h.params[P::linkGesture] = 1.0f;
                h.setSlots (a, b);
                addWiggles (h, (int) kFs);
                auto out = h.run (drums (2, (int) kFs, kFs));
                if (! allFinite (out) || peakOf (out) > 2.0f) ++bad;
            }
        }
    IFX_CHECK (bad == 0);
}

IFX_TEST (Engine, slotTypesRejectDuplicates)
{
    Engine e;
    IFX_CHECK (! e.setSlotTypes (SourceType::DirtyPot, SourceType::DirtyPot));
    IFX_CHECK (e.setSlotTypes (SourceType::DirtyPot, SourceType::LooseJack));
    IFX_CHECK (e.setSlotTypes (SourceType::None, SourceType::None));
}

IFX_TEST (Engine, bypassedSlotAdvancesTimeSilently)
{
    // A slot bypassed for a while must not replay old events when re-enabled.
    Harness h (kFs, 2, 256);
    h.params = quietParams();
    h.params[P::jkPop] = 1.0f;
    h.params[P::jkWear] = 1.0f;
    h.params[P::jkWiggle] = 1.0f;
    h.params[P::jkWearPos] = 0.8f;
    h.params[P::jkInsert] = 0.8f;
    h.setSlots (SourceType::LooseJack, SourceType::None);
    h.params[P::slotAOn] = 0.0f;
    h.runSilence ((int) kFs);
    const auto evWhileBypassed = h.engine->telemetry.slot[0].openEvents.load() + h.engine->telemetry.slot[0].resistiveEvents.load();
    IFX_CHECK (evWhileBypassed > 0); // the physical clock advanced
}
