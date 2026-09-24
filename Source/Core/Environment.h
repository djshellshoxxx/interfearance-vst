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
        float rf = 0.0f;        // square-law detected transmitter (peak ~1) x field strength; zero when rf amount is 0
        float gsm = 0.0f;       // 2G phone burst interference (peak ~1), zero when the phone is away
    };

    enum class BuzzType { Supply = 0, Dimmer = 1 };
    enum class Phone { Away = 0, CheckIn, Incoming, InCall };
    enum class RfType { AmStation = 0, TwoWay = 1, Motor = 2 };

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
        prepareRf();
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
        resetRf();
        rfSnap = true;          // the next setRf / setRfType start settled (seek == fresh start)
        rfTypeSnap = true;
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
    /** Interference amount = RF field strength at the gear, linear 0..1 (RF level 20 log10 a dB).
        `rf` is the detected waveform for a unit field times the (smoothed) field; Source::envWave
        multiplies by the amount once more, so the injected level follows the square law:
        -10 dB of RF is -20 dB of noise (Jim Brown, "RFI in Audio Systems"). */
    void setRf (float amount) noexcept
    {
        rfAmount = std::clamp (amount, 0.0f, 1.0f);
        if (rfSnap)
        {
            rfA = rfAmount;
            rfSnap = false;
        }
    }
    /** 0 = AM station, 1 = two-way radio (default), 2 = motor sparks. A change fades the
        detected output out and back in over 10 ms. */
    void setRfType (int type) noexcept
    {
        rfTypeReq = std::clamp (type, 0, 2);
        if (rfTypeSnap)
        {
            if (rfTypeReq != rfTypeActive)
                activateRfType (rfTypeReq);
            rfTypeGain = 1.0;
            rfTypeSnap = false;
        }
    }
    /** Motor sparks generated since the last reset (test / telemetry). */
    uint64_t rfSparkCount() const noexcept { return sparkCount; }
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

        s.rf = rfAmount > 0.0f || rfA > 0.0 ? (float) rfStep() : 0.0f;

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

    //==========================================================================
    // RF interference (Docs/research/ElectricalInterference.md 1.5, 5.7). A junction in the audio
    // circuit is a square-law detector: the audio it produces is k * e(t)^2, where e(t) is the
    // envelope of the RF reaching it, followed by the stage's own DC coupling and band limit.
    // The three transmitters differ only in e(t). All randomness comes from `rng`.

    /** Minimal TPT state-variable band-pass (unity peak gain) for the synthetic AM programme. */
    struct Svf
    {
        double ic1 = 0.0, ic2 = 0.0;
        double process (double x, double f, double q, double sr) noexcept
        {
            const double g = std::tan (kPiD * std::min (f, sr * 0.45) / sr), k = 1.0 / q;
            const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return k * v1;
        }
        void reset() noexcept { ic1 = ic2 = 0.0; }
    };

    void prepareRf() noexcept
    {
        lpC = 1.0 - std::exp (-2.0 * kPiD * 2500.0 / fs);   // two-way chatter band (as before)
        amC = 1.0 - std::exp (-2.0 * kPiD * 6.0 / fs);      // two-way strength wander (as before)
        rfSmooth = 1.0 - std::exp (-1.0 / (0.02 * fs));     // field strength glide
        rfFadeStep = 1.0 / (0.01 * fs);                      // transmitter switch fade
        keyStep = 1.0 / (0.002 * fs);                        // PTT key rise / fall
        rfDcR = std::exp (-2.0 * kPiD * 20.0 / fs);          // DC coupling of the detecting stage
        noiseScale = std::sqrt (fs / 48000.0);               // white-noise density independent of the rate
        fadeC = 1.0 - std::exp (-1.0 / (0.6 * fs));          // AM carrier fading
        glideC = 1.0 - std::exp (-1.0 / (0.025 * fs));       // formant glides
        sparkDecay = std::exp (-2.0 / (kSparkTau * fs));     // squared spark envelope decays at 2 / tau
        motorRateC = 1.0 - std::exp (-1.0 / (0.3 * fs));
        setRxCutoff (rfTypeActive);
    }

    void setRxCutoff (int type) noexcept
    {
        static const double rx[3] = { 4500.0, 3000.0, 7000.0 }; // AM programme, voice radio, spark crackle
        rxC = 1.0 - std::exp (-2.0 * kPiD * std::min (rx[std::clamp (type, 0, 2)], fs * 0.45) / fs);
    }

    void resetRf() noexcept
    {
        rfDcX1 = rfDcY1 = rfRx1 = rfRx2 = 0.0;
        // two-way
        burstOn = false;
        burstCounter = 0;
        rfLp1 = rfLp2 = rfAm = 0.0;
        rfAmTarget = 0.0;
        keyLevel = 0.0;
        // AM station
        f1.reset();
        f2.reset();
        amG = amGTarget = 0.8;
        fadeCounter = 0;
        sylLeft = 0;
        sylPos = sylLen = 0;
        sylAmp = 0.0;
        pauseLeft = 0;
        f1Hz = f1Target = 500.0;
        f2Hz = f2Target = 1500.0;
        speaker = 1.0;
        // motor
        sparkE2 = 0.0;
        motorRate = motorRateTarget = 150.0;
        motorCounter = 0;
        sparkCount = 0;
    }

    void activateRfType (int type) noexcept
    {
        const uint64_t sparks = sparkCount;
        rfTypeActive = type;
        resetRf();
        sparkCount = sparks;
        setRxCutoff (type);
    }

    /** One sample of rf: unit-field detected waveform times the smoothed field strength. */
    double rfStep() noexcept
    {
        rfA += ((double) rfAmount - rfA) * rfSmooth;
        if (rfAmount <= 0.0f && rfA < 1.0e-6)
        {
            rfA = 0.0;                   // fully faded out: stop and restart clean next time
            resetRf();
            return 0.0;
        }
        // transmitter switch: fade out, switch while silent, fade back in
        if (rfTypeReq != rfTypeActive)
        {
            rfTypeGain -= rfFadeStep;
            if (rfTypeGain <= 0.0)
            {
                rfTypeGain = 0.0;
                activateRfType (rfTypeReq);
            }
        }
        else if (rfTypeGain < 1.0)
            rfTypeGain = std::min (1.0, rfTypeGain + rfFadeStep);

        double d = 0.0, gain = 1.0;          // detected signal for a unit field, output normalisation
        switch (rfTypeActive)
        {
            case 0:  { const double e = amStationEnvelope(); d = e * e; gain = 0.5; break; }   // square law
            case 2:  { d = motorDetected(); gain = 2.0; break; }                                // spark energy
            default: { const double e = twoWayEnvelope(); d = e * e; gain = 0.7; break; }      // square law
        }
        // DC coupling of the detecting stage, then its band limit (two poles)
        const double hp = d - rfDcX1 + rfDcR * rfDcY1;
        rfDcX1 = d;
        rfDcY1 = std::abs (hp) < 1.0e-20 ? 0.0 : hp;
        rfRx1 += (rfDcY1 - rfRx1) * rxC;
        rfRx2 += (rfRx1 - rfRx2) * rxC;
        return rfA * rfTypeGain * std::clamp (rfRx2 * gain, -1.0, 1.0);
    }

    /** Keyed two-way traffic: short transmissions (30..250 ms) with gaps of 0.12..1.3 s; the
        traffic does not depend on the field strength. While keyed, the carrier is modulated by
        band-limited chatter; key up / down become thumps after the DC coupling. */
    double twoWayEnvelope() noexcept
    {
        if (--burstCounter <= 0)
        {
            if (burstOn)
                burstOn = false;
            else
            {
                burstOn = rng.uniform() < 0.85;
                if (burstOn) rfAmTarget = rng.range (0.4, 1.0);
            }
            burstCounter = std::max (1, (int) (fs * (burstOn ? rng.range (0.03, 0.25) : rng.range (0.12, 1.3))));
        }
        if (burstOn && rng.uniform() < 190.0 / fs)
            rfAmTarget = rng.range (0.4, 1.0);
        rfAm += (rfAmTarget - rfAm) * amC;
        keyLevel = burstOn ? std::min (1.0, keyLevel + keyStep) : std::max (0.0, keyLevel - keyStep);
        const double n = rng.bipolar() * noiseScale;
        rfLp1 += (n - rfLp1) * lpC;
        rfLp2 += (rfLp1 - rfLp2) * lpC;
        const double x = (rfLp1 - rfLp2) * 2.5;
        return std::max (0.0, keyLevel * rfAm * (1.0 + 0.6 * x));
    }

    /** A distant AM station: continuous carrier with slow fading, modulated by a SYNTHETIC,
        unintelligible programme: whispered (noise-excited) two-formant syllables at 2.5..7 Hz in
        phrases of 3..9 with short pauses. No pitch pulses, so no tonal line in the audio. */
    double amStationEnvelope() noexcept
    {
        if (--fadeCounter <= 0)
        {
            amGTarget = rng.range (0.55, 1.0);
            fadeCounter = std::max (1, (int) (fs * rng.range (1.0, 4.0)));
        }
        amG += (amGTarget - amG) * fadeC;

        double env = 0.0;
        if (pauseLeft > 0)
            --pauseLeft;
        else
        {
            if (sylPos >= sylLen)
            {
                if (sylLeft <= 0)
                {
                    sylLeft = 3 + (int) (rng.uniform() * 7.0);          // a new phrase
                    speaker = rng.range (0.9, 1.15);
                }
                --sylLeft;
                static const double vowels[8][2] = { { 730, 1090 }, { 270, 2290 }, { 530, 1840 }, { 660, 1720 },
                                                     { 300, 870 },  { 570, 840 },  { 440, 1020 }, { 490, 1350 } };
                const int v = std::min (7, (int) (rng.uniform() * 8.0));
                f1Target = vowels[v][0] * speaker;
                f2Target = vowels[v][1] * speaker;
                sylLen = std::max (1, (int) (fs / rng.range (2.5, 7.0)));
                sylAmp = rng.range (0.5, 1.0);
                sylPos = 0;
            }
            const double t = (double) sylPos / (double) sylLen;
            const double sn = std::sin (kPiD * t);
            env = sylAmp * sn * sn;
            if (++sylPos >= sylLen && sylLeft <= 0)
                pauseLeft = std::max (1, (int) (fs * rng.range (0.15, 0.6)));
        }
        f1Hz += (f1Target - f1Hz) * glideC;
        f2Hz += (f2Target - f2Hz) * glideC;
        const double n = rng.bipolar() * noiseScale;
        const double voice = f1.process (n, f1Hz, 5.0, fs) + 0.7 * f2.process (n, f2Hz, 7.0, fs);
        const double prog = std::clamp (5.0 * env * voice, -1.5, 1.5);
        return std::max (0.0, amG * (1.0 + 0.8 * prog));
    }

    /** Brush-motor commutation sparks: Poisson impulses at 60..250 per second (the rate wanders
        with the motor's load). Each spark is a short broadband RF burst whose envelope decays in
        kSparkTau; its detected energy A^2 tau / 2 is added directly (sample-rate independent). */
    double motorDetected() noexcept
    {
        if (--motorCounter <= 0)
        {
            motorRateTarget = rng.range (60.0, 250.0);
            motorCounter = std::max (1, (int) (fs * rng.range (0.5, 2.0)));
        }
        motorRate += (motorRateTarget - motorRate) * motorRateC;
        sparkE2 *= sparkDecay;
        if (rng.uniform() < motorRate / fs)
        {
            const double a = std::min (1.6, 0.6 * std::exp (0.4 * rng.gaussian()));
            sparkE2 += a * a * kSparkTau * 0.5 * fs * (1.0 - sparkDecay);
            ++sparkCount;
        }
        return sparkE2;
    }

    static constexpr double kSparkTau = 50.0e-6;

    double fs = 48000.0, glide = 0.99, phase = 0.0;
    double freq = 50.0, targetFreq = 50.0;
    float buzzAmount = 0.0f, rfAmount = 0.0f;
    // RF
    double rfA = 0.0, rfSmooth = 0.001, rfTypeGain = 1.0, rfFadeStep = 0.002, keyStep = 0.01, keyLevel = 0.0;
    double rfDcR = 0.997, rfDcX1 = 0.0, rfDcY1 = 0.0, rxC = 0.3, rfRx1 = 0.0, rfRx2 = 0.0, noiseScale = 1.0;
    int rfTypeReq = 1, rfTypeActive = 1;
    bool rfSnap = true, rfTypeSnap = true;
    Svf f1, f2;
    double amG = 0.8, amGTarget = 0.8, fadeC = 0.0001, glideC = 0.001, f1Hz = 500.0, f2Hz = 1500.0, f1Target = 500.0,
           f2Target = 1500.0, speaker = 1.0, sylAmp = 0.0;
    int fadeCounter = 0, sylLeft = 0, sylPos = 0, sylLen = 0, pauseLeft = 0;
    double sparkE2 = 0.0, sparkDecay = 0.4, motorRate = 150.0, motorRateTarget = 150.0, motorRateC = 0.0001;
    int motorCounter = 0;
    uint64_t sparkCount = 0;
    BuzzType buzzType = BuzzType::Supply;
    Phone phone = Phone::Away;
    Rng rng, gsmRng;
    bool burstOn = false;
    int burstCounter = 0;
    double rfLp1 = 0, rfLp2 = 0, rfAm = 0, rfAmTarget = 0, lpC = 0.3, amC = 0.001; // two-way

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
