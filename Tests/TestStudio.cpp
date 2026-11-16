#include "TestHelpers.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
constexpr double kFs = 48000.0;

/** Drive the Studio chain directly with a mono signal (both channels equal). */
struct Chain
{
    StudioChain c;
    ParamSnapshot p;
    StudioBlockInfo info;
    Chain() { c.prepare (kFs, 512); }
    juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& in)
    {
        juce::AudioBuffer<float> out (2, in.getNumSamples());
        for (int pos = 0; pos < in.getNumSamples(); pos += 256)
        {
            c.setParams (p, info);
            const int len = std::min (256, in.getNumSamples() - pos);
            for (int i = pos; i < pos + len; ++i)
            {
                float x[2] = { in.getSample (0, i), in.getSample (std::min (1, in.getNumChannels() - 1), i) };
                c.process (x, 2);
                out.setSample (0, i, x[0]);
                out.setSample (1, i, x[1]);
            }
            info.ppqStart += len * info.bpm / 60.0 / kFs;
        }
        return out;
    }
};

juce::AudioBuffer<float> impulse (int n, float amp = 0.5f)
{
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    b.setSample (0, 10, amp);
    b.setSample (1, 10, amp);
    return b;
}
}

IFX_TEST (Studio, everyStageBypassedIsIdentity)
{
    Chain ch;
    auto in = noise (2, 20000, 1);
    auto out = ch.run (in);
    IFX_CHECK (maxAbsDiff (in, out) == 0.0);
}

IFX_TEST (Studio, eachStageBypassIdentityAfterToggle)
{
    const int stages[] = { P::eqOn, P::ctOn, P::drOn, P::crOn, P::rsOn, P::gtOn };
    for (int s : stages)
    {
        Chain ch;
        ch.p[s] = 1.0f;
        ch.run (noise (2, 9600, 2));
        ch.p[s] = 0.0f;
        ch.run (noise (2, 9600, 3));                // ramp out
        auto in = noise (2, 9600, 4);
        auto out = ch.run (in);
        IFX_CHECK_MSG (maxAbsDiff (in, out) < 1e-5, juce::String ("stage ") + paramInfo (s).id);
    }
}

IFX_TEST (Studio, noStageAddsSoundFromSilenceWithoutFeedbackState)
{
    Chain ch;
    for (int s : { P::eqOn, P::ctOn, P::drOn, P::crOn, P::rsOn, P::gtOn, P::ecOn })
        ch.p[s] = 1.0f;
    ch.p[P::crDither] = 1.0f;
    ch.p[P::fbAmount] = 1.0f;
    ch.p[P::eqPkGain] = 12.0f;
    ch.p[P::drPre] = 36.0f;
    juce::AudioBuffer<float> z (2, 48000);
    z.clear();
    IFX_CHECK (peakOf (ch.run (z)) == 0.0f);
}

IFX_TEST (Studio, localFeedbackTailIsFiniteAndKillable)
{
    Chain ch;
    ch.p[P::drOn] = 1.0f;
    ch.p[P::drPre] = 30.0f;
    ch.p[P::drPost] = 6.0f;
    ch.p[P::rsOn] = 1.0f;
    ch.p[P::rsFeedback] = 0.9f;
    ch.p[P::rsDecay] = 3.0f;
    ch.p[P::fbAmount] = 1.0f;
    ch.p[P::fbTime] = 20.0f;
    ch.run (impulse (4800, 0.9f));
    IFX_CHECK (ch.c.effectiveFeedback() < 0.97f);
    IFX_CHECK (ch.c.feedbackLimited());
    juce::AudioBuffer<float> z (2, 48000);
    z.clear();
    for (int i = 0; i < 40; ++i) ch.run (z);   // 40 s
    auto late = ch.run (z);
    IFX_CHECK_MSG (peakOf (late) < 1e-3f, "tail after 40 s: " + juce::String (peakOf (late)));

    Chain k;
    k.p = ch.p;
    k.run (impulse (4800, 0.9f));
    k.p[P::killFb] = 1.0f;
    k.p[P::killTail] = 1.0f;
    k.run (z);
    IFX_CHECK (peakOf (k.run (z)) < 1e-4f);
}

IFX_TEST (Studio, echoTailIsFinite)
{
    Chain ch;
    ch.p[P::ecOn] = 1.0f;
    ch.p[P::ecFeedback] = 0.9f;
    ch.p[P::ecMix] = 1.0f;
    ch.p[P::ecSync] = 0.0f;
    ch.p[P::ecTime] = 100.0f;
    ch.run (impulse (4800));
    juce::AudioBuffer<float> z (2, 48000);
    z.clear();
    for (int i = 0; i < 20; ++i) ch.run (z);
    IFX_CHECK (peakOf (ch.run (z)) < 1e-4f);
    IFX_CHECK (ch.c.tailSeconds() > 1.0 && ch.c.tailSeconds() <= 30.0);
}

IFX_TEST (Studio, resonatorRingsThenDecaysNoSelfOscillation)
{
    Chain ch;
    ch.p[P::rsOn] = 1.0f;
    ch.p[P::rsMix] = 1.0f;
    ch.p[P::rsFeedback] = 0.9f;
    ch.p[P::rsDecay] = 0.5f;
    auto out = ch.run (impulse (48000));
    IFX_CHECK (peakOf (slice (out, 100, 4800)) > 1e-3f);
    juce::AudioBuffer<float> z (2, 48000);
    z.clear();
    for (int i = 0; i < 10; ++i) ch.run (z);
    IFX_CHECK (peakOf (ch.run (z)) < 1e-5f);
}

IFX_TEST (Studio, crushQuantisesToExpectedSteps)
{
    Chain ch;
    ch.p[P::crOn] = 1.0f;
    ch.p[P::crBits] = 4.0f;
    ch.p[P::crRate] = 48000.0f;
    ch.p[P::crDither] = 0.0f;
    juce::AudioBuffer<float> ramp (2, 9600);
    for (int i = 0; i < 9600; ++i) { ramp.setSample (0, i, -0.9f + 1.8f * i / 9600.0f); ramp.setSample (1, i, 0.0f); }
    ch.run (ramp);                                   // settle the stage ramp
    auto out = ch.run (ramp);
    const float step = 2.0f / std::pow (2.0f, 3.0f) * 0.5f;
    bool quantised = true;
    for (int i = 0; i < 9600; ++i)
    {
        const float v = out.getSample (0, i) / step;
        quantised &= std::abs (v - std::round (v)) < 1e-4f;
    }
    IFX_CHECK (quantised);
}

IFX_TEST (Studio, crushHoldRateHoldsSamples)
{
    Chain ch;
    ch.p[P::crOn] = 1.0f;
    ch.p[P::crBits] = 16.0f;
    ch.p[P::crRate] = 4800.0f;   // hold each value for 10 samples
    ch.run (noise (2, 4800, 1));
    auto out = ch.run (noise (2, 4800, 2));
    int changes = 0;
    for (int i = 1; i < 4800; ++i) changes += out.getSample (0, i) != out.getSample (0, i - 1);
    IFX_CHECK (changes <= 481 && changes >= 400);
}

IFX_TEST (Studio, driveAdaaAliasesLessThanNaive)
{
    // 7 kHz sine driven hard: the 7th harmonic (49 kHz) folds to 1 kHz at 48 kHz.
    Chain ch;
    ch.p[P::drOn] = 1.0f;
    ch.p[P::drCurve] = 1.0f; // hard clip: worst case
    ch.p[P::drPre] = 24.0f;
    ch.p[P::drPost] = 0.0f;
    ch.p[P::drTone] = 1.0f;
    auto in = sine (2, 48000, 7000.0, kFs, 0.5f);
    ch.run (in);
    auto out = ch.run (in);
    juce::AudioBuffer<float> naive (2, 48000);
    for (int i = 0; i < 48000; ++i)
    {
        const float v = std::clamp (in.getSample (0, i) * dbToGain (24.0f), -1.0f, 1.0f);
        naive.setSample (0, i, v);
        naive.setSample (1, i, v);
    }
    const double aliasAdaa = bandEnergy (out, kFs, 990.0, 1010.0, 3);
    const double aliasNaive = bandEnergy (naive, kFs, 990.0, 1010.0, 3);
    IFX_CHECK_MSG (aliasAdaa < aliasNaive * 0.5, juce::String (aliasAdaa) + " vs " + juce::String (aliasNaive));
}

IFX_TEST (Studio, asymmetricDriveDcIsBlocked)
{
    Chain ch;
    ch.p[P::drOn] = 1.0f;
    ch.p[P::drCurve] = 2.0f;
    ch.p[P::drPre] = 30.0f;
    auto in = sine (2, 96000, 200.0, kFs, 0.8f);
    auto out = ch.run (in);
    double mean = 0.0;
    for (int i = 48000; i < 96000; ++i) mean += out.getSample (0, i);
    mean /= 48000.0;
    IFX_CHECK (std::abs (mean) < 1e-3);
}

IFX_TEST (Studio, chokeClosesGateOnlyInStudioBranch)
{
    Harness h (kFs, 2, 256);
    h.params[P::hum] = 1.0f;
    h.params[P::tcTouch] = 1.0f;
    h.params[P::studioOn] = 1.0f;
    h.params[P::gtOn] = 1.0f;
    h.params[P::gtMode] = 2.0f;
    h.params[P::laneOn] = 1.0f;
    h.params[P::mix] = 0.5f;
    h.transport.hasHostTransport = true;
    h.transport.playing = true;
    h.setSlots (SourceType::PlugTouch, SourceType::None);
    Pattern p;
    for (int i = 0; i < 16; ++i) { p.lanes[0].steps[i].active = 1; p.lanes[0].steps[i].choke = 1; p.lanes[0].steps[i].duration = 1.0f; p.lanes[0].steps[i].gesture = 0; }
    h.publish (p);
    auto in = sine (2, (int) kFs, 440.0, kFs, 0.5f);
    auto out = h.run (in);
    // the dry half of the mix still carries the full input tone
    IFX_CHECK (bandEnergy (slice (out, 24000, 24000), kFs, 435.0, 445.0, 3)
               > bandEnergy (slice (in, 24000, 24000), kFs, 435.0, 445.0, 3) * 0.2);
}

IFX_TEST (Studio, gateThroughIsOpen)
{
    Chain ch;
    ch.p[P::gtOn] = 1.0f;
    ch.p[P::gtMode] = 0.0f;
    ch.run (noise (2, 4800, 1));
    auto in = noise (2, 4800, 2);
    IFX_CHECK (maxAbsDiff (ch.run (in), in) < 1e-5);
}

IFX_TEST (Studio, autoMatchIsBounded)
{
    Chain ch;
    ch.p[P::drOn] = 1.0f;
    ch.p[P::drPre] = 36.0f;
    ch.p[P::drPost] = -24.0f;
    ch.p[P::drAuto] = 1.0f;
    auto in = sine (2, 96000, 300.0, kFs, 0.1f);
    auto out = ch.run (in);
    IFX_CHECK (allFinite (out));
    IFX_CHECK (peakOf (out) < 4.0f);
}

IFX_TEST (Studio, scaleSnapKeepsNotesInKey)
{
    // C major: 61 (C#) snaps to 60 or 62; every result is a scale member for all keys and scales
    for (int scale = 0; scale < Scale::kNumScales; ++scale)
        for (int key = 0; key < 12; ++key)
            for (double m = 30.0; m < 100.0; m += 0.37)
            {
                const int n = Scale::snapMidi (m, key, scale);
                IFX_CHECK (Scale::mask (scale)[(size_t) (((n - key) % 12 + 12) % 12)]);
                IFX_CHECK (std::abs (n - m) <= 3.0);
            }
    IFX_CHECK (Scale::snapMidi (60.0, 0, 1) == 60);
    IFX_CHECK (Scale::snapMidi (61.2, 0, 1) == 62);
    IFX_CHECK (Scale::snapMidi (60.9, 0, 5) == 62 || Scale::snapMidi (60.9, 0, 5) == 60);
    IFX_CHECK (std::abs (Scale::snapHz (445.0, 9, 1) - 440.0) < 1e-6); // A in A major stays A
    IFX_CHECK (Scale::snapHz (0.0, 0, 1) == 0.0);
}
