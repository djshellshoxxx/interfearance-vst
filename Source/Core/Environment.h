#pragma once

#include "Random.h"
#include <cmath>
#include <algorithm>

namespace ifx
{

/** The shared electrical environment. One mains phase for the whole plugin (two slots never
    get two independent hum generators). Phase is continuous across blocks and 50<->60 Hz
    changes glide over ~60 ms. On a transport reset the phase is re-derived from absolute
    host time so a bounce is repeatable. */
class Environment
{
public:
    struct Sample
    {
        float humClean = 0.0f;  // fundamental-dominated pickup waveform (peak ~1)
        float humBright = 0.0f; // harmonic-rich pickup waveform (peak ~1)
        float buzz = 0.0f;      // low 2x-mains buzz band (peak ~1), zero when buzz amount is 0
        float rf = 0.0f;        // sparse RF-like demodulated chatter (peak ~1), zero when rf amount is 0
    };

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        glide = std::exp (-1.0 / (0.02 * fs));
        const double w = 2.0 * 3.14159265358979 * 2500.0 / fs;
        lpC = 1.0 - std::exp (-w);
        amC = 1.0 - std::exp (-2.0 * 3.14159265358979 * 6.0 / fs);
        reset (0.0);
    }

    void seed (uint64_t s) noexcept { rng.seed (hashCombine (s, 0xE11Full)); }

    void reset (double hostSeconds) noexcept
    {
        freq = targetFreq;
        const double cycles = hostSeconds * freq;
        phase = cycles - std::floor (cycles);
        burstOn = false;
        burstCounter = 0;
        rfLp1 = rfLp2 = rfAm = 0.0;
        rfAmTarget = 0.0;
    }

    void setMainsFrequency (float hz) noexcept { targetFreq = hz; }
    void setBuzz (float amount) noexcept { buzzAmount = amount; }
    void setRf (float amount) noexcept { rfAmount = amount; }

    Sample step() noexcept
    {
        freq = targetFreq + (freq - targetFreq) * glide;
        phase += freq / fs;
        phase -= std::floor (phase);
        const double p = 2.0 * 3.14159265358979 * phase;

        Sample s;
        const double s1 = std::sin (p), s2 = std::sin (2 * p), s3 = std::sin (3 * p), s5 = std::sin (5 * p), s7 = std::sin (7 * p);
        s.humClean = (float) ((s1 + 0.12 * s2 + 0.18 * s3 + 0.05 * s5) * 0.74);
        s.humBright = (float) ((0.6 * s1 + 0.25 * s2 + 0.45 * s3 + 0.3 * s5 + 0.2 * s7 + 0.12 * std::sin (9 * p)) * 0.55);

        if (buzzAmount > 0.0f)
        {
            // rectified-mains shaped pulses at 2x mains (dimmer / supply buzz), softened
            const double r = std::abs (s1);
            s.buzz = (float) ((std::pow (r, 10.0) - 0.18) * 1.1);
        }

        if (rfAmount > 0.0f)
        {
            if (--burstCounter <= 0)
            {
                burstOn = ! burstOn && rng.uniform() < 0.35 + 0.6 * rfAmount;
                burstCounter = (int) (fs * (burstOn ? rng.range (0.03, 0.25) : rng.range (0.15, 1.6) * (1.2 - rfAmount)));
                burstCounter = std::max (1, burstCounter);
            }
            if (burstOn && rng.uniform() < 0.004)
                rfAmTarget = rng.uniform();
            const double target = burstOn ? rfAmTarget : 0.0;
            rfAm += (target - rfAm) * amC;
            const double n = rng.bipolar();
            rfLp1 += (n - rfLp1) * lpC;
            rfLp2 += (rfLp1 - rfLp2) * lpC;
            s.rf = (float) std::clamp ((rfLp1 - rfLp2) * 5.0 * rfAm, -1.0, 1.0);
        }
        else
            rfAm = 0.0;

        return s;
    }

    double getPhase() const noexcept { return phase; }

private:
    double fs = 48000.0, glide = 0.99, phase = 0.0;
    double freq = 50.0, targetFreq = 50.0;
    float buzzAmount = 0.0f, rfAmount = 0.0f;
    Rng rng;
    bool burstOn = false;
    int burstCounter = 0;
    double rfLp1 = 0, rfLp2 = 0, rfAm = 0, rfAmTarget = 0, lpC = 0.3, amC = 0.001;
};

} // namespace ifx
