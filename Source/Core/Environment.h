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
        // harmonics by the Chebyshev recurrence sin(n p) = 2 cos(p) sin((n-1) p) - sin((n-2) p):
        // two transcendental calls per sample instead of six
        const double s1 = std::sin (p), c2 = 2.0 * std::cos (p);
        const double s2 = c2 * s1, s3 = c2 * s2 - s1, s4 = c2 * s3 - s2, s5 = c2 * s4 - s3;
        const double s6 = c2 * s5 - s4, s7 = c2 * s6 - s5, s8 = c2 * s7 - s6, s9 = c2 * s8 - s7;
        s.humClean = (float) ((s1 + 0.12 * s2 + 0.18 * s3 + 0.05 * s5) * 0.74);
        s.humBright = (float) ((0.6 * s1 + 0.25 * s2 + 0.45 * s3 + 0.3 * s5 + 0.2 * s7 + 0.12 * s9) * 0.55);

        if (buzzAmount > 0.0f)
        {
            // rectified-mains shaped pulses at 2x mains (dimmer / supply buzz), softened
            const double r = std::abs (s1), r2 = r * r, r4 = r2 * r2, r5 = r4 * r;
            s.buzz = (float) ((r5 * r5 - 0.18) * 1.1);
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
