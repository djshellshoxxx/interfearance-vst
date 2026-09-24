// Environment models from Docs/research/ElectricalInterference.md: coupling-aware hum, ground
// loop + interface rejection, phase-control dimmer buzz, 2G phone bursts.
#include "TestHelpers.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
constexpr double kFs = 48000.0;

double toneEnergy (const juce::AudioBuffer<float>& b, double hz) { return bandEnergy (b, kFs, hz, hz, 1); }

juce::AudioBuffer<float> renderGroundLoop (int gear, int iface, float run = 10.0f, int shield = 0, int outlets = 2,
                                           SourceType slot = SourceType::None, float insert = 1.0f, int block = 256)
{
    Harness h (kFs, 1, block);
    h.params = quietParams();
    h.params[P::glGear] = (float) gear;
    h.params[P::glIface] = (float) iface;
    h.params[P::glRun] = run;
    h.params[P::glShield] = (float) shield;
    h.params[P::glOutlets] = (float) outlets;
    h.params[P::jkInsert] = insert;
    h.params[P::jkWiggle] = 0.0f;
    h.setSlots (slot, SourceType::None);
    h.runSilence ((int) (kFs * 0.2)); // settle
    return h.runSilence ((int) kFs);
}
}

IFX_TEST (Environment, groundLoopGrowsWithLeakageCurrentAndStaysCapped)
{
    const double low = rmsOf (renderGroundLoop (1, 0)), mid = rmsOf (renderGroundLoop (2, 0)), high = rmsOf (renderGroundLoop (3, 0));
    IFX_CHECK_MSG (low > 0.0 && mid > low * 5.0 && high > mid * 5.0,
                   "low " + juce::String (low) + " mid " + juce::String (mid) + " high " + juce::String (high));
    const auto catv = renderGroundLoop (4, 0, 30.0f, 0, 3);
    IFX_CHECK (allFinite (catv));
    IFX_CHECK (peakOf (catv) <= limit (limits::backgroundCeiling, Policy::Natural) * 1.3f + 1.0e-6f);
    IFX_CHECK (isSilent (renderGroundLoop (0, 0), 1.0e-12f)); // no other gear: no loop
}

IFX_TEST (Environment, leakageLoopHumIsOddHarmonicBuzz)
{
    const auto b = renderGroundLoop (3, 0);
    // leakage from 2-prong gear is capacitive: odd mains harmonics, no rectifier 2nd harmonic
    IFX_CHECK (toneEnergy (b, 150.0) > toneEnergy (b, 100.0) * 20.0);
    IFX_CHECK (toneEnergy (b, 250.0) > toneEnergy (b, 200.0) * 20.0);
}

IFX_TEST (Environment, interfaceRejectsHumButLeavesAThinBuzz)
{
    const auto unbal = renderGroundLoop (4, 0);
    const auto bal = renderGroundLoop (4, 1);
    const auto iso = renderGroundLoop (4, 2);
    const double f0u = toneEnergy (unbal, 50.0), f0b = toneEnergy (bal, 50.0), f0i = toneEnergy (iso, 50.0);
    IFX_CHECK_MSG (f0b < f0u * 1.0e-2, "balanced should reject >= 20 dB at 50 Hz");
    IFX_CHECK_MSG (f0i < f0u * 1.0e-5, "isolator should reject >= 50 dB at 50 Hz");
    // rejection falls with frequency: the residue is relatively brighter
    const double brightU = toneEnergy (unbal, 650.0) / std::max (1.0e-30, f0u);
    const double brightI = toneEnergy (iso, 650.0) / std::max (1.0e-30, f0i);
    IFX_CHECK_MSG (brightI > brightU * 10.0, "isolator residue should be buzzier than the raw loop");
}

IFX_TEST (Environment, groundLoopBreaksWhenTheSleeveOpens)
{
    const double seated = rmsOf (renderGroundLoop (3, 0, 10.0f, 0, 2, SourceType::LooseJack, 1.0f));
    const double withdrawn = rmsOf (renderGroundLoop (3, 0, 10.0f, 0, 2, SourceType::LooseJack, 0.0f));
    IFX_CHECK_MSG (seated > 0.0 && withdrawn < seated * 0.1,
                   "seated " + juce::String (seated) + " withdrawn " + juce::String (withdrawn));
}

namespace
{
juce::AudioBuffer<float> renderDimmer (float brightness, int kind, int block = 256)
{
    Harness h (kFs, 1, block);
    h.params = quietParams();
    h.params[P::buzz] = 1.0f;
    h.params[P::buzzType] = 1.0f;
    h.params[P::dimBright] = brightness;
    h.params[P::dimKind] = (float) kind;
    h.params[P::tcTouch] = 1.0f;
    h.params[P::tcPressure] = 1.0f;
    h.params[P::tcExposure] = 1.0f;
    h.params[P::tcShield] = 0.0f;
    h.setSlots (SourceType::PlugTouch, SourceType::None);
    h.runSilence ((int) (kFs * 0.3));
    return h.runSilence ((int) kFs);
}
}

IFX_TEST (Environment, dimmerBuzzPeaksNearHalfBrightnessAndIsLockedToMains)
{
    const double half = rmsOf (renderDimmer (0.5f, 0)), dim = rmsOf (renderDimmer (0.08f, 0)), bright = rmsOf (renderDimmer (0.95f, 0));
    IFX_CHECK_MSG (half > dim && half > bright,
                   "half " + juce::String (half) + " dim " + juce::String (dim) + " bright " + juce::String (bright));
    const auto b = renderDimmer (0.5f, 0);
    // fully on: the triac never switches, so only the load current's own 50 Hz field remains (no spikes)
    IFX_CHECK (toneEnergy (renderDimmer (1.0f, 0), 1050.0) < toneEnergy (b, 1050.0) * 1.0e-3);
    IFX_CHECK (toneEnergy (b, 150.0) > toneEnergy (b, 100.0) * 10.0); // odd harmonics of the mains
    IFX_CHECK (toneEnergy (b, 350.0) > toneEnergy (b, 330.0) * 10.0); // harmonic, not noise
    // a choke-softened stage dimmer is quieter and darker than a cheap wall dimmer
    const auto stage = renderDimmer (0.5f, 1);
    IFX_CHECK (rmsOf (stage) < half);
    IFX_CHECK (toneEnergy (stage, 2550.0) / toneEnergy (stage, 150.0) < toneEnergy (b, 2550.0) / toneEnergy (b, 150.0));
}

namespace
{
juce::AudioBuffer<float> renderPhone (int state, float distance, int block = 256)
{
    Harness h (kFs, 1, block);
    h.params = quietParams();
    h.params[P::gsm] = (float) state;
    h.params[P::gsmDist] = distance;
    h.setSlots (SourceType::None, SourceType::None);
    return h.runSilence ((int) (kFs * 1.5));
}
}

IFX_TEST (Environment, phoneBurstsAt217HzAndFallOffWithDistance)
{
    const auto call = renderPhone (3, 0.15f);
    IFX_CHECK (allFinite (call));
    IFX_CHECK (toneEnergy (call, 5200.0 / 24.0) > toneEnergy (call, 190.0) * 20.0);
    IFX_CHECK (toneEnergy (call, 2.0 * 5200.0 / 24.0) > toneEnergy (call, 410.0) * 5.0);
    IFX_CHECK (rmsOf (renderPhone (3, 1.5f)) < rmsOf (call) * 0.05);
    IFX_CHECK (isSilent (renderPhone (0, 0.15f), 1.0e-12f)); // phone away: nothing
}

IFX_TEST (Environment, newEnvironmentSourcesAreBlockSizeIndependentAndBounded)
{
    auto render = [] (int block)
    {
        Harness h (kFs, 2, block);
        h.params = quietParams();
        h.params[P::policy] = 1.0f;
        h.params[P::glGear] = 4.0f;
        h.params[P::glOutlets] = 3.0f;
        h.params[P::glRun] = 30.0f;
        h.params[P::glShield] = 0.0f;
        h.params[P::glIface] = 2.0f;
        h.params[P::buzz] = 1.0f;
        h.params[P::buzzType] = 1.0f;
        h.params[P::gsm] = 2.0f;
        h.params[P::gsmDist] = 0.1f;
        h.params[P::jkInsert] = 0.8f;
        h.params[P::jkWiggle] = 0.7f;
        h.setSlots (SourceType::LooseJack, SourceType::CableHandling);
        return h.run (drums (2, (int) kFs * 2, kFs));
    };
    const auto a = render (64), b = render (401);
    IFX_CHECK (allFinite (a));
    IFX_CHECK (peakOf (a) < kSafetyClamp);
    IFX_CHECK_MSG (maxAbsDiff (a, b) < 1.0e-5, "64 vs 401: " + juce::String (maxAbsDiff (a, b)));
}
