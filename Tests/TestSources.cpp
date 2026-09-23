// One test per row of the realism-constraint tables (realism spec sections 3-6),
// asserting both audible properties and state/event telemetry.
#include "TestHelpers.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
constexpr double kFs = 48000.0;

struct Single
{
    Harness h { kFs, 2, 256 };
    explicit Single (SourceType t)
    {
        h.params = quietParams();
        h.setSlots (t, SourceType::None);
    }
    const SlotTelemetry& tele() { return h.engine->telemetry.slot[0]; }
    uint32_t events() { return tele().openEvents.load() + tele().resistiveEvents.load(); }
};

/** Linear parameter sweep between two values over `seconds`, processed in 10 ms chunks. */
juce::AudioBuffer<float> sweep (Harness& h, int param, float from, float to, double seconds, const juce::AudioBuffer<float>* src = nullptr)
{
    const int total = (int) (seconds * h.fs);
    const int chunk = (int) (h.fs * 0.01);
    juce::AudioBuffer<float> out (h.channels, total);
    for (int pos = 0; pos < total; pos += chunk)
    {
        const int len = std::min (chunk, total - pos);
        h.params[param] = from + (to - from) * (float) pos / (float) total;
        juce::AudioBuffer<float> in (h.channels, len);
        if (src != nullptr)
            for (int ch = 0; ch < h.channels; ++ch)
                in.copyFrom (ch, 0, *src, ch, pos % std::max (1, src->getNumSamples() - len), len);
        else
            in.clear();
        auto o = h.run (in);
        for (int ch = 0; ch < h.channels; ++ch)
            out.copyFrom (ch, 0, o, ch, 0, len);
    }
    return out;
}

double rmsRatio (const juce::AudioBuffer<float>& out, const juce::AudioBuffer<float>& in)
{
    return rmsOf (out) / std::max (1e-12, rmsOf (in));
}

/** Energy near a frequency (narrow Goertzel). */
double tone (const juce::AudioBuffer<float>& b, double f, double fs)
{
    return bandEnergy (b, fs, f * 0.98, f * 1.02, 3);
}
}

// =============================================================================
// Loose Jack
// =============================================================================
IFX_TEST (LooseJack, seatedHealthyIsNearTransparent)
{
    Single s (SourceType::LooseJack);
    s.h.params[P::jkInsert] = 1.0f;
    s.h.params[P::jkWiggle] = 0.1f;
    auto in = sine (2, (int) (kFs * 5), 440.0, kFs);
    auto out = s.h.run (in);
    IFX_CHECK (s.tele().openEvents.load() <= 1);
    IFX_CLOSE (rmsRatio (out, in), 1.0, 0.03);
}

IFX_TEST (LooseJack, unstableZoneMovementMakesClusteredFaults)
{
    Single s (SourceType::LooseJack);
    s.h.params[P::jkWiggle] = 0.6f;
    s.h.params[P::jkWear] = 0.6f;
    s.h.params[P::jkWearPos] = 0.7f;
    auto src = sine (2, (int) kFs, 220.0, kFs);
    for (int r = 0; r < 3; ++r)
    {
        sweep (s.h, P::jkInsert, 1.0f, 0.6f, 1.0, &src);
        sweep (s.h, P::jkInsert, 0.6f, 1.0f, 1.0, &src);
    }
    IFX_CHECK_MSG (s.events() > 5, "events " + juce::String ((int) s.events()));
}

IFX_TEST (LooseJack, withdrawnPlugLosesThroughSignal)
{
    Single s (SourceType::LooseJack);
    s.h.params[P::jkInsert] = 0.0f;
    auto in = sine (2, (int) kFs, 440.0, kFs);
    auto out = s.h.run (in);
    IFX_CHECK (rmsRatio (slice (out, 4800, 40000), slice (in, 4800, 40000)) < 0.01);
}

IFX_TEST (LooseJack, withdrawnPlugHumDependsOnShielding)
{
    double humE[2];
    for (int k = 0; k < 2; ++k)
    {
        Single s (SourceType::LooseJack);
        s.h.params[P::hum] = 1.0f;
        s.h.params[P::jkInsert] = 0.0f;
        s.h.params[P::jkShield] = k == 0 ? 0.0f : 1.0f;
        auto out = s.h.runSilence ((int) kFs);
        humE[k] = tone (slice (out, 9600, 38400), 50.0, kFs);
    }
    IFX_CHECK (humE[0] > 1e-8);
    IFX_CHECK (humE[1] < humE[0] * 0.05);
}

IFX_TEST (LooseJack, wornLocationFailsMoreThanPristine)
{
    uint32_t ev[2];
    for (int k = 0; k < 2; ++k)
    {
        Single s (SourceType::LooseJack);
        s.h.params[P::jkInsert] = 0.95f;
        s.h.params[P::jkWiggle] = 0.9f;
        s.h.params[P::jkWear] = 1.0f;
        s.h.params[P::jkPressure] = 0.3f;
        s.h.params[P::jkWearPos] = k == 0 ? 0.95f : 0.3f;
        s.h.run (sine (2, (int) (kFs * 6), 220.0, kFs));
        ev[k] = s.events();
    }
    IFX_CHECK_MSG (ev[0] > 3 * ev[1] + 2, "worn " + juce::String ((int) ev[0]) + " pristine " + juce::String ((int) ev[1]));
}

IFX_TEST (LooseJack, shortToSleeveIsQuieterThanOpen)
{
    double e[2];
    for (int k = 0; k < 2; ++k)
    {
        Single s (SourceType::LooseJack);
        s.h.params[P::hum] = 1.0f;
        s.h.params[P::jkShield] = 0.0f;
        s.h.params[P::jkShort] = k == 0 ? 0.0f : 1.0f;
        s.h.params[P::jkWear] = 1.0f;
        s.h.params[P::jkWearPos] = 0.7f;
        s.h.params[P::jkWiggle] = 1.0f;
        s.h.params[P::jkPressure] = 0.0f;
        s.h.params[P::jkIntermit] = 1.0f;
        s.h.params[P::jkInsert] = 0.7f;
        auto out = s.h.runSilence ((int) (kFs * 4));
        e[k] = rmsOf (out);
    }
    IFX_CHECK_MSG (e[1] < e[0] * 0.7, "open " + juce::String (e[0]) + " short " + juce::String (e[1]));
}

IFX_TEST (LooseJack, popNeedsBias)
{
    for (int k = 0; k < 2; ++k)
    {
        Single s (SourceType::LooseJack);
        s.h.params[P::jkPop] = k == 0 ? 0.0f : 1.0f;
        s.h.params[P::jkWear] = 1.0f;
        s.h.params[P::jkWiggle] = 1.0f;
        s.h.params[P::jkWearPos] = 0.9f;
        s.h.params[P::jkPressure] = 0.0f;
        auto out = sweep (s.h, P::jkInsert, 1.0f, 0.5f, 2.0);
        IFX_CHECK (s.events() > 0);
        if (k == 0) IFX_CHECK_MSG (peakOf (out) < 1e-6f, "silent circuit without bias must stay silent");
        else        IFX_CHECK (peakOf (out) > 1e-3f && peakOf (out) <= limit (limits::popPeak, Policy::Natural) + 1e-3f);
    }
}

// =============================================================================
// Exposed Plug Touch
// =============================================================================
IFX_TEST (PlugTouch, touchCouplesMainsNotBroadband)
{
    Single s (SourceType::PlugTouch);
    s.h.params[P::hum] = 1.0f;
    s.h.params[P::tcTouch] = 0.8f;
    s.h.params[P::tcPressure] = 1.0f;
    auto out = s.h.runSilence ((int) (kFs * 2));
    auto tail = slice (out, (int) kFs, (int) kFs);
    const double mains = tone (tail, 50.0, kFs) + tone (tail, 150.0, kFs);
    const double hf = bandEnergy (tail, kFs, 9000.0, 20000.0, 12);
    IFX_CHECK (mains > 1e-7);
    IFX_CHECK (hf < mains * 1e-3);
    // 60 Hz mains moves the fundamental
    Single t (SourceType::PlugTouch);
    t.h.params = s.h.params;
    t.h.params[P::mains] = 1.0f;
    auto o60 = slice (t.h.runSilence ((int) (kFs * 2)), (int) kFs, (int) kFs);
    IFX_CHECK (tone (o60, 60.0, kFs) > tone (o60, 50.0, kFs) * 5.0);
}

IFX_TEST (PlugTouch, shieldedTerminatedPicksUpLittle)
{
    double e[2];
    for (int k = 0; k < 2; ++k)
    {
        Single s (SourceType::PlugTouch);
        s.h.params[P::hum] = 1.0f;
        s.h.params[P::tcTouch] = 0.8f;
        s.h.params[P::tcExposure] = k == 0 ? 1.0f : 0.1f;
        s.h.params[P::tcShield] = k == 0 ? 0.0f : 1.0f;
        e[k] = rmsOf (slice (s.h.runSilence ((int) (kFs * 2)), (int) kFs, (int) kFs));
    }
    IFX_CHECK (e[1] < e[0] * 0.05);
}

IFX_TEST (PlugTouch, untouchedIsWeakerThanTouched)
{
    double e[2];
    for (int k = 0; k < 2; ++k)
    {
        Single s (SourceType::PlugTouch);
        s.h.params[P::hum] = 1.0f;
        s.h.params[P::tcTouch] = k == 0 ? 0.0f : 0.8f;
        e[k] = rmsOf (slice (s.h.runSilence ((int) (kFs * 2)), (int) kFs, (int) kFs));
    }
    IFX_CHECK (e[0] > 0.0);            // exposure-dependent ambient pickup
    IFX_CHECK (e[0] < e[1] * 0.3);
}

IFX_TEST (PlugTouch, stationaryFirmFingerMakesNoOnsetEvents)
{
    Single s (SourceType::PlugTouch);
    s.h.params[P::hum] = 1.0f;
    s.h.params[P::tcTouch] = 0.7f;
    s.h.params[P::tcPressure] = 1.0f;
    s.h.runSilence ((int) kFs);
    const auto before = s.events();
    s.h.runSilence ((int) (kFs * 3));
    IFX_CHECK (s.events() == before);
}

IFX_TEST (PlugTouch, pressAndReleaseChatterAroundTransitions)
{
    Single s (SourceType::PlugTouch);
    s.h.params[P::hum] = 1.0f;
    s.h.params[P::tcPressure] = 0.1f;
    for (int r = 0; r < 4; ++r)
    {
        sweep (s.h, P::tcTouch, 0.0f, 0.6f, 0.25);
        sweep (s.h, P::tcTouch, 0.6f, 0.0f, 0.25);
    }
    IFX_CHECK (s.events() > 0);
}

IFX_TEST (PlugTouch, humOffLeavesOnlyBriefContactEvents)
{
    Single s (SourceType::PlugTouch);
    s.h.params[P::tcPop] = 0.0f;
    s.h.params[P::tcPressure] = 0.2f;
    auto out = sweep (s.h, P::tcTouch, 0.0f, 1.0f, 1.0);
    IFX_CHECK (peakOf (out) < 1e-6f); // no hum source, no bias: touch alone is silent
}

// =============================================================================
// Cable Handling
// =============================================================================
IFX_TEST (CableHandling, soundCableWithoutMicIsSilentAndTransparent)
{
    Single s (SourceType::CableHandling);
    s.h.params[P::cbDamage] = 0.0f;
    s.h.params[P::cbShieldDmg] = 0.0f;
    s.h.params[P::cbShake] = 1.0f;
    s.h.params[P::hum] = 1.0f;
    auto in = noise (2, (int) (kFs * 2), 5);
    auto out = s.h.run (in);
    IFX_CHECK (maxAbsDiff (out, in) < 1e-6);
    IFX_CHECK (s.events() == 0);
}

IFX_TEST (CableHandling, bendOverWeakSpotFaultsMoreThanElsewhere)
{
    uint32_t ev[2];
    for (int k = 0; k < 2; ++k)
    {
        Single s (SourceType::CableHandling);
        s.h.params[P::cbDamage] = 1.0f;
        s.h.params[P::cbWeakPos] = 0.5f;
        s.h.params[P::cbShake] = 1.0f;
        s.h.params[P::cbBend] = k == 0 ? 0.5f : 0.05f;
        s.h.run (sine (2, (int) (kFs * 6), 220.0, kFs));
        ev[k] = s.events();
    }
    IFX_CHECK_MSG (ev[0] > 3 * ev[1] + 2, "weak " + juce::String ((int) ev[0]) + " elsewhere " + juce::String ((int) ev[1]));
}

IFX_TEST (CableHandling, shieldFaultAdmitsHumWhileConductorLive)
{
    Single s (SourceType::CableHandling);
    s.h.params[P::hum] = 1.0f;
    s.h.params[P::cbDamage] = 0.0f;
    s.h.params[P::cbShieldDmg] = 1.0f;
    s.h.params[P::cbBend] = 0.5f;
    s.h.params[P::cbWeakPos] = 0.5f;
    auto in = sine (2, (int) (kFs * 2), 1000.0, kFs, 0.3f);
    auto out = s.h.run (in);
    auto tail = slice (out, (int) kFs, (int) kFs);
    IFX_CHECK (tone (tail, 50.0, kFs) > 1e-8);                       // hum admitted
    IFX_CHECK (tone (tail, 1000.0, kFs) > tone (slice (in, (int) kFs, (int) kFs), 1000.0, kFs) * 0.9); // signal intact
}

IFX_TEST (CableHandling, handlingFollowsMotionAndStops)
{
    Single s (SourceType::CableHandling);
    s.h.params[P::cbDamage] = 0.0f;
    s.h.params[P::cbShieldDmg] = 0.0f;
    s.h.params[P::cbMic] = 1.0f;
    s.h.params[P::cbShake] = 0.3f;
    auto slow = s.h.runSilence ((int) (kFs * 2));
    s.h.params[P::cbShake] = 1.0f;
    auto fast = s.h.runSilence ((int) (kFs * 2));
    IFX_CHECK (rmsOf (fast) > rmsOf (slow));
    s.h.params[P::cbShake] = 0.0f;
    s.h.runSilence ((int) (kFs * 1));
    auto after = s.h.runSilence ((int) (kFs * 1));
    IFX_CHECK (peakOf (after) < 1e-4f); // motion stopped: only a short resonant tail, then quiet
}

IFX_TEST (CableHandling, micOffMeansNoHandlingSound)
{
    Single s (SourceType::CableHandling);
    s.h.params[P::cbDamage] = 0.0f;
    s.h.params[P::cbShieldDmg] = 0.0f;
    s.h.params[P::cbMic] = 0.0f;
    s.h.params[P::cbShake] = 1.0f;
    s.h.pendingEvents.push_back ({ 1000, ControlEvent::GestureA, (int) GestureKind::Tap, 1.0f });
    IFX_CHECK (peakOf (s.h.runSilence ((int) (kFs * 2))) < 1e-7f);
}

IFX_TEST (CableHandling, tapIsShortAndDecays)
{
    Single s (SourceType::CableHandling);
    s.h.params[P::cbDamage] = 0.0f;
    s.h.params[P::cbShieldDmg] = 0.0f;
    s.h.params[P::cbMic] = 1.0f;
    s.h.pendingEvents.push_back ({ 1000, ControlEvent::GestureA, (int) GestureKind::Tap, 1.0f });
    auto out = s.h.runSilence ((int) kFs);
    IFX_CHECK (peakOf (slice (out, 1000, 4800)) > 1e-3f);
    IFX_CHECK (peakOf (slice (out, 30000, 18000)) < peakOf (slice (out, 1000, 4800)) * 0.01f);
}

// =============================================================================
// Dirty Pot
// =============================================================================
IFX_TEST (DirtyPot, cleanPotFollowsTaperWithoutScratch)
{
    Single s (SourceType::DirtyPot);
    s.h.params[P::ptDirt] = 0.0f;
    s.h.params[P::ptTaper] = 1.0f; // log
    s.h.params[P::ptTurn] = 0.5f;
    auto in = sine (2, (int) kFs, 440.0, kFs);
    auto out = s.h.run (in);
    IFX_CLOSE (rmsRatio (slice (out, 4800, 40000), slice (in, 4800, 40000)), 0.1, 0.005);
    s.h.captureTap = Tap::FaultDifference;
    auto src = sine (2, (int) kFs, 440.0, kFs);
    sweep (s.h, P::ptTurn, 0.1f, 0.9f, 1.0, &src);
    IFX_CHECK (s.events() == 0);
}

IFX_TEST (DirtyPot, turningThroughWornArcScratchesThere)
{
    auto measure = [] (float from, float to)
    {
        Single s (SourceType::DirtyPot);
        s.h.params[P::ptDirt] = 1.0f;
        s.h.params[P::ptWearPos] = 0.5f;
        s.h.params[P::ptWearWidth] = 0.08f;
        s.h.params[P::ptScratch] = 1.0f;
        s.h.params[P::ptTaper] = 0.0f;
        auto src = sine (2, (int) kFs, 330.0, kFs);
        for (int r = 0; r < 4; ++r)
        {
            sweep (s.h, P::ptTurn, from, to, 0.5, &src);
            sweep (s.h, P::ptTurn, to, from, 0.5, &src);
        }
        return s.events();
    };
    const auto worn = measure (0.4f, 0.6f), clean = measure (0.85f, 1.0f);
    IFX_CHECK_MSG (worn > 3 * clean + 2, "worn " + juce::String ((int) worn) + " clean " + juce::String ((int) clean));
}

IFX_TEST (DirtyPot, heldStillStopsScratch)
{
    Single s (SourceType::DirtyPot);
    s.h.params[P::ptDirt] = 0.6f;
    s.h.params[P::ptWearPos] = 0.5f;
    s.h.params[P::ptScratch] = 1.0f;
    auto src = sine (2, (int) kFs, 330.0, kFs);
    sweep (s.h, P::ptTurn, 0.3f, 0.5f, 0.5, &src);
    s.h.run (sine (2, (int) (kFs * 0.5), 330.0, kFs));   // settle
    const auto before = s.events();
    s.h.captureTap = Tap::FaultDifference;
    s.h.run (sine (2, (int) (kFs * 3), 330.0, kFs), nullptr, true);
    IFX_CHECK (s.events() == before);
    // D is a stable (static) resistive error: no noise, i.e. D is just a scaled copy of R
    auto d = s.h.capture;
    auto dS = slice (d, 0, 48000);
    const double hf = bandEnergy (dS, kFs, 3000.0, 15000.0, 10);
    IFX_CHECK (hf < 1e-9);
}

IFX_TEST (DirtyPot, severeDirtUnderVibrationMayStayIntermittent)
{
    Single s (SourceType::DirtyPot);
    s.h.params[P::ptDirt] = 1.0f;
    s.h.params[P::ptWearPos] = 0.5f;
    s.h.params[P::ptTurn] = 0.5f;
    s.h.params[P::ptVibration] = 1.0f;
    s.h.run (sine (2, (int) (kFs * 4), 330.0, kFs));
    IFX_CHECK (s.events() > 0);
    Single q (SourceType::DirtyPot);
    q.h.params = s.h.params;
    q.h.params[P::ptDirt] = 0.5f;           // normal dirt: vibration alone does nothing
    q.h.params[P::ptVibration] = 1.0f;
    q.h.run (sine (2, (int) (kFs * 4), 330.0, kFs));
    IFX_CHECK (q.events() == 0);
}

IFX_TEST (DirtyPot, silentInputZeroBiasIsSilentBiasIsNot)
{
    for (int k = 0; k < 2; ++k)
    {
        Single s (SourceType::DirtyPot);
        s.h.params[P::ptDirt] = 1.0f;
        s.h.params[P::ptScratch] = 1.0f;
        s.h.params[P::ptBias] = k == 0 ? 0.0f : 1.0f;
        auto out = sweep (s.h, P::ptTurn, 0.2f, 0.8f, 1.0);
        if (k == 0) IFX_CHECK (peakOf (out) < 1e-7f);
        else        IFX_CHECK (peakOf (out) > 1e-4f);
    }
}

IFX_TEST (DirtyPot, toneCircuitFiltersNotJustLevel)
{
    double hi[2], lo[2];
    for (int k = 0; k < 2; ++k)
    {
        Single s (SourceType::DirtyPot);
        s.h.params[P::ptDirt] = 0.0f;
        s.h.params[P::ptCircuit] = 1.0f;
        s.h.params[P::ptTurn] = k == 0 ? 0.0f : 1.0f;
        auto in = noise (2, (int) kFs, 3);
        auto out = s.h.run (in);
        hi[k] = bandEnergy (out, kFs, 4000.0, 12000.0, 8);
        lo[k] = bandEnergy (out, kFs, 60.0, 200.0, 6);
    }
    IFX_CHECK (hi[0] < hi[1] * 0.1);          // turned down = treble cut
    IFX_CLOSE (lo[0] / lo[1], 1.0, 0.35);     // lows roughly untouched
}

IFX_TEST (DirtyPot, taperLawsAreMonotoneAndDistinct)
{
    for (int t = 0; t < 3; ++t)
    {
        float prev = -1.0f;
        for (int i = 0; i <= 100; ++i)
        {
            const float v = DirtyPot::taperLaw (i / 100.0f, t);
            IFX_CHECK (v >= prev);
            prev = v;
        }
        IFX_CLOSE (DirtyPot::taperLaw (0.0f, t), 0.0, 1e-6);
        IFX_CLOSE (DirtyPot::taperLaw (1.0f, t), 1.0, 1e-6);
    }
    IFX_CLOSE (DirtyPot::taperLaw (0.5f, 1), 0.1, 1e-3);
    IFX_CLOSE (DirtyPot::taperLaw (0.5f, 2), 0.9, 1e-3);
}
