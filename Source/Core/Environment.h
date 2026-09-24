#pragma once

#include "Random.h"
#include <cmath>
#include <algorithm>

namespace ifx
{

/** One-sample-delayed polyBLEP: removes most aliasing from the steps of a naive waveform.
    Call add() for every step inside the current sample interval, then out() once. */
struct DelayedBlep
{
    /** u = time since the step in samples (0..1), h = step height (after - before). */
    void add (double u, double h) noexcept
    {
        prevCorr += h * u * u * 0.5;            // sample before the step
        curCorr -= h * (1.0 - u) * (1.0 - u) * 0.5; // sample after the step
    }
    double out (double naive) noexcept
    {
        const double y = prevNaive + prevCorr;
        prevNaive = naive;
        prevCorr = curCorr;
        curCorr = 0.0;
        return y;
    }
    void reset() noexcept { prevNaive = prevCorr = curCorr = 0.0; }
    double prevNaive = 0.0, prevCorr = 0.0, curCorr = 0.0;
};

/** The shared electrical environment. One mains phase for the whole plugin (two slots never
    get two independent hum generators). Phase is continuous across blocks and 50<->60 Hz
    changes glide over ~60 ms. On a transport reset the phase is re-derived from absolute
    host time so a bounce is repeatable.

    Hum colour comes from the coupling path, not from noise (Docs/research/ElectricalInterference.md
    5.1-5.2): the mains voltage is a sine with small odd harmonics; resistive / magnetic pickup of
    linear currents keeps that shape (humClean), capacitive (electric-field) pickup and leakage
    currents weight harmonic n by ~n (humBright). Neither has a 2nd harmonic: even harmonics mean a
    rectifier (the Supply buzz). */
class Environment
{
public:
    struct Sample
    {
        float humClean = 0.0f;  // line-voltage shaped pickup (round hum), peak ~1
        float humBright = 0.0f; // capacitive / leakage pickup, odd harmonics weighted by n, peak ~1
        float buzz = 0.0f;      // Buzz Band: supply (2x mains pulses) or dimmer (firing spikes), peak ~1
        float rf = 0.0f;        // sparse RF-like demodulated chatter (peak ~1), zero when rf amount is 0
        float gsm = 0.0f;       // 2G phone burst interference (peak ~1), zero when the phone is away
    };

    enum class BuzzType { Supply = 0, Dimmer = 1 };
    enum class Phone { Away = 0, CheckIn, Incoming, InCall };

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        glide = std::exp (-1.0 / (0.02 * fs));
        const double w = 2.0 * kPiD * 2500.0 / fs;
        lpC = 1.0 - std::exp (-w);
        amC = 1.0 - std::exp (-2.0 * kPiD * 6.0 / fs);
        gsmHp = std::exp (-2.0 * kPiD * 100.0 / fs);
        gsmLp = 1.0 - std::exp (-2.0 * kPiD * 4000.0 / fs);
        gsmDrift = 1.0 - std::exp (-1.0 / (0.8 * fs));
        reset (0.0);
    }

    void seed (uint64_t s) noexcept
    {
        rng.seed (hashCombine (s, 0xE11Full));
        gsmRng.seed (hashCombine (s, 0x65A2ull)); // separate stream: RF takes stay unchanged
    }

    void reset (double hostSeconds) noexcept
    {
        freq = targetFreq;
        const double cycles = hostSeconds * freq;
        phase = cycles - std::floor (cycles);
        burstOn = false;
        burstCounter = 0;
        rfLp1 = rfLp2 = rfAm = 0.0;
        rfAmTarget = 0.0;
        dimI = dimPrevI = 0.0;
        dimBlep.reset();
        prevTheta = -1.0;
        framePhase = 0.0;
        frameCount = 0;
        gsmBlep.reset();
        gsmHpState = gsmHpPrev = gsmLp1 = gsmLp2 = 0.0;
        gsmActive = gsmRunning = false;
        gsmCounter = 0;
        gsmClusterLeft = 0;
        gsmLevel = gsmLevelTarget = 1.0;
        gsmDriftCounter = 0;
    }

    void setMainsFrequency (float hz) noexcept { targetFreq = hz; }
    void setBuzz (float amount) noexcept { buzzAmount = amount; }
    void setRf (float amount) noexcept { rfAmount = amount; }
    void setBuzzType (BuzzType t) noexcept { buzzType = t; }
    /** brightness 0..1 (0 = dark, never fires), kind 0 = cheap wall (5 us rise), 1 = stage choke
        (350 us), 2 = TV-studio choke (800 us). */
    void setDimmer (float brightness, int kind) noexcept
    {
        dimAlpha = kPiD * (1.0 - std::clamp ((double) brightness, 0.0, 1.0));
        static const double rise[] = { 5.0e-6, 350.0e-6, 800.0e-6 };
        const double tr = rise[std::clamp (kind, 0, 2)];
        dimK = 1.0 - std::exp (-1.0 / (tr * fs));
    }
    void setPhone (Phone p) noexcept
    {
        if (p != phone)
        {
            phone = p;
            gsmCounter = 0;      // re-plan at the next frame boundary instead of after the old segment
            gsmClusterLeft = 0;
        }
    }

    Sample step() noexcept
    {
        freq = targetFreq + (freq - targetFreq) * glide;
        const double prevPhase = phase;
        phase += freq / fs;
        phase -= std::floor (phase);
        const double p = 2.0 * kPiD * phase;

        Sample s;
        // odd harmonics by the Chebyshev recurrence sin(n p) = 2 cos(p) sin((n-1) p) - sin((n-2) p)
        const double s1 = std::sin (p), c2 = 2.0 * std::cos (p);
        double sm2 = 0.0, sm1 = s1, h[14] {};
        h[1] = s1;
        for (int n = 2; n <= 13; ++n)
        {
            const double sn = c2 * sm1 - sm2;
            h[n] = sn;
            sm2 = sm1;
            sm1 = sn;
        }
        // line voltage harmonics (EN 50160-scale flat-topping, starting points, unmeasured)
        static const double a[14] = { 0, 1.0, 0, 0.03, 0, 0.045, 0, 0.025, 0, 0.01, 0, 0.02, 0, 0.015 };
        double clean = 0.0, bright = 0.0;
        for (int n = 1; n <= 13; n += 2)
        {
            clean += a[n] * h[n];
            bright += a[n] * n * h[n];
        }
        s.humClean = (float) (clean * 0.93);
        s.humBright = (float) (bright * 0.62);

        if (buzzAmount > 0.0f)
        {
            if (buzzType == BuzzType::Supply)
            {
                // capacitor-input rectifier: current pulses at 2x mains near the voltage peaks
                const double r = std::abs (s1), r2 = r * r, r4 = r2 * r2, r5 = r4 * r;
                s.buzz = (float) ((r5 * r5 - 0.246) * 1.1);
            }
            else
                s.buzz = (float) dimmerStep (prevPhase);
        }
        else
            prevTheta = -1.0;

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

        if (phone != Phone::Away || gsmRunning)
        {
            s.gsm = (float) gsmStep();
            // after Away the current burst completes and the receiver filters ring out
            gsmRunning = phone != Phone::Away || gsmActive || std::abs (s.gsm) > 1.0e-6f;
        }
        return s;
    }

    double getPhase() const noexcept { return phase; }

    static constexpr double kPiD = 3.14159265358979323846;

private:
    /** Phase-control (triac) dimmer on a resistive load: the load current is zero until the
        firing angle, then follows the mains; the induced buzz is its (rise-time limited)
        derivative. Odd harmonics of the mains, loudest near half brightness, locked to mains. */
    double dimmerStep (double prevPhase) noexcept
    {
        const double dTheta = 2.0 * kPiD * freq / fs;
        const double theta = std::fmod (phase * 2.0 * kPiD, kPiD);
        const double sign = phase < 0.5 ? 1.0 : -1.0;
        const bool first = prevTheta < 0.0;
        const bool newHalf = first || theta < prevTheta || (prevPhase < 0.5) != (phase < 0.5);
        if (newHalf)
        {
            // the triac fires once per half-cycle at the angle latched at its zero crossing and
            // conducts until the next zero crossing (automation cannot switch it off mid-cycle)
            alphaHalf = dimAlpha;
            fired = first && alphaHalf < kPiD && theta >= alphaHalf; // after a reset: already conducting
        }
        if (! fired && alphaHalf < kPiD && theta >= alphaHalf)
        {
            fired = true;
            if (alphaHalf > 0.0)
                dimBlep.add (std::clamp ((theta - alphaHalf) / dTheta, 0.0, 1.0), sign * std::sin (alphaHalf));
        }
        const double naive = fired ? sign * std::sin (theta) : 0.0;
        prevTheta = theta;
        if (first)
        {
            dimBlep.prevNaive = naive; // start in steady state: no tick on enable / reset
            dimI = dimPrevI = naive;
        }
        const double iBl = dimBlep.out (naive);
        dimI += (iBl - dimI) * dimK;                 // rise time of the switching current
        const double dIdt = (dimI - dimPrevI) * fs / (2.0 * kPiD * 9000.0);
        dimPrevI = dimI;
        const double guard = 1.5 * std::max (1.0, fs / 48000.0); // numeric guard only; the injection low-pass sets the peak
        return std::clamp (dIdt * 2.0, -guard, guard);
    }

    /** 2G (GSM/TDMA) handset: 577 us bursts every 4.615 ms (216.7 Hz), every 26th frame idle,
        detected by the audio circuit (100 % AM, unipolar) and band-limited by the receiver.
        Call states only change WHEN bursts happen, never the physical rhythm. */
    double gsmStep() noexcept
    {
        constexpr double frameHz = 5200.0 / 24.0; // 216.67 Hz
        constexpr double duty = 1.0 / 8.0;
        const double inc = frameHz / fs;
        const double prev = framePhase;
        framePhase += inc;

        // burst sequencer: decides at each frame start whether this frame transmits
        if (framePhase >= 1.0)
        {
            framePhase -= 1.0;
            ++frameCount;
            if (--gsmCounter <= 0)
                nextGsmSegment();
        }
        const bool frameOn = gsmActive && (frameCount % 26) != 25;
        const double naive = frameOn && framePhase < duty ? 1.0 : 0.0;

        // band-limited edges: rising at the frame start, falling at 1/8 frame
        if (framePhase < prev) // frame boundary crossed in this sample
        {
            const double u = framePhase / inc;
            if (frameOn) gsmBlep.add (std::clamp (u, 0.0, 1.0), 1.0);
        }
        if (prev < duty && framePhase >= duty && frameOn)
            gsmBlep.add (std::clamp ((framePhase - duty) / inc, 0.0, 1.0), -1.0);
        const double x = gsmBlep.out (naive);

        // slow power-control drift (+-3 dB)
        if (--gsmDriftCounter <= 0)
        {
            gsmDriftCounter = (int) (fs * 0.5);
            gsmLevelTarget = std::pow (10.0, gsmRng.range (-3.0, 3.0) / 20.0);
        }
        gsmLevel += (gsmLevelTarget - gsmLevel) * gsmDrift;

        // receiver: high-pass ~100 Hz, then two one-pole low-passes ~4 kHz (1/f^2 harmonics)
        const double hp = gsmHp * (gsmHpState + x - gsmHpPrev);
        gsmHpState = hp;
        gsmHpPrev = x;
        gsmLp1 += (hp - gsmLp1) * gsmLp;
        gsmLp2 += (gsmLp1 - gsmLp2) * gsmLp;
        return std::clamp (gsmLp2 * 1.6 * gsmLevel, -1.5, 1.5);
    }

    void nextGsmSegment() noexcept
    {
        const double frames = frameHzRate();
        switch (phone)
        {
            case Phone::InCall:
                gsmActive = true;
                gsmCounter = (int) frames; // re-evaluate every second
                break;
            case Phone::CheckIn:
                // short location-update bursts a few times, separated by quiet seconds
                gsmActive = ! gsmActive && gsmRng.uniform() < 0.8;
                gsmCounter = (int) (frames * (gsmActive ? gsmRng.range (0.3, 1.0) : gsmRng.range (2.0, 8.0)));
                break;
            case Phone::Incoming:
                // handshake: irregular clusters of dit-dit bursts, then a pause
                if (gsmClusterLeft > 0)
                {
                    gsmActive = ! gsmActive;
                    if (! gsmActive) --gsmClusterLeft;
                    gsmCounter = (int) (frames * (gsmActive ? gsmRng.range (0.05, 0.2) : gsmRng.range (0.08, 0.35)));
                }
                else
                {
                    gsmActive = false;
                    gsmClusterLeft = 3 + (int) (gsmRng.uniform() * 8.0);
                    gsmCounter = (int) (frames * gsmRng.range (1.0, 3.0));
                }
                break;
            case Phone::Away:
            default:
                gsmActive = false;
                gsmCounter = (int) frames;
                break;
        }
        gsmCounter = std::max (1, gsmCounter);
    }

    static double frameHzRate() noexcept { return 5200.0 / 24.0; } // frames per second

    double fs = 48000.0, glide = 0.99, phase = 0.0;
    double freq = 50.0, targetFreq = 50.0;
    float buzzAmount = 0.0f, rfAmount = 0.0f;
    BuzzType buzzType = BuzzType::Supply;
    Phone phone = Phone::Away;
    Rng rng, gsmRng;
    bool burstOn = false;
    int burstCounter = 0;
    double rfLp1 = 0, rfLp2 = 0, rfAm = 0, rfAmTarget = 0, lpC = 0.3, amC = 0.001;

    // dimmer
    double dimAlpha = kPiD * 0.5, alphaHalf = kPiD, dimK = 1.0, dimI = 0.0, dimPrevI = 0.0, prevTheta = -1.0;
    bool fired = false;
    DelayedBlep dimBlep;

    // GSM
    double framePhase = 0.0, gsmHp = 0.99, gsmLp = 0.4, gsmDrift = 0.0001;
    double gsmHpState = 0.0, gsmHpPrev = 0.0, gsmLp1 = 0.0, gsmLp2 = 0.0, gsmLevel = 1.0, gsmLevelTarget = 1.0;
    int64_t frameCount = 0;
    int gsmCounter = 0, gsmClusterLeft = 0, gsmDriftCounter = 0;
    bool gsmActive = false, gsmRunning = false;
    DelayedBlep gsmBlep;
};

} // namespace ifx
