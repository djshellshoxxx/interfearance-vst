#pragma once

#include "Filters.h"
#include "../Core/Params.h"
#include "../Core/Random.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>

namespace ifx
{

/** Delay line with a preallocated buffer (allocated in prepare only). */
struct DelayLine
{
    std::vector<float> buf;
    int w = 0;
    void allocate (int samples) { buf.assign ((size_t) std::max (4, samples), 0.0f); w = 0; }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) noexcept { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    /** Read `d` samples behind the most recently pushed sample (fractional, linear). d >= 1. */
    float read (double d) const noexcept
    {
        const int n = (int) buf.size();
        d = std::clamp (d, 1.0, (double) n - 2.0);
        const int i = (int) d;
        const float f = (float) (d - i);
        int a = w - i; if (a < 0) a += n;
        int b = a - 1; if (b < 0) b += n;
        return buf[(size_t) a] + (buf[(size_t) b] - buf[(size_t) a]) * f;
    }
};

/** Per-block musical context for the Studio chain. */
struct StudioBlockInfo
{
    double bpm = 120.0;
    double ppqStart = 0.0;
    bool clockRunning = false;
    bool midiGateOpen = false;
    float sidechainEnv = 0.0f;
    bool sidechainPresent = false;
    int resonatorNote = -1;
};

/** The fixed-order Studio chain (routing spec section 3). Stereo; every stage has its own
    bypass and wet amount; the local feedback loop taps after the Resonator and returns into
    the Drive input with a loop gain clamped below 1 so every tail is finite. */
class StudioChain
{
public:
    enum Stage { EQ = 0, Contour, Drive, Crush, Resonator, Gate, Echo, NumStages };

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        fs = sampleRate;
        const int maxEcho = (int) (fs * 2.1);
        const int maxFb = (int) (fs * 0.55);
        for (int ch = 0; ch < 2; ++ch)
        {
            echo[ch].allocate (maxEcho);
            fbLine[ch].allocate (maxFb);
        }
        stageSmooth = (float) std::exp (-1.0 / (0.01 * fs));
        stageStep = (float) (1.0 / (0.01 * fs));
        // time constants in seconds (the old per-sample constants at 44.1 kHz), so the Studio
        // sounds the same at every sample rate
        auto coef = [this] (double sec) { return (float) (1.0 - std::exp (-1.0 / (sec * fs))); };
        kFastAtt = coef (0.00011); kFastRel = coef (0.011);
        kSlowAtt = coef (0.0045);  kSlowRel = coef (0.075);
        kCtGain = coef (0.0023);   kRms = coef (0.113); kAuto = coef (0.045);
        kGlide = coef (0.03);
        for (auto& d : fbDc) d.prepare (fs, 10.0); // coefficient only; never reset per block
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (auto& b : eq[ch]) b.reset();
            envFast[ch] = envSlow[ch] = 0.0f;
            ctGain[ch] = 1.0f;
            sat[ch].reset();
            driveDc[ch].reset();
            toneLp[ch].reset();
            holdVal[ch] = 0.0f;
            res1[ch].reset();
            res2[ch].reset();
            resFbState[ch] = 0.0f;
            echo[ch].clear();
            echoLp[ch].reset();
            fbLine[ch].clear();
            fbHp[ch].reset();
            fbLp[ch].reset();
            fbDc[ch].reset();
            rmsIn[ch] = rmsOut[ch] = 0.0f;
        }
        holdPhase = 0.0;
        gateEnv = 1.0f;
        for (auto& s : stageMix) s = 0.0f;
        fbReturn = 0.0f;
        killRamp = 1.0f;
        tailRamp = 1.0f;
        autoGain = 1.0f;
        ditherRng.seed (0xD17E);
        chokeActive = false;
        fbCleared = tailCleared = false;
        snap = true;
        for (int st = 0; st < NumStages; ++st) pkIn[st] = pkOut[st] = 0.0f;
    }

    void setParams (const ParamSnapshot& p, const StudioBlockInfo& info) noexcept
    {
        prm = &p;
        bi = info;
        on[EQ] = p.on (P::eqOn);
        on[Contour] = p.on (P::ctOn);
        on[Drive] = p.on (P::drOn);
        on[Crush] = p.on (P::crOn);
        on[Resonator] = p.on (P::rsOn);
        on[Gate] = p.on (P::gtOn);
        on[Echo] = p.on (P::ecOn);

        // EQ coefficients (block rate)
        const bool hp = p[P::eqHP] > 20.5f, lp = p[P::eqLP] < 19999.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            if (hp) eq[ch][0].highpass (p[P::eqHP], 0.7071, fs); else eq[ch][0].setIdentity();
            if (lp) eq[ch][1].lowpass (p[P::eqLP], 0.7071, fs); else eq[ch][1].setIdentity();
            const float tilt = p[P::eqTilt];
            if (std::abs (tilt) > 0.01f) { eq[ch][2].lowShelf (1000.0, -tilt * 0.5, fs); eq[ch][3].highShelf (1000.0, tilt * 0.5, fs); }
            else { eq[ch][2].setIdentity(); eq[ch][3].setIdentity(); }
            if (std::abs (p[P::eqPkGain]) > 0.01f) eq[ch][4].peak (p[P::eqPkFreq], 0.7, p[P::eqPkGain], fs); else eq[ch][4].setIdentity();
        }

        // Drive
        const int curve = p.choice (P::drCurve);
        for (auto& s : sat) s.curve = (AdaaSaturator::Curve) curve;
        drivePreT = dbToGain (p[P::drPre]);
        drivePostT = dbToGain (p[P::drPost]);
        toneLp[0].setCutoff (800.0 + 15000.0 * p[P::drTone], fs);
        toneLp[1].a = toneLp[0].a;

        // Crush
        crushStep = 2.0f / std::pow (2.0f, p[P::crBits] - 1.0f) * 0.5f;
        holdInc = std::min (1.0, (double) p[P::crRate] / fs);

        // Resonator (constant 0 dB peak bandpass; Q from decay time)
        double f1 = p[P::rsFreq];
        if (p.on (P::rsMidi) && info.resonatorNote >= 0)
            f1 = 440.0 * std::pow (2.0, (info.resonatorNote - 69) / 12.0);
        f1 = std::clamp (f1, 20.0, fs * 0.45);
        const double f2 = std::clamp (f1 * p[P::rsRatio], 20.0, fs * 0.45);
        const double t60 = p[P::rsDecay];
        const double q1 = std::clamp (kPi * f1 * t60 / 6.91, 0.5, 2000.0);
        const double q2 = std::clamp (kPi * f2 * t60 / 6.91, 0.5, 2000.0);
        for (int ch = 0; ch < 2; ++ch)
        {
            res1[ch].bandpass (f1, q1, fs);
            res2[ch].bandpass (f2, q2, fs);
        }
        resFb = std::min (0.9f, p[P::rsFeedback]);
        // modal gain: a 0 dB-peak band-pass barely rings on clicks, so the peak gain grows
        // with Q up to +18 dB; the tanh stage below bounds sustained excitation
        resGain = (float) std::clamp (q1, 1.0, 8.0);

        // Echo
        double echoSec = p[P::ecTime] * 0.001;
        if (p.on (P::ecSync))
        {
            static const double beats[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 2.0 };
            echoSec = beats[std::clamp (p.choice (P::ecDiv), 0, 7)] * 60.0 / std::max (20.0, info.bpm);
        }
        echoDelayT = std::clamp (echoSec * fs, 1.0, fs * 2.0);
        echoFbT = std::min (0.9f, p[P::ecFeedback]);
        for (auto& l : echoLp) l.setCutoff (1500.0 + 14000.0 * p[P::ecTone], fs);

        // Local feedback: loop gain clamped below 0.97 whatever the stage gains are
        fbDelayT = std::clamp ((double) p[P::fbTime] * 0.001 * fs, 1.0, fs * 0.5);
        for (int ch = 0; ch < 2; ++ch)
        {
            fbHp[ch].setCutoff (p[P::fbHP], fs);
            fbLp[ch].setCutoff (p[P::fbLP], fs);
        }
        autoMatch = p.on (P::drAuto);
        double loopGain = 1.0;
        if (on[Drive]) loopGain *= (double) std::max (drivePreT, drivePre) * (double) std::max (drivePostT, drivePost);
        if (on[Resonator]) loopGain *= (double) resGain / (1.0 - resFb);
        if (autoMatch && (on[Drive] || on[Crush])) loopGain *= 4.0; // Auto Match gain sits inside the loop
        const float requested = p[P::fbAmount];
        fbTarget = (float) std::min ((double) requested, 0.97 / std::max (1.0, loopGain));
        fbLimited = requested > 0.0f && fbTarget < requested - 1.0e-4f;
        killFb = p.on (P::killFb);
        killTail = p.on (P::killTail);

        // Gate
        gateMode = p.choice (P::gtMode);
        gateKey = p.choice (P::gtKey);
        static const double divBeats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0 };
        gateDiv = divBeats[std::clamp (p.choice (P::gtDiv), 0, 6)];
        gateAtt = (float) std::exp (-1.0 / (p[P::gtAttack] * 0.001 * fs));
        gateRel = (float) std::exp (-1.0 / (p[P::gtRelease] * 0.001 * fs));
        gateFloor = 1.0f - p[P::gtDepth];

        wetT[EQ] = p[P::eqMix]; wetT[Contour] = p[P::ctMix]; wetT[Drive] = p[P::drMix]; wetT[Crush] = p[P::crMix];
        wetT[Resonator] = p[P::rsMix]; wetT[Gate] = 1.0f; wetT[Echo] = p[P::ecMix];
        if (snap)
        {
            snap = false;
            drivePre = drivePreT; drivePost = drivePostT;
            echoDelay = echoDelayT; echoFb = echoFbT; fbDelay = fbDelayT;
            for (int st = 0; st < NumStages; ++st) wet[st] = wetT[st];
        }
        ctAtt = p[P::ctAttack];
        ctSus = p[P::ctSustain];
        dither = p[P::crDither];
        ppq = info.ppqStart;
        ppqPerSample = info.bpm / 60.0 / fs;
    }

    /** Programmed choke from a lane step (sample-accurate call from the engine). */
    void setChoke (bool c, bool killsFeedback, bool clearsEcho) noexcept
    {
        chokeActive = c;
        chokeKillsFb = killsFeedback;
        chokeClearsEcho = clearsEcho;
    }

    /** Process one stereo sample of the selected branch F. Returns P in-place. */
    void process (float (&x)[2], int numChannels) noexcept
    {
        const auto& p = *prm;
        for (int s = 0; s < NumStages; ++s)
        {
            stageMix[s] = on[s] ? std::min (1.0f, stageMix[s] + stageStep) : std::max (0.0f, stageMix[s] - stageStep);
            wet[s] = smoothTo (wet[s], wetT[s], stageSmooth);
        }
        drivePre = smoothTo (drivePre, drivePreT, stageSmooth);
        drivePost = smoothTo (drivePost, drivePostT, stageSmooth);
        echoFb = smoothTo (echoFb, echoFbT, stageSmooth);
        // delay times glide (tape-like) instead of jumping the read head
        echoDelay += (echoDelayT - echoDelay) * kGlide;
        fbDelay += (fbDelayT - fbDelay) * kGlide;

        // feedback return ramps
        const bool killNow = killFb || (chokeActive && chokeKillsFb);
        killRamp = (killNow ? 0.0f : 1.0f) + (killRamp - (killNow ? 0.0f : 1.0f)) * stageSmooth;
        fbReturn = fbTarget * killRamp;
        const bool tailKill = killTail || (chokeActive && chokeClearsEcho);
        tailRamp = (tailKill ? 0.0f : 1.0f) + (tailRamp - (tailKill ? 0.0f : 1.0f)) * stageSmooth;
        if (killNow && killRamp < 1.0e-3f)
        {
            if (! fbCleared) { for (auto& l : fbLine) l.clear(); fbCleared = true; } // once per kill
        }
        else if (! killNow)
            fbCleared = false;

        // gate target
        float gateTarget = 1.0f;
        if (on[Gate])
        {
            bool open = true;
            if (gateMode == 1) // Pulse
            {
                switch (gateKey)
                {
                    case 0: { const double ph = std::fmod (ppq / gateDiv, 1.0); open = ! bi.clockRunning || ph < 0.5; break; }
                    case 1: open = bi.midiGateOpen; break;
                    case 2: open = bi.sidechainPresent ? bi.sidechainEnv > 0.05f : true; break;
                    default: open = (envSlow[0] > 0.003f); break;
                }
            }
            else if (gateMode == 2) // Choke
                open = ! chokeActive;
            gateTarget = open ? 1.0f : gateFloor;
        }
        gateEnv = gateTarget + (gateEnv - gateTarget) * (gateTarget > gateEnv ? gateAtt : gateRel);
        ppq += ppqPerSample;

        const bool doHold = on[Crush];
        bool holdTick = false;
        if (doHold)
        {
            holdPhase += holdInc;
            if (holdPhase >= 1.0) { holdPhase -= 1.0; holdTick = true; }
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float v = x[ch];

            // 1 EQ
            if (stageMix[EQ] > 1.0e-4f)
            {
                pkIn[EQ] = std::max (pkIn[EQ], std::abs (v));
                float e = v;
                for (auto& b : eq[ch]) e = b.process (e);
                v = mixTo (v, e, stageMix[EQ] * wet[EQ]);
                pkOut[EQ] = std::max (pkOut[EQ], std::abs (v));
            }

            // 2 Contour
            {
                const float a = std::abs (v);
                envFast[ch] += (a - envFast[ch]) * (a > envFast[ch] ? kFastAtt : kFastRel);
                envSlow[ch] += (a - envSlow[ch]) * (a > envSlow[ch] ? kSlowAtt : kSlowRel);
                if (stageMix[Contour] > 1.0e-4f)
                {
                    pkIn[Contour] = std::max (pkIn[Contour], a);
                    const float d = gainToDb (envFast[ch] + 1e-9f) - gainToDb (envSlow[ch] + 1e-9f);
                    const float t = std::clamp (d / 12.0f, 0.0f, 1.0f);
                    const float gDb = std::clamp (t * ctAtt + (1.0f - t) * ctSus, -24.0f, 12.0f);
                    ctGain[ch] += (dbToGain (gDb) - ctGain[ch]) * kCtGain;
                    v = mixTo (v, v * ctGain[ch], stageMix[Contour] * wet[Contour]);
                    pkOut[Contour] = std::max (pkOut[Contour], std::abs (v));
                }
            }

            const float preDrive = v;
            // local feedback return into the Drive input
            const float fbIn = fbReturn > 0.0f ? fbLine[ch].read (fbDelay) * fbReturn : 0.0f;
            v += fbIn;

            // 3 Drive
            if (stageMix[Drive] > 1.0e-4f)
            {
                pkIn[Drive] = std::max (pkIn[Drive], std::abs (v));
                float d = sat[ch].process (v * drivePre);
                d = driveDc[ch].process (d);
                d = toneLp[ch].lp (d) * drivePost;
                v = mixTo (v, d, stageMix[Drive] * wet[Drive]);
                pkOut[Drive] = std::max (pkOut[Drive], std::abs (v));
            }
            else
                sat[ch].reset();

            // 4 Crush
            if (stageMix[Crush] > 1.0e-4f)
            {
                pkIn[Crush] = std::max (pkIn[Crush], std::abs (v));
                if (holdTick)
                {
                    float h = v;
                    if (dither > 0.0f && std::abs (v) > 3.0e-5f)
                        h += (float) ((ditherRng.uniform() - ditherRng.uniform()) * crushStep * dither);
                    holdVal[ch] = std::round (h / crushStep) * crushStep;
                }
                v = mixTo (v, holdVal[ch], stageMix[Crush] * wet[Crush]);
                pkOut[Crush] = std::max (pkOut[Crush], std::abs (v));
            }

            // Auto Match (bounded, slow)
            if (autoMatch && (on[Drive] || on[Crush]))
            {
                rmsIn[ch] += (preDrive * preDrive - rmsIn[ch]) * kRms;
                rmsOut[ch] += (v * v - rmsOut[ch]) * kRms;
                if (ch == 0 && rmsOut[0] > 1.0e-10f && rmsIn[0] > 1.0e-10f)
                {
                    const float target = std::clamp (std::sqrt (rmsIn[0] / rmsOut[0]), 0.25f, 4.0f);
                    autoGain += (target - autoGain) * kAuto;
                }
                v *= autoGain;
            }

            // 5 Resonator (+ its own bounded feedback)
            if (stageMix[Resonator] > 1.0e-4f)
            {
                pkIn[Resonator] = std::max (pkIn[Resonator], std::abs (v));
                const float in = v + resFbState[ch] * resFb;
                // the internal loop runs on the 0 dB-peak modes (loop gain = resFb < 1);
                // only the output is scaled by the modal gain
                const float m = std::tanh ((res1[ch].process (in) + 0.7f * res2[ch].process (in)) * 0.8f) / 0.8f;
                resFbState[ch] = m;
                const float r = std::tanh (m * resGain * 0.5f) * 2.0f;
                v = mixTo (v, r * tailRamp, stageMix[Resonator] * wet[Resonator]);
                pkOut[Resonator] = std::max (pkOut[Resonator], std::abs (v));
            }

            // feedback tap: after Resonator, before Gate
            {
                float t = fbHp[ch].hp (v);
                t = fbLp[ch].lp (t);
                t = fbDc[ch].process (t);
                t = std::tanh (t);
                fbLine[ch].push (fbReturn > 0.0f ? t : 0.0f);
            }

            // 6 Rhythm Gate
            if (stageMix[Gate] > 1.0e-4f)
            {
                pkIn[Gate] = std::max (pkIn[Gate], std::abs (v));
                v *= 1.0f + stageMix[Gate] * (gateEnv - 1.0f);
                pkOut[Gate] = std::max (pkOut[Gate], std::abs (v));
            }

            // 7 Echo
            {
                const float delayed = echo[ch].read (echoDelay);
                const float fb = echoLp[ch].lp (delayed) * echoFb * tailRamp;
                const float inToEcho = stageMix[Echo] > 1.0e-4f ? v : 0.0f;
                if (stageMix[Echo] > 1.0e-4f) pkIn[Echo] = std::max (pkIn[Echo], std::abs (v));
                echo[ch].push (std::tanh (inToEcho + fb));
                if (stageMix[Echo] > 1.0e-4f || std::abs (delayed) > 1.0e-6f)
                    v += (stageMix[Echo] * wet[Echo]) * delayed * tailRamp;
                if (stageMix[Echo] > 1.0e-4f) pkOut[Echo] = std::max (pkOut[Echo], std::abs (v));
            }

            x[ch] = undenorm (v);
        }
        // tails killed: clear the stores once per kill episode (not every sample)
        if (tailKill && tailRamp < 1.0e-3f)
        {
            if (! tailCleared)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    echo[ch].clear();
                    res1[ch].reset();
                    res2[ch].reset();
                    resFbState[ch] = 0.0f;
                }
                tailCleared = true;
            }
        }
        else if (! tailKill)
            tailCleared = false;
        (void) p;
    }

    /** Block peaks per stage (input / output of the stages that ran), for the stage meters.
        Reads and clears; called by the engine once per block. */
    void takeStagePeaks (std::atomic<float>* in, std::atomic<float>* out) noexcept
    {
        for (int st = 0; st < NumStages; ++st)
        {
            in[st].store (pkIn[st], std::memory_order_relaxed);
            out[st].store (pkOut[st], std::memory_order_relaxed);
            pkIn[st] = pkOut[st] = 0.0f;
        }
    }

    /** Crossfade that is exact at both ends (k = 1 returns the target bit-for-bit). */
    static float mixTo (float v, float target, float k) noexcept
    {
        return k >= 1.0f ? target : v + k * (target - v);
    }

    /** One-pole smoothing step that lands exactly on the target. */
    static float smoothTo (float v, float target, float c) noexcept
    {
        const float n = target + (v - target) * c;
        return std::abs (n - target) <= 1.0e-7f * std::max (1.0f, std::abs (target)) ? target : n;
    }

    float effectiveFeedback() const noexcept { return fbTarget; }
    bool feedbackLimited() const noexcept { return fbLimited; }
    bool stageOn (int s) const noexcept { return on[s]; }
    double tailSeconds() const noexcept
    {
        double t = 0.0;
        if (on[Echo] && echoFbT > 0.0f) t = std::max (t, echoDelayT / fs * std::log (0.001) / std::log ((double) echoFbT));
        else if (on[Echo]) t = std::max (t, echoDelayT / fs);
        if (on[Resonator]) t = std::max (t, (double) prm->v[P::rsDecay] * (1.0 + 4.0 * resFb));
        if (fbTarget > 0.0f) t = std::max (t, fbDelayT / fs * std::log (0.001) / std::log (std::max (0.01, (double) fbTarget)));
        return std::min (t, 30.0);
    }

private:
    double fs = 48000.0;
    const ParamSnapshot* prm = nullptr;
    StudioBlockInfo bi;
    bool on[NumStages] {};
    float wet[NumStages] {}, wetT[NumStages] {};
    float stageMix[NumStages] {};
    float pkIn[NumStages] {}, pkOut[NumStages] {};
    float kFastAtt = 0.2f, kFastRel = 0.002f, kSlowAtt = 0.005f, kSlowRel = 0.0003f, kCtGain = 0.01f,
          kRms = 0.0002f, kAuto = 0.0005f, kGlide = 0.001f;
    bool snap = true, fbCleared = false, tailCleared = false;
    float stageSmooth = 0.99f, stageStep = 0.002f;

    Biquad eq[2][5];
    float envFast[2] {}, envSlow[2] {}, ctGain[2] { 1, 1 }, ctAtt = 0, ctSus = 0;
    AdaaSaturator sat[2];
    DcBlocker driveDc[2];
    OnePole toneLp[2];
    float drivePre = 1, drivePost = 1, drivePreT = 1, drivePostT = 1;
    float crushStep = 0.01f, holdVal[2] {}, dither = 0;
    double holdPhase = 0.0, holdInc = 1.0;
    Rng ditherRng;
    Biquad res1[2], res2[2];
    float resFb = 0, resFbState[2] {}, resGain = 1;
    DelayLine echo[2];
    OnePole echoLp[2];
    double echoDelay = 1000, echoDelayT = 1000;
    float echoFb = 0, echoFbT = 0;
    DelayLine fbLine[2];
    OnePole fbHp[2], fbLp[2];
    DcBlocker fbDc[2];
    double fbDelay = 100, fbDelayT = 100;
    float fbTarget = 0, fbReturn = 0, killRamp = 1, tailRamp = 1;
    bool fbLimited = false, killFb = false, killTail = false, autoMatch = false;
    float rmsIn[2] {}, rmsOut[2] {}, autoGain = 1;
    int gateMode = 1, gateKey = 0;
    double gateDiv = 0.25, ppq = 0, ppqPerSample = 0;
    float gateEnv = 1, gateAtt = 0.9f, gateRel = 0.99f, gateFloor = 0;
    bool chokeActive = false, chokeKillsFb = false, chokeClearsEcho = false;
};

} // namespace ifx
