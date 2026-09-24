// One test per PhysicalLimits bound that no other test exercises (spec audit R2.12):
// maxResistiveMs, humPeak, scratchGain, mechanicalPeak, injectionBandwidth, gestureRate.
// maxEventRate, maxOpenMs, maxMotionSpeed / maxAcceleration and minRetriggerMs are covered in
// TestCore.cpp; popPeak, backgroundCeiling, noiseBudget and excitationPeak in TestEngine.cpp /
// TestSources.cpp. Every test asserts the audible or physical behaviour, and where the bound
// can actually be reached it also checks that it is (so a cap that is never exercised fails).
#include "TestHelpers.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
constexpr double kFs = 48000.0;

const char* policyName (Policy p) { return p == Policy::Natural ? "Natural" : "Expressive"; }

/** Process `in` through the harness in chunks, calling `before (pos)` ahead of each chunk
    (parameter automation at chunk rate). */
template <typename Fn>
juce::AudioBuffer<float> runAutomated (Harness& h, const juce::AudioBuffer<float>& in, int chunk, Fn&& before)
{
    const int n = in.getNumSamples();
    juce::AudioBuffer<float> out (h.channels, n);
    for (int pos = 0; pos < n; pos += chunk)
    {
        const int len = std::min (chunk, n - pos);
        before (pos);
        auto o = h.run (slice (in, pos, len));
        for (int ch = 0; ch < h.channels; ++ch)
            out.copyFrom (ch, pos, o, ch, 0, len);
    }
    return out;
}

double rmsOfDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double s = 0.0;
    int n = 0;
    for (int ch = 0; ch < std::min (a.getNumChannels(), b.getNumChannels()); ++ch)
        for (int i = 0; i < std::min (a.getNumSamples(), b.getNumSamples()); ++i)
        {
            const double d = (double) a.getSample (ch, i) - (double) b.getSample (ch, i);
            s += d * d;
            ++n;
        }
    return n > 0 ? std::sqrt (s / n) : 0.0;
}
}

//==============================================================================
// maxResistiveMs: a resistive excursion (partial loss) never lasts longer than the cap.
IFX_TEST (Limits, resistiveExcursionDurationIsCapped)
{
    for (auto pol : { Policy::Natural, Policy::Expressive })
    {
        ContactModel c;
        c.prepare (kFs);
        c.seed (21);
        ContactModel::Inputs in;
        in.baseline = 1.0f;
        in.hazard = 40.0f;          // a badly worn contact that is moving
        in.openProbability = 0.0f;  // every excursion stays resistive (never opens)
        in.recovery = 0.0f;         // slowest recovery = the longest excursions
        in.intermittency = 0.0f;
        in.depthScale = 1.0f;
        int run = 0, longest = 0;
        for (int i = 0; i < (int) kFs * 20; ++i)
        {
            c.step (in, pol);
            if (c.getState() == ContactModel::State::Resistive) longest = std::max (longest, ++run);
            else run = 0;
        }
        const int cap = (int) (kFs * limit (limits::maxResistiveMs, pol) * 0.001f);
        IFX_CHECK (c.resistiveEvents > 50);
        IFX_CHECK (c.openEvents == 0);
        IFX_CHECK_MSG (longest <= cap + 2, juce::String (policyName (pol)) + ": longest resistive run "
                           + juce::String (longest) + " samples, cap " + juce::String (cap));
        // the lognormal dwell regularly asks for more than 40 ms: the Natural cap must bind
        if (pol == Policy::Natural)
            IFX_CHECK_MSG (longest >= (int) (cap * 0.8), "cap never reached: longest " + juce::String (longest));
    }
}

//==============================================================================
// humPeak: one fully exposed path never picks up more mains than the cap, and does get close.
IFX_TEST (Limits, fullyExposedHumPickupStaysUnderHumPeak)
{
    enum Config { OpenJack = 0, TouchedTip, BrokenShield };
    float jackPeak[2] {};
    for (auto pol : { Policy::Natural, Policy::Expressive })
        for (int cfg = 0; cfg < 3; ++cfg)
        {
            Harness h (kFs, 2, 256);
            h.params = quietParams();
            h.params[P::policy] = (float) (int) pol;
            h.params[P::hum] = 1.0f;
            h.params[P::stereo] = 0.0f;
            h.params[P::monitor] = (float) (int) Tap::Artifacts;
            SourceType src = SourceType::LooseJack;
            if (cfg == OpenJack)
            {
                h.params[P::jkInsert] = 0.0f;   // withdrawn: the input floats
                h.params[P::jkWiggle] = 0.0f;
                h.params[P::jkShield] = 0.0f;
                h.params[P::jkShort] = 0.0f;
            }
            else if (cfg == TouchedTip)
            {
                src = SourceType::PlugTouch;
                h.params[P::inputMode] = 1.0f;  // a high-impedance (passive) source: the node is not held down
                h.params[P::tcTouch] = 1.0f;    // whole finger, firm, unshielded, unterminated
                h.params[P::tcPressure] = 1.0f;
                h.params[P::tcExposure] = 1.0f;
                h.params[P::tcShield] = 0.0f;
                h.params[P::tcBody] = 1.0f;
            }
            else
            {
                src = SourceType::CableHandling;
                h.params[P::inputMode] = 1.0f;  // passive source drives the exposed conductor
                h.params[P::cbShieldDmg] = 1.0f; // braid gone right where the cable is bent
                h.params[P::cbDamage] = 0.0f;
                h.params[P::cbBend] = 0.5f;
                h.params[P::cbWeakPos] = 0.5f;
                h.params[P::cbShake] = 0.0f;
            }
            h.setSlots (src, SourceType::None);
            const auto out = h.runSilence ((int) kFs);
            const float pk = peakOf (slice (out, (int) (kFs * 0.2), (int) (kFs * 0.8)));
            const float cap = limit (limits::humPeak, pol);
            IFX_CHECK (allFinite (out));
            IFX_CHECK_MSG (pk <= cap * 1.02f, juce::String (policyName (pol)) + " config " + juce::String (cfg)
                                                + ": hum peak " + juce::String (pk) + " cap " + juce::String (cap));
            if (cfg == OpenJack)
            {
                IFX_CHECK_MSG (pk >= cap * 0.3f, "a withdrawn, unshielded plug should pick up close to the cap: " + juce::String (pk));
                jackPeak[(int) pol] = pk;
            }
            else
                IFX_CHECK (pk > 1.0e-4f);
        }
    // Expressive really is a wider range, not the same cap
    IFX_CHECK_MSG (jackPeak[1] > jackPeak[0] * 1.5f, juce::String (jackPeak[0]) + " vs " + juce::String (jackPeak[1]));
}

//==============================================================================
// scratchGain: the dirty-wiper modulation of the through signal stays within the cap (RMS
// depth relative to the same take without scratch) and is clearly present while turning.
IFX_TEST (Limits, scratchModulationDepthStaysUnderScratchGain)
{
    double ratio[2] {};
    for (auto pol : { Policy::Natural, Policy::Expressive })
    {
        auto render = [pol] (float scratch)
        {
            Harness h (kFs, 2, 256);
            h.params = quietParams();
            h.params[P::policy] = (float) (int) pol;
            h.params[P::ptCircuit] = 0.0f;   // Level
            h.params[P::ptTaper] = 0.0f;     // Linear: the through signal stays large over the sweep
            h.params[P::ptDirt] = 1.0f;
            h.params[P::ptWearPos] = 0.5f;
            h.params[P::ptWearWidth] = 0.2f;
            h.params[P::ptScratch] = scratch;
            h.params[P::ptVibration] = 0.0f;
            h.params[P::monitor] = (float) (int) Tap::FullCore;
            h.setSlots (SourceType::DirtyPot, SourceType::None);
            const auto in = sine (2, (int) (kFs * 3), 330.0, kFs, 0.5f);
            // turn the knob back and forth across the worn arc: 0.3 .. 0.7 .. 0.3 once a second
            return runAutomated (h, in, 256, [&h] (int pos)
            {
                const double t = pos / kFs;
                const double tri = 2.0 * std::abs (t - std::floor (t + 0.5));
                h.params[P::ptTurn] = (float) (0.3 + 0.4 * tri);
            });
        };
        // Scratch changes only the scratch: the contact stream, motion and taper are identical,
        // so the difference is exactly the scratch modulation of the through signal.
        const auto clean = render (0.0f), scratched = render (1.0f);
        const double r = rmsOfDifference (scratched, clean) / std::max (1.0e-9, rmsOf (clean));
        const float cap = limit (limits::scratchGain, pol);
        IFX_CHECK (allFinite (scratched));
        IFX_CHECK_MSG (r <= cap, juce::String (policyName (pol)) + ": scratch depth " + juce::String (r, 4)
                                     + " exceeds cap " + juce::String (cap));
        IFX_CHECK_MSG (r >= 0.05 * cap, juce::String (policyName (pol)) + ": scratch barely present " + juce::String (r, 4));
        ratio[(int) pol] = r;
    }
    IFX_CHECK_MSG (ratio[1] > ratio[0], "Expressive scratch should be deeper: " + juce::String (ratio[0], 4)
                                            + " vs " + juce::String (ratio[1], 4));
}

//==============================================================================
// mechanicalPeak: cable handling (microphonic path) never exceeds the cap, even under hard
// taps and shaking; the cap is reached by real taps.
IFX_TEST (Limits, handlingNoiseStaysUnderMechanicalPeak)
{
    for (auto pol : { Policy::Natural, Policy::Expressive })
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::policy] = (float) (int) pol;
        h.params[P::cbMic] = 1.0f;
        h.params[P::cbShake] = 1.0f;
        h.params[P::cbDamage] = 0.0f;     // electrically sound: only the mechanical path speaks
        h.params[P::cbShieldDmg] = 0.0f;
        h.params[P::monitor] = (float) (int) Tap::Artifacts;
        h.setSlots (SourceType::CableHandling, SourceType::None);
        for (int k = 0; k < 20; ++k)      // hard taps every 150 ms (above the retrigger spacing)
            h.pendingEvents.push_back ({ (int) (k * 0.15 * kFs), ControlEvent::GestureA, (int) GestureKind::Tap, 1.0f });
        const auto out = h.runSilence ((int) (kFs * 3));
        const float cap = limit (limits::mechanicalPeak, pol);
        IFX_CHECK (allFinite (out));
        IFX_CHECK (h.engine->telemetry.slot[0].mechEvents.load() > 0);
        IFX_CHECK_MSG (peakOf (out) <= cap * 1.0001f + 1.0e-6f, juce::String (policyName (pol)) + ": handling peak "
                                                                   + juce::String (peakOf (out)) + " cap " + juce::String (cap));
        if (pol == Policy::Natural)
            IFX_CHECK_MSG (peakOf (out) >= cap * 0.15f, "hard taps should approach the cap: " + juce::String (peakOf (out)));
    }
}

//==============================================================================
// injectionBandwidth: injected artifacts are low-passed at the policy's bandwidth. The same
// RF chatter reaches the A bus in both policies (only the gain and the injection low-pass
// differ), so the high/low band ratio isolates the low-pass: Natural (9 kHz) must keep
// clearly less energy around 20 kHz than Expressive (16 kHz).
IFX_TEST (Limits, injectedArtifactsAreBandLimitedByThePolicy)
{
    const double fs = 96000.0;
    double ratio[2] {};
    for (auto pol : { Policy::Natural, Policy::Expressive })
    {
        Harness h (fs, 1, 256);
        h.params = quietParams();
        h.params[P::policy] = (float) (int) pol;
        h.params[P::rf] = 1.0f;           // broadband demodulated chatter from a transmitter
        h.params[P::stereo] = 0.0f;
        h.params[P::jkInsert] = 0.0f;     // withdrawn, unshielded: the chatter is injected
        h.params[P::jkWiggle] = 0.0f;
        h.params[P::jkShield] = 0.0f;
        h.params[P::jkShort] = 0.0f;
        h.params[P::monitor] = (float) (int) Tap::Artifacts;
        h.setSlots (SourceType::LooseJack, SourceType::None);
        const auto out = h.runSilence ((int) (fs * 3.0));   // keyed traffic: long enough to catch transmissions
        const double lf = bandEnergy (out, fs, 500.0, 2000.0);
        const double hf = bandEnergy (out, fs, 16000.0, 24000.0);
        IFX_CHECK (allFinite (out));
        IFX_CHECK_MSG (lf > 0.0, juce::String (policyName (pol)) + ": no chatter reached the A bus");
        IFX_CHECK (hf < lf);
        ratio[(int) pol] = hf / std::max (1.0e-30, lf);
    }
    IFX_CHECK_MSG (ratio[0] < 0.7 * ratio[1], "Natural HF/LF " + juce::String (ratio[0], 6)
                                                  + " vs Expressive " + juce::String (ratio[1], 6));
}

//==============================================================================
// gestureRate: a flood of MIDI triggers can never start more gestures per second than the
// cap (the rest are coalesced), and the flood is not simply ignored.
IFX_TEST (Limits, gestureStartsNeverExceedGestureRate)
{
    for (auto pol : { Policy::Natural, Policy::Expressive })
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::policy] = (float) (int) pol;
        h.setSlots (SourceType::LooseJack, SourceType::None);
        const double seconds = 4.0;
        for (int i = 0; i < (int) (seconds * 200.0); ++i)   // a note every 5 ms
            h.pendingEvents.push_back ({ i * (int) (kFs / 200.0), ControlEvent::NoteOn, 36, 1.0f });
        const auto out = h.runSilence ((int) (kFs * seconds));
        const auto started = (double) h.engine->telemetry.slot[0].gestures.load();
        const double rate = limit (limits::gestureRate, pol);
        const double spacing = limit (limits::minRetriggerMs, pol) * 0.001;
        IFX_CHECK (allFinite (out));
        IFX_CHECK_MSG (started <= rate * seconds + 1.0, juce::String (policyName (pol)) + ": " + juce::String (started)
                                                            + " gestures in " + juce::String (seconds) + " s, cap "
                                                            + juce::String (rate) + "/s");
        IFX_CHECK_MSG (started >= 0.5 * std::min (rate, 1.0 / spacing) * seconds,
                       juce::String (policyName (pol)) + ": only " + juce::String (started) + " gestures started");
        IFX_CHECK (h.engine->gesture (0).coalesced > 0);
    }
}
