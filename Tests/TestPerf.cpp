// Slow tests (run with --slow): CPU cost of the worst-case topology and an offline soak of one
// hour of audio with continuous automation (master spec 11: CPU scaling, 1+ hour soak / drift).
#include "TestHelpers.h"
#include <iostream>
#include <algorithm>

using namespace ifx;
using namespace ifxtest;

namespace
{
/** Both slots, every Studio stage, local feedback, lane, hum and excitation: the most
    expensive legal configuration. */
void worstCase (Harness& h)
{
    h.params[P::hum] = 0.6f;
    h.params[P::buzz] = 0.3f;
    h.params[P::rf] = 0.2f;
    h.params[P::inputMode] = 1.0f;
    h.params[P::preOn] = 1.0f;
    h.params[P::excType] = 1.0f;
    h.params[P::jkInsert] = 0.8f;
    h.params[P::jkWiggle] = 0.6f;
    h.params[P::cbShake] = 0.5f;
    h.params[P::cbMic] = 0.5f;
    h.params[P::laneOn] = 1.0f;
    h.params[P::studioOn] = 1.0f;
    h.params[P::studioSrc] = 2.0f;
    for (int p : { (int) P::eqOn, (int) P::ctOn, (int) P::drOn, (int) P::crOn, (int) P::rsOn, (int) P::gtOn, (int) P::ecOn, (int) P::ceilOn })
        h.params[p] = 1.0f;
    h.params[P::fbAmount] = 0.5f;
    h.params[P::ghostOn] = 1.0f;
    h.transport.hasHostTransport = true;
    h.transport.playing = true;
    h.transport.bpm = 124.0;
    h.setSlots (SourceType::LooseJack, SourceType::CableHandling);
    Pattern pat;
    for (int i = 0; i < 16; i += 2) pat.lanes[0].steps[i].active = 1;
    for (int i = 1; i < 16; i += 3) pat.lanes[1].steps[i].active = 1;
    h.publish (pat);
}

double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }
}

IFX_SLOW_TEST (Perf, cpuRealtimeFactorWorstCase)
{
    std::cout << "    CPU: " << juce::SystemStats::getCpuModel() << std::endl;
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Harness h (fs, 2, 256);
        worstCase (h);
        const auto chunk = drums (2, (int) fs, fs); // 1 s
        h.run (chunk); // warm-up
        const double t0 = nowMs();
        const int seconds = 20;
        bool finite = true;
        for (int s = 0; s < seconds; ++s)
            finite = finite && allFinite (h.run (chunk));
        const double rt = (nowMs() - t0) / 1000.0 / seconds;
        std::cout << "    " << juce::String (fs / 1000.0, 1) << " kHz stereo, block 256, worst case: "
                  << juce::String (rt * 100.0, 2) << " % of one core (real-time factor " << juce::String (1.0 / std::max (1.0e-9, rt), 1) << "x)"
                  << std::endl;
        IFX_CHECK (finite);
        IFX_CHECK_MSG (rt < 0.5, "worst case too slow at " + juce::String (fs) + " Hz: " + juce::String (rt, 3));
    }
}

IFX_SLOW_TEST (Perf, soakOneHourOfAudioNoDriftFiniteBounded)
{
    const double fs = 48000.0;
    Harness h (fs, 2, 512);
    worstCase (h);
    const auto chunk = drums (2, (int) fs, fs, 99);
    Rng rng (777);
    const int minutes = 60, segment = 5;
    std::vector<double> segMs;
    double segStart = nowMs();
    float peak = 0.0f;
    bool finite = true;
    for (int s = 1; s <= minutes * 60; ++s)
    {
        // continuous automation: a few Natural-range parameters change every second
        h.params[P::jkWiggle] = (float) rng.uniform() * 0.8f;
        h.params[P::cbBend] = (float) rng.uniform();
        h.params[P::mix] = 0.5f + 0.5f * (float) rng.uniform();
        h.params[P::drPre] = (float) rng.range (0.0, 24.0);
        h.params[P::rsFreq] = (float) rng.range (80.0, 2000.0);
        h.params[P::ghostFreq] = (float) rng.range (20.0, 2000.0);
        if (s % 97 == 0) h.params[P::killFb] = 1.0f;
        if (s % 97 == 3) h.params[P::killFb] = 0.0f;
        const auto out = h.run (chunk);
        finite = finite && allFinite (out);
        peak = std::max (peak, peakOf (out));
        if (s % (segment * 60) == 0)
        {
            const double t = nowMs();
            segMs.push_back (t - segStart);
            segStart = t;
        }
    }
    juce::String times;
    for (auto t : segMs) times << juce::String (t / 1000.0, 2) << " ";
    std::cout << "    60 min of 48 kHz stereo audio, worst case + automation. Seconds per 5-min segment: " << times << std::endl;
    std::cout << "    peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS" << std::endl;
    IFX_CHECK (finite);
    IFX_CHECK (peak <= kSafetyClamp);
    // no CPU drift: the slowest late segment is not much slower than the fastest early one
    const double early = std::min (segMs[0], segMs[1]);
    const double late = *std::max_element (segMs.end() - 3, segMs.end());
    IFX_CHECK_MSG (late < early * 1.6 + 500.0, "CPU drift: early " + juce::String (early) + " ms, late " + juce::String (late) + " ms");
}
