// RF interference with transmitter types (square-law detection) and the triboelectric part of
// cable microphonics (Docs/research/ElectricalInterference.md 1.5, 1.8, 5.7, 5.10).
#include "TestHelpers.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
constexpr double kFs = 48000.0;
constexpr double kTwoPi = 2.0 * juce::MathConstants<double>::pi;
const char* kTypeName[3] = { "AM Station", "Two-Way Radio", "Motor Sparks" };

/** The environment's rf output alone (unit mapping: what Source::envWave receives). */
juce::AudioBuffer<float> renderEnvRf (int type, float amount, double seconds, double fs = kFs, uint64_t seed = 7)
{
    Environment env;
    env.prepare (fs);
    env.seed (seed);
    env.setRf (amount);
    env.setRfType (type);
    const int n = (int) (seconds * fs);
    juce::AudioBuffer<float> b (1, n);
    for (int i = 0; i < n; ++i)
        b.setSample (0, i, env.step().rf);
    return b;
}

/** Interference reaching the core through a withdrawn, unshielded plug (Artifacts tap). */
juce::AudioBuffer<float> renderEngineRf (int type, float amount, int block, double seconds, uint64_t seed = 12345)
{
    Harness h (kFs, 2, block);
    h.engine->setSeed (seed);
    h.params = quietParams();
    h.params[P::rf] = amount;
    h.params[P::rfType] = (float) type;
    h.params[P::stereo] = 0.0f;
    h.params[P::jkInsert] = 0.0f;
    h.params[P::jkWiggle] = 0.0f;
    h.params[P::jkShield] = 0.0f;
    h.params[P::jkShort] = 0.0f;
    h.params[P::monitor] = (float) (int) Tap::Artifacts;
    h.setSlots (SourceType::LooseJack, SourceType::None);
    return h.runSilence ((int) (seconds * kFs));
}

/** Power of one channel at frequency f (Goertzel). */
double powerAt (const juce::AudioBuffer<float>& b, int start, int len, double fs, double f)
{
    const double c = 2.0 * std::cos (kTwoPi * f / fs);
    double s1 = 0.0, s2 = 0.0;
    const auto* d = b.getReadPointer (0);
    for (int i = start; i < start + len; ++i)
    {
        const double s = d[i] + c * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

double rmsRange (const juce::AudioBuffer<float>& b, int start, int len)
{
    double s = 0.0;
    for (int i = start; i < start + len; ++i)
        s += (double) b.getSample (0, i) * b.getSample (0, i);
    return len > 0 ? std::sqrt (s / len) : 0.0;
}

float peakRange (const juce::AudioBuffer<float>& b, int start, int len)
{
    float p = 0.0f;
    for (int i = start; i < start + len; ++i)
        p = std::max (p, std::abs (b.getSample (0, i)));
    return p;
}

double meanRange (const juce::AudioBuffer<float>& b, int start, int len)
{
    double s = 0.0;
    for (int i = start; i < start + len; ++i)
        s += b.getSample (0, i);
    return len > 0 ? s / len : 0.0;
}

/** Frame RMS values (one channel). */
std::vector<double> frameRms (const juce::AudioBuffer<float>& b, int start, int frame)
{
    std::vector<double> r;
    for (int pos = start; pos + frame <= b.getNumSamples(); pos += frame)
        r.push_back (rmsRange (b, pos, frame));
    return r;
}
}

//==============================================================================
IFX_TEST (Rf, silentWhenInterferenceIsZero)
{
    for (int type = 0; type < 3; ++type)
    {
        IFX_CHECK_MSG (peakOf (renderEnvRf (type, 0.0f, 1.0)) == 0.0f, kTypeName[type]);
        IFX_CHECK_MSG (peakOf (renderEngineRf (type, 0.0f, 256, 1.0)) == 0.0f, kTypeName[type]);

        // turned down at runtime: fades out and stops completely
        Environment env;
        env.prepare (kFs);
        env.seed (3);
        env.setRf (1.0f);
        env.setRfType (type);
        for (int i = 0; i < (int) kFs; ++i) env.step();
        env.setRf (0.0f);
        float late = 0.0f;
        for (int i = 0; i < (int) kFs; ++i)
        {
            const float v = env.step().rf;
            if (i > (int) (kFs * 0.5)) late = std::max (late, std::abs (v));
        }
        IFX_CHECK_MSG (late == 0.0f, juce::String (kTypeName[type]) + ": still " + juce::String (late) + " after turning it off");
    }
}

// Square law (Jim Brown, "RFI in Audio Systems"): 10 dB less RF is 20 dB less audible interference.
IFX_TEST (Rf, tenDbLessRfIsTwentyDbLessNoise)
{
    for (int type = 0; type < 3; ++type)
    {
        const auto full = renderEngineRf (type, 1.0f, 256, 3.0);
        const auto less = renderEngineRf (type, (float) std::pow (10.0, -10.0 / 20.0), 256, 3.0);
        const double r = 20.0 * std::log10 (rmsOf (less) / std::max (1.0e-20, rmsOf (full)));
        IFX_CHECK_MSG (rmsOf (full) > 1.0e-4, juce::String (kTypeName[type]) + ": no interference reached the input");
        IFX_CHECK_MSG (std::abs (r + 20.0) < 0.5, juce::String (kTypeName[type]) + ": -10 dB RF gave " + juce::String (r, 2) + " dB");
    }
}

IFX_TEST (Rf, boundedDcFreeAndSampleRateTolerant)
{
    for (int type = 0; type < 3; ++type)
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            const auto b = renderEnvRf (type, 1.0f, 4.0, fs);
            const int from = (int) (fs * 0.5), len = b.getNumSamples() - from;
            const juce::String name = juce::String (kTypeName[type]) + " @ " + juce::String (fs, 0);
            IFX_CHECK_MSG (allFinite (b) && peakOf (b) <= 1.0f, name);
            IFX_CHECK_MSG (peakOf (b) > 0.1f, name + ": peak " + juce::String (peakOf (b)));
            IFX_CHECK_MSG (std::abs (meanRange (b, from, len)) < 0.01, name + ": DC " + juce::String (meanRange (b, from, len), 5));
        }
}

// AM station: a continuous, syllabic (2-8 Hz) programme that is never a tone.
IFX_TEST (Rf, amStationIsSyllabicAndNotTonal)
{
    const auto b = renderEnvRf (0, 1.0f, 8.5);
    const int from = (int) (kFs * 0.5);

    // syllabic envelope: rectify, smooth (30 Hz), 100 Hz frames, then the modulation spectrum
    juce::AudioBuffer<float> env (1, (b.getNumSamples() - from) / 480);
    {
        const double a = 1.0 - std::exp (-kTwoPi * 30.0 / kFs);
        double lp = 0.0;
        for (int i = from, k = 0; k < env.getNumSamples(); ++i)
        {
            lp += (std::abs ((double) b.getSample (0, i)) - lp) * a;
            if ((i - from) % 480 == 479) env.setSample (0, k++, (float) lp);
        }
        const double m = meanRange (env, 0, env.getNumSamples());
        for (int k = 0; k < env.getNumSamples(); ++k) env.setSample (0, k, env.getSample (0, k) - (float) m);
    }
    double syll = 0.0, fast = 0.0;
    for (double f = 2.0; f <= 8.0; f += 0.125) syll += powerAt (env, 0, env.getNumSamples(), 100.0, f);
    for (double f = 12.0; f <= 40.0; f += 0.125) fast += powerAt (env, 0, env.getNumSamples(), 100.0, f);
    syll /= 49.0;
    fast /= 225.0;
    IFX_CHECK_MSG (syll > 3.0 * fast, "envelope power 2-8 Hz " + juce::String (syll, 6) + " vs 12-40 Hz " + juce::String (fast, 6));

    // continuous station: most 100 ms windows carry programme (only short phrase pauses)
    const auto fr = frameRms (b, from, 4800);
    const double overall = rmsRange (b, from, b.getNumSamples() - from);
    int active = 0;
    for (double v : fr) if (v > 0.05 * overall) ++active;
    IFX_CHECK_MSG (active > (int) (0.55 * (double) fr.size()), juce::String (active) + " of " + juce::String ((int) fr.size()) + " windows active");

    // no tonal line: around the strongest bin, a +/-2 Hz band holds only a small part of +/-50 Hz
    const int w = (int) kFs, start = (int) (kFs * 2.0);
    double best = 0.0, fBest = 0.0;
    for (double f = 100.0; f <= 4000.0; f += 2.0)
    {
        const double p = powerAt (b, start, w, kFs, f);
        if (p > best) { best = p; fBest = f; }
    }
    double narrow = 0.0, wide = 0.0;
    for (int df = -50; df <= 50; ++df)
    {
        const double p = powerAt (b, start, w, kFs, fBest + df);
        wide += p;
        if (std::abs (df) <= 2) narrow += p;
    }
    IFX_CHECK_MSG (narrow < 0.5 * wide, "tonal line near " + juce::String (fBest, 0) + " Hz: " + juce::String (narrow / wide, 3));
}

// Two-way radio: keyed bursts (tens to hundreds of ms) separated by silence.
IFX_TEST (Rf, twoWayRadioIsKeyedBursts)
{
    const auto b = renderEnvRf (1, 1.0f, 12.0);
    const auto fr = frameRms (b, (int) (kFs * 0.2), 480);  // 10 ms frames
    double mx = 0.0;
    for (double v : fr) mx = std::max (mx, v);
    const double thr = 0.02 * mx;
    int silent = 0, segments = 0, run = 0, longest = 0;
    bool on = false;
    for (double v : fr)
    {
        const bool a = v > thr;
        if (a && ! on) ++segments;
        run = a ? run + 1 : 0;
        longest = std::max (longest, run);
        if (! a) ++silent;
        on = a;
    }
    const double silentShare = (double) silent / (double) fr.size();
    IFX_CHECK_MSG (silentShare > 0.3 && silentShare < 0.97, "silent share " + juce::String (silentShare, 3));
    IFX_CHECK_MSG (segments >= 4, juce::String (segments) + " transmissions in 12 s");
    IFX_CHECK_MSG (longest <= 45, "longest transmission " + juce::String (longest * 10) + " ms");
}

// Motor sparks: Poisson commutation impulses at 50-300 per second, detected into crackle.
IFX_TEST (Rf, motorSparksAreImpulsesAt50To300PerSecond)
{
    Environment env;
    env.prepare (kFs);
    env.seed (11);
    env.setRf (1.0f);
    env.setRfType (2);
    const int n = (int) (kFs * 6.0);
    juce::AudioBuffer<float> b (1, n);
    for (int i = 0; i < n; ++i) b.setSample (0, i, env.step().rf);
    const double rate = (double) env.rfSparkCount() / 6.0;
    IFX_CHECK_MSG (rate >= 50.0 && rate <= 300.0, "spark rate " + juce::String (rate, 1) + " per second");

    const int from = (int) (kFs * 0.5), len = n - from;
    const double crest = peakRange (b, from, len) / std::max (1.0e-12, rmsRange (b, from, len));
    IFX_CHECK_MSG (crest > 4.0, "crest factor " + juce::String (crest, 2) + " (crackle is impulsive, not noise)");

    // audible clicks: peaks above 5 % of the maximum, at least 0.3 ms apart
    const float thr = 0.05f * peakOf (b);
    int clicks = 0, last = -100000;
    for (int i = from + 1; i < n - 1; ++i)
    {
        const float v = std::abs (b.getSample (0, i));
        if (v > thr && v >= std::abs (b.getSample (0, i - 1)) && v >= std::abs (b.getSample (0, i + 1)) && i - last > (int) (kFs * 0.0003))
        {
            ++clicks;
            last = i;
        }
    }
    const double clickRate = clicks / (len / kFs);
    IFX_CHECK_MSG (clickRate > 20.0 && clickRate < 330.0, "audible click rate " + juce::String (clickRate, 1) + " per second");
}

IFX_TEST (Rf, blockSizeIndependentAndSeeded)
{
    for (int type = 0; type < 3; ++type)
    {
        const auto a = renderEngineRf (type, 0.8f, 64, 2.0), b = renderEngineRf (type, 0.8f, 997, 2.0);
        IFX_CHECK_MSG (maxAbsDiff (a, b) < 1.0e-7, juce::String (kTypeName[type]) + ": blocks 64 vs 997 differ by " + juce::String (maxAbsDiff (a, b), 8));
        IFX_CHECK_MSG (maxAbsDiff (a, renderEngineRf (type, 0.8f, 256, 2.0, 99)) > 1.0e-5, juce::String (kTypeName[type]) + ": the seed changes nothing");
    }
}

// Switching the transmitter mid-run stays finite and bounded, and the new type takes over.
IFX_TEST (Rf, transmitterSwitchIsBoundedAndTakesEffect)
{
    Environment env;
    env.prepare (kFs);
    env.seed (5);
    env.setRf (1.0f);
    env.setRfType (1);
    float pk = 0.0f;
    for (int i = 0; i < (int) kFs; ++i) pk = std::max (pk, std::abs (env.step().rf));
    env.setRfType (2);
    const uint64_t before = env.rfSparkCount();
    for (int i = 0; i < (int) kFs * 2; ++i)
    {
        const float v = env.step().rf;
        IFX_CHECK (std::isfinite (v));
        pk = std::max (pk, std::abs (v));
    }
    IFX_CHECK (pk <= 1.0f);
    IFX_CHECK_MSG (env.rfSparkCount() - before > 50, "the motor did not take over");
}

//==============================================================================
// Triboelectric microphonics: needs Mic (realism spec 5), follows the node impedance (Source
// Circuit) and, with a passive source, the cable capacitance sets its corner.
namespace
{
juce::AudioBuffer<float> renderHandling (int inputMode, float cableLoad, float mic)
{
    Harness h (kFs, 1, 256);
    h.params = quietParams();
    h.params[P::inputMode] = (float) inputMode;
    h.params[P::cableLoad] = cableLoad;
    h.params[P::cbMic] = mic;
    h.params[P::cbShake] = 1.0f;
    h.params[P::cbDamage] = 0.0f;
    h.params[P::cbShieldDmg] = 0.0f;
    h.params[P::monitor] = (float) (int) Tap::Artifacts;
    h.setSlots (SourceType::CableHandling, SourceType::None);
    for (int k = 0; k < 6; ++k)
        h.pendingEvents.push_back ({ (int) ((0.2 + 0.3 * k) * kFs), ControlEvent::GestureA, (int) GestureKind::Tap, 0.4f });
    return h.runSilence ((int) (kFs * 2.0));
}
}

IFX_TEST (Rf, triboelectricHandlingNeedsMicAndFollowsTheNode)
{
    // a sealed cable with Mic at zero stays silent even from a passive source
    IFX_CHECK (peakOf (renderHandling (1, 600.0f, 0.0f)) == 0.0f);

    // passive (high-impedance) source: the friction charge is heard; buffered: held down
    const auto passive = renderHandling (1, 200.0f, 1.0f), buffered = renderHandling (0, 200.0f, 1.0f);
    IFX_CHECK_MSG (rmsOf (passive) > 1.2 * rmsOf (buffered), "passive " + juce::String (rmsOf (passive), 6) + " buffered " + juce::String (rmsOf (buffered), 6));

    // passive: more cable capacitance lowers the corner, so the crackle gets darker
    const auto heavy = renderHandling (1, 2000.0f, 1.0f);
    const double hfLight = bandEnergy (passive, kFs, 6000.0, 12000.0), hfHeavy = bandEnergy (heavy, kFs, 6000.0, 12000.0);
    IFX_CHECK_MSG (hfLight > 2.0 * hfHeavy, "6-12 kHz with 200 pF " + juce::String (hfLight, 9) + " vs 2000 pF " + juce::String (hfHeavy, 9));

    // buffered: the corner is far above the audio band whatever the cable, so Cable Load changes nothing
    IFX_CHECK (maxAbsDiff (buffered, renderHandling (0, 2000.0f, 1.0f)) == 0.0);
}
