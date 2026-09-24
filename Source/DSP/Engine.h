#pragma once

#include "Source.h"
#include "LooseJack.h"
#include "PlugTouch.h"
#include "CableHandling.h"
#include "DirtyPot.h"
#include "StudioChain.h"
#include "Telemetry.h"
#include "../Core/Pattern.h"
#include <array>
#include <cstring>

namespace ifx
{

/** Host transport for one block (plugin layer fills it from the AudioPlayHead). */
struct TransportInfo
{
    bool hasHostTransport = false; // host supplied a position
    bool playing = false;
    double ppq = 0.0;              // quarter notes at the first sample
    double bpm = 120.0;
    int sigNum = 4, sigDen = 4;
    double hostSeconds = 0.0;
    bool offline = false;
};

/** Control events already translated from MIDI (sample-accurate). */
struct ControlEvent
{
    enum Type { NoteOn = 0, NoteOff, Controller, Pressure, GestureA, GestureB, ReleaseA, ReleaseB };
    int offset = 0;
    int type = 0;
    int number = 0;
    float value = 0.0f;
};

/** Named buses / taps (routing spec section 2). */
enum class Tap : int { FullOutput = 0, FullCore, Artifacts, FaultDifference, StudioOnly, FullWet, Dry };

/** The complete real-time engine: input conditioning, two hardware slots, counterfactual
    buses R / C0 / C, Studio branch, dry/wet, taps and protection. No allocation, locks or IO
    in process(); every buffer is sized in prepare(). */
class Engine
{
public:
    static constexpr int kMaxEvents = 512;

    Engine()
    {
        sources[1] = &jack;
        sources[2] = &touch;
        sources[3] = &cable;
        sources[4] = &pot;
    }

    //==========================================================================
    void prepare (double sampleRate, int maxBlockSize, int channels)
    {
        fs = sampleRate;
        maxBlock = std::max (1, maxBlockSize);
        numChannels = std::clamp (channels, 1, 2);
        for (int i = 1; i <= kNumSourceTypes; ++i)
            sources[i]->prepare (fs);
        for (auto& g : gen) g.prepare (fs);
        env.prepare (fs);
        studio.prepare (fs, maxBlock);
        smoothCoeff = (float) std::exp (-1.0 / (0.01 * fs));
        rampStep = (float) (1.0 / (0.01 * fs));
        slowCoeff = (float) std::exp (-1.0 / (0.05 * fs));
        for (auto& b : passive) b.reset();
        excLp.setCutoff (1500.0, fs);
        excPink1.setCutoff (80.0, fs);
        excPink2.setCutoff (900.0, fs);
        ccLp[0].setCutoff (6.0, fs);
        ccLp[1].setCutoff (6.0, fs);
        scEnvCoeff = (float) std::exp (-1.0 / (0.08 * fs));
        scFastDecay = (float) std::exp (-1.0 / (0.0208 * fs));
        scAttack = (float) (1.0 - std::exp (-1.0 / (0.0021 * fs)));
        ceilRel = (float) (1.0 - std::exp (-1.0 / (0.045 * fs)));
        telemetry.sampleRate.store (fs);
        telemetry.numChannels.store (numChannels);
        lastPassiveLoad = -1.0f;
        fullReset (0.0, 3);
        prepared = true;
    }

    /** Seed and lock are non-parameter state (message thread writes, audio reads atomically). */
    void setSeed (uint64_t s) noexcept { seedParam.store (s); }
    uint64_t getSeed() const noexcept { return seedParam.load(); }
    void setSeedLock (bool l) noexcept { seedLock.store (l); }
    /** Take number (New Take increments it; saved in the state). */
    void setTake (uint32_t t) noexcept { takeParam.store (t); }
    uint32_t getTake() const noexcept { return takeParam.load(); }
    bool getSeedLock() const noexcept { return seedLock.load(); }

    /** Slot contents; a source can only be in one slot (duplicates are rejected). */
    bool setSlotTypes (SourceType a, SourceType b) noexcept
    {
        if (a != SourceType::None && a == b)
            return false;
        pendingSlots.store (((int) a) | ((int) b << 4) | (1 << 8));
        return true;
    }
    SourceType getSlotType (int slot) const noexcept { return (SourceType) requestedSlots[slot].load(); }

    PatternExchange& patternExchange() noexcept { return exchange; }

    /** Offline / test helper: apply pending slot changes immediately (no fade). */
    void applyPendingNow() noexcept
    {
        const int pend = pendingSlots.exchange (0);
        if (pend & (1 << 8))
        {
            slotType[0] = (SourceType) (pend & 15);
            slotType[1] = (SourceType) ((pend >> 4) & 15);
            requestedSlots[0].store ((int) slotType[0]);
            requestedSlots[1].store ((int) slotType[1]);
        }
        exchange.fetch (pattern, patternVersion);
    }

    /** Explicit reset (message thread must not call during process; the processor wraps it). */
    void requestReset() noexcept { resetRequested.store (true); }

    const Source* source (SourceType t) const noexcept { return t == SourceType::None ? nullptr : sources[(int) t]; }

    //==========================================================================
    /** in/out may alias. `capture` (optional) receives the chosen capture tap. `sidechain`
        may be null (absent). */
    void process (const float* const* in, float* const* out, int numSamples, const ParamSnapshot& prm,
                  const TransportInfo& tr, const ControlEvent* events, int numEvents,
                  const float* const* sidechain, int sidechainChannels,
                  float* const* capture = nullptr, Tap captureTap = Tap::FullOutput) noexcept
    {
        if (numSamples <= 0 || ! prepared)
            return;

        const int nch = numChannels;
        const Policy policy = (Policy) prm.choice (P::policy);

        // ---- block-rate housekeeping ------------------------------------------------
        if (resetRequested.exchange (false))
            fullReset (tr.hostSeconds, 3);

        const int pend = pendingSlots.load();
        if ((pend & (1 << 8)) && slotFade <= 0.0f)
        {
            pendingSlots.store (0);
            slotType[0] = (SourceType) (pend & 15);
            slotType[1] = (SourceType) ((pend >> 4) & 15);
            requestedSlots[0].store ((int) slotType[0]);
            requestedSlots[1].store ((int) slotType[1]);
            for (int s = 0; s < 2; ++s)
                if (slotType[s] != SourceType::None)
                    sources[(int) slotType[s]]->reset();
        }
        const bool slotChangePending = (pendingSlots.load() & (1 << 8)) != 0;

        exchange.fetch (pattern, patternVersion);

        // transport / clock
        const double bpm = tr.hasHostTransport && tr.bpm > 1.0 ? tr.bpm : (double) prm[P::bpm];
        const double beatsPerBar = std::max (1.0, (double) tr.sigNum * 4.0 / std::max (1, tr.sigDen));
        const double samplesPerBeat = fs * 60.0 / std::max (20.0, bpm);
        const bool hostPlaying = tr.hasHostTransport && tr.playing;
        const bool runInternal = prm.on (P::runInternal);

        uint64_t baseSeed = seedParam.load();
        if (baseSeed != mapSeedBase)
        {
            mapSeedBase = baseSeed;
            for (int t = 1; t <= kNumSourceTypes; ++t)
                sources[t]->mapSeed = mapSeedFor (baseSeed, (SourceType) t);
        }
        if (hostPlaying && ! wasHostPlaying)
        {
            if (! seedLock.load()) ++takeCounter;
            fullReset (tr.hostSeconds, 0);
        }
        else if (hostPlaying)
        {
            const double expected = lastPpqEnd;
            const double tol = 2.0 / samplesPerBeat + 1.0e-6;
            if (tr.ppq < expected - tol)
                softReset (tr.hostSeconds, 2);       // loop jump / seek backwards
            else if (tr.ppq > expected + tol + 1.0 / samplesPerBeat * 4.0)
                fullReset (tr.hostSeconds, 1);       // seek forwards
        }
        wasHostPlaying = hostPlaying;
        // effective stream seed = (Seed, Take); the wear maps depend on Seed only, so New Take
        // keeps timing, positions and fault maps but draws different micro-events
        const uint32_t take = takeParam.load() + (seedLock.load() ? 0u : takeCounter);
        const uint64_t seedEff = take == 0 ? baseSeed : hashCombine (baseSeed, (uint64_t) take);
        if (seedEff != currentSeed)
        {
            currentSeed = seedEff;
            reseedAll (0);
        }

        bool clockRunning = hostPlaying;
        double ppqStart = tr.ppq;
        if (! hostPlaying)
        {
            if (runInternal)
            {
                if (! internalRunning) { internalRunning = true; internalPpq = 0.0; fullReset (0.0, 0); }
                ppqStart = internalPpq;
                clockRunning = true;
            }
            else
                internalRunning = false;
        }
        else
            internalRunning = false;

        if (oneShotActive)
        {
            ppqStart = oneShotPpq;
            clockRunning = true;
        }
        const double ppqEnd = ppqStart + numSamples / samplesPerBeat;

        // ---- parameters for this block ----------------------------------------------
        const float dist[2] = { prm[P::slotALevel], prm[P::slotBLevel] };
        const bool swap = prm.on (P::swapOrder);
        for (int s = 0; s < 2; ++s)
            if (slotType[s] != SourceType::None)
                sources[(int) slotType[s]]->setParams (prm, dist[s]);

        env.setMainsFrequency (prm.choice (P::mains) == 1 ? 60.0f : 50.0f);
        env.setBuzz (prm[P::buzz]);
        env.setRf (prm[P::rf]);
        env.setBuzzType ((Environment::BuzzType) std::clamp (prm.choice (P::buzzType), 0, 1));
        env.setDimmer (prm[P::dimBright], prm.choice (P::dimKind));
        env.setPhone ((Environment::Phone) std::clamp (prm.choice (P::gsm), 0, 3));

        // ground loop (Docs/research/ElectricalInterference.md 5.3 / 5.4): shield current x loop
        // resistance, in series with the signal, then the receiving interface's common-mode rejection
        static const double rShieldPerM[3] = { 0.135, 0.02, 0.0036 };   // ohm/m: drain, braid, heavy braid
        static const double glLeak[4] = { 0.0, 5.0e-6, 1.0e-4, 1.0e-3 }; // A: 2-prong leakage low / mid / high
        static const double glVdiff[4] = { 0.001, 0.01, 0.1, 1.0 };      // V: ground difference by outlets
        static const float ifaceG0[4] = { 1.0f, 0.0316f, 3.16e-4f, 1.0e-5f }; // CMRR at mains: 0 / 30 / 70 / 100 dB
        static const double ifaceFk[4] = { 1.0, 3000.0, 53.0, 67.0 };    // rejection falls 6 dB/oct above f_k
        const int glGear = std::clamp (prm.choice (P::glGear), 0, 4);
        const int glIface = std::clamp (prm.choice (P::glIface), 0, 3);
        const double glRloop = std::clamp ((double) prm[P::glRun], 0.3, 30.0) * rShieldPerM[std::clamp (prm.choice (P::glShield), 0, 2)]
                             + 2.0 * 0.05;                                // two connector contacts, 50 mohm each
        const double glVd = glVdiff[std::clamp (prm.choice (P::glOutlets), 0, 3)];
        const float glCap = limit (limits::backgroundCeiling, policy);
        const float glG0 = ifaceG0[glIface];
        const float glDiffK = glIface == 0 ? 0.0f : (float) (fs / (2.0 * kPi * ifaceFk[glIface]));
        const float gsmLevel = prm.choice (P::gsm) == 0 ? 0.0f
            : std::min (1.0f, (float) std::pow (0.15 / std::max (0.1, (double) prm[P::gsmDist]), 2.0)) * limit (limits::backgroundCeiling, policy);
        float glPeak = 0.0f;

        SampleCtx ctx;
        ctx.hum = prm[P::hum];
        ctx.buzz = prm[P::buzz];
        ctx.rf = prm[P::rf];
        ctx.stereo = prm[P::stereo];
        ctx.policy = policy;

        updatePassive (prm);
        const bool passiveOn = prm.choice (P::inputMode) == 1;
        const float trimTarget = dbToGain (prm[P::inTrim]);

        const bool rawHw = prm.on (P::rawHw);
        const int excType = prm.choice (P::excType);
        const bool excEnabled = excType != 0 && (! rawHw || prm.on (P::keepExc));
        if (snapGains) excTypeActive = excType;
        const bool excSwitching = excType != excTypeActive;
        const float excRequested = dbToGain (prm[P::excLevel]);
        const float excCap = limit (limits::excitationPeak, policy);
        const float excGainTarget = excEnabled && ! excSwitching ? std::min (excRequested, excCap) : 0.0f;
        telemetry.excitationLimited.store (excEnabled && excRequested > excCap);
        telemetry.excitationEffectiveDb.store (gainToDb (std::min (excRequested, excCap)));

        const bool preOn = prm.on (P::preOn);
        const float preDrive = dbToGain (prm[P::preDrive]);
        const float preClip = dbToGain (prm[P::preHeadroom] - 12.0f);

        const bool studioOn = prm.on (P::studioOn) && ! rawHw;
        const int studioSrc = prm.choice (P::studioSrc);
        const float studioLevel = dbToGain (prm[P::studioLevel]);

        const float mix = prm[P::mix];
        float dryG = 1.0f - mix, wetG = mix;
        if (prm.choice (P::mixLaw) == 1)
        {
            dryG = (float) std::cos (mix * kPi * 0.5);
            wetG = (float) std::sin (mix * kPi * 0.5);
        }
        dryG *= dbToGain (prm[P::dryTrim]);
        const float outTrim = dbToGain (prm[P::outTrim]);
        const Tap monitor = (Tap) prm.choice (P::monitor);
        const bool ceilOn = prm.on (P::ceilOn);
        const float ceiling = dbToGain (prm[P::ceiling]);
        const bool bypass = prm.on (P::bypass);
        const bool link = prm.on (P::linkGesture);
        const float chaos = prm[P::chaos];
        const bool slotOn[2] = { prm.on (P::slotAOn), prm.on (P::slotBOn) };
        const float humT = ctx.hum, buzzT = ctx.buzz, rfT = ctx.rf;
        // hidden Ghost Carrier: ring modulation of the wet path by a slowly wandering carrier
        const bool ghostOn = prm.on (P::ghostOn);
        const float ghostAmt = std::clamp (prm[P::ghostMix], 0.0f, 1.0f);
        const double ghostF = std::clamp ((double) prm[P::ghostFreq], 20.0, 2000.0);
        const double ghostDrift = std::clamp ((double) prm[P::ghostDrift], 0.0, 1.0);

        // swap-order declick: fade the wet core out, swap at zero, fade back in
        if (swap != effSwap && ! orderSwitching)
            orderSwitching = true;

        // lane scheduling
        SchedulerSettings ss;
        ss.seed = seedEff;
        ss.stepBeats = divisionBeats (prm.choice (P::laneDiv));
        ss.beatsPerBar = beatsPerBar;
        ss.swing = prm[P::swing];
        ss.humanize = prm[P::humanize];
        ss.density = prm[P::density];
        ss.evolve = prm[P::evolve];
        ss.patternLevel = prm[P::patLevel];
        ss.studioSend = prm[P::studioSend];
        ss.fill = prm.on (P::fill) || fillHeld;
        int numLaneEvents = 0;
        bool laneActive = (prm.on (P::laneOn) || oneShotActive) && clockRunning;
        if (laneActive)
            numLaneEvents = scheduleLane (pattern, ss, ppqStart, oneShotActive ? oneShotSamplesLeft (ppqStart, numSamples, samplesPerBeat, ss.stepBeats) : numSamples,
                                          samplesPerBeat, fs, laneEvents.data(), kMaxEvents);

        StudioBlockInfo sbi;
        sbi.bpm = bpm;
        sbi.ppqStart = ppqStart;
        sbi.clockRunning = clockRunning;
        sbi.midiGateOpen = notesHeld > 0;
        sbi.sidechainPresent = sidechain != nullptr && sidechainChannels > 0;
        sbi.sidechainEnv = scEnv;
        sbi.resonatorNote = lastNote;
        studio.setParams (prm, sbi);

        const float humPeakCap = std::min (limit (limits::backgroundCeiling, policy), limit (limits::noiseBudget, policy));
        const float reactAmt = prm[P::react];
        const float reactThr = dbToGain (prm[P::reactThresh]);
        const int presetGesture[2] = { prm.choice (P::gestureA), prm.choice (P::gestureB) };

        // momentary GUI / automation triggers
        for (int s = 0; s < 2; ++s)
        {
            const bool t = prm.on (s == 0 ? P::trigA : P::trigB);
            if (t && ! trigLatch[s])
                triggerSlot (s, (GestureKind) presetGesture[s], 0.8f, -1.0f, 0.35f, chaos, hash3 (seedEff, 0x7A16ull, ++manualCounter), policy);
            if (! t && trigLatch[s])
                gen[s].release();
            trigLatch[s] = t;
        }

        float pk[8] {};
        int laneIdx = 0, evIdx = 0;
        const int64_t blockStartSample = (int64_t) std::llround (ppqStart * samplesPerBeat);
        int64_t barOrigin = blockStartSample;
        oneShotStartSample = 0;
        const double samplesPerBar = samplesPerBeat * beatsPerBar;
        const float ghostAmtT = ghostAmt;

        // automated gains are smoothed per sample (no zipper, no block-size dependence); after a
        // prepare / reset they start at their targets so identities hold from the first sample
        if (snapGains)
        {
            snapGains = false;
            dryS = dryG; wetS = wetG; outS = outTrim; studioLvlS = studioLevel; ceilS = ceiling;
            preDriveS = preDrive; preClipS = preClip; ghostAmtS = ghostAmtT;
            humS = humT; buzzS = buzzT; rfS = rfT;
            curTap = prevTap = (int) monitor; tapFade = 1.0f;
            curSrc = prevSrc = studioSrc; srcFade = 1.0f;
            passiveMix = passiveOn ? 1.0f : 0.0f;
            preMix = preOn ? 1.0f : 0.0f;
            ceilMix = ceilOn ? 1.0f : 0.0f;
        }
        // monitor tap and Studio source changes crossfade over 10 ms (one change at a time)
        if ((int) monitor != curTap && tapFade >= 1.0f) { prevTap = curTap; curTap = (int) monitor; tapFade = 0.0f; }
        if (studioSrc != curSrc && srcFade >= 1.0f) { prevSrc = curSrc; curSrc = studioSrc; srcFade = 0.0f; }

        for (int i = 0; i < numSamples; ++i)
        {
            // ---- sample-accurate events ------------------------------------------
            while (laneIdx < numLaneEvents && laneEvents[(size_t) laneIdx].sampleOffset <= i)
                handleLaneEvent (laneEvents[(size_t) laneIdx++], presetGesture, chaos, policy, prm, beatsPerBar);
            while (evIdx < numEvents && events[evIdx].offset <= i)
                handleControlEvent (events[evIdx++], presetGesture, chaos, policy);

            // a MIDI one-shot pattern started inside this block: its step 0 lands on this sample
            if (oneShotStartPending)
            {
                oneShotStartPending = false;
                oneShotStartSample = i;
                clockRunning = true;
                laneActive = true;
                barOrigin = -(int64_t) i;
                numLaneEvents = scheduleLane (pattern, ss, 0.0, oneShotSamplesLeft (0.0, numSamples - i, samplesPerBeat, ss.stepBeats),
                                              samplesPerBeat, fs, laneEvents.data(), kMaxEvents);
                for (int k = 0; k < numLaneEvents; ++k)
                    laneEvents[(size_t) k].sampleOffset += i;
                laneIdx = 0;
                while (laneIdx < numLaneEvents && laneEvents[(size_t) laneIdx].sampleOffset <= i)
                    handleLaneEvent (laneEvents[(size_t) laneIdx++], presetGesture, chaos, policy, prm, beatsPerBar);
            }

            // bar re-keying of the random sub-streams while a clock runs
            if (clockRunning)
            {
                const int64_t bar = (int64_t) std::floor ((double) (barOrigin + i) / samplesPerBar + 1.0e-9);
                if (bar != lastBar)
                {
                    lastBar = bar;
                    reseedAll ((uint64_t) (bar + 1));
                    telemetry.push (LogBar, (int32_t) bar, 0.0f);
                }
            }

            if (chokeSamples > 0 && --chokeSamples == 0)
                studio.setChoke (false, false, false);
            if (sendSamples > 0 && --sendSamples == 0)
                sendTarget = 1.0f;
            sendGain = sendTarget + (sendGain - sendTarget) * smoothCoeff;

            // ---- per-sample smoothing of block targets --------------------------------
            dryS = smoothTo (dryS, dryG, smoothCoeff);
            wetS = smoothTo (wetS, wetG, smoothCoeff);
            outS = smoothTo (outS, outTrim, smoothCoeff);
            studioLvlS = smoothTo (studioLvlS, studioLevel, smoothCoeff);
            ceilS = smoothTo (ceilS, ceiling, smoothCoeff);
            preDriveS = smoothTo (preDriveS, preDrive, smoothCoeff);
            preClipS = smoothTo (preClipS, preClip, smoothCoeff);
            ghostAmtS = smoothTo (ghostAmtS, ghostAmtT, smoothCoeff);
            humS = smoothTo (humS, humT, smoothCoeff);
            buzzS = smoothTo (buzzS, buzzT, smoothCoeff);
            rfS = smoothTo (rfS, rfT, smoothCoeff);
            ctx.hum = humS; ctx.buzz = buzzS; ctx.rf = rfS;
            tapFade = std::min (1.0f, tapFade + rampStep);
            srcFade = std::min (1.0f, srcFade + rampStep);
            passiveMix = rampTo (passiveMix, passiveOn ? 1.0f : 0.0f);
            preMix = rampTo (preMix, preOn ? 1.0f : 0.0f);
            ceilMix = rampTo (ceilMix, ceilOn ? 1.0f : 0.0f);

            // ---- gestures --------------------------------------------------------
            GestureOut g[2] = { gen[0].step(), gen[1].step() };
            // continuous MIDI motion (CC1) and pressure (aftertouch)
            const float ccMotion = ccLp[0].lp (ccMod > 0.0f ? (float) ccRng.bipolar() : 0.0f) * 6.0f * ccMod;
            const float press = ccLp[1].lp (ccPressure);
            for (auto& gg : g) { gg.displacement += ccMotion; gg.pressure += press; }
            if (link)
            {
                GestureOut m;
                m.displacement = std::clamp (g[0].displacement + g[1].displacement, -1.0f, 1.0f);
                m.pressure = std::clamp (g[0].pressure + g[1].pressure, -1.0f, 1.0f);
                const int k = g[0].steer >= g[1].steer ? 0 : 1;
                m.steer = g[k].steer;
                m.steerPos = g[k].steerPos;
                m.tapImpulse = std::max (g[0].tapImpulse, g[1].tapImpulse);
                m.envelope = std::max (g[0].envelope, g[1].envelope);
                m.active = g[0].active || g[1].active;
                g[0] = g[1] = m;
            }

            ctx.env = env.step();

            // ---- input conditioning + excitation ---------------------------------
            trim = trimTarget + (trim - trimTarget) * smoothCoeff;
            excGain = excGainTarget + (excGain - excGainTarget) * smoothCoeff;
            if (excType != excTypeActive && excGain < 1.0e-5f)
            {
                excTypeActive = excType; // switch waveforms only while silent
                excPhase = 0.0;
                excGain = 0.0f;
            }
            const float e = excGain > 1.0e-6f ? excitation (excTypeActive) * excGain : 0.0f;
            float x[2] = { 0.0f, 0.0f }, p[2] = { 0.0f, 0.0f };
            for (int ch = 0; ch < nch; ++ch)
            {
                x[ch] = in != nullptr && in[ch] != nullptr ? in[ch][i] : 0.0f;
                if (! std::isfinite (x[ch])) x[ch] = 0.0f;
                float v = x[ch] * trim;
                if (passiveMix > 0.0f)
                {
                    const float pv = (float) (passive[ch].process (v) * passiveNorm);
                    v = passiveMix >= 1.0f ? pv : v + passiveMix * (pv - v);
                }
                else
                    passive[ch].reset();
                p[ch] = v + e;
            }
            const float lvl = std::max (std::abs (p[0]), std::abs (p[nch - 1]));
            inputLevel = lvl > inputLevel ? lvl : inputLevel * slowCoeff;
            ctx.inputLevel = inputLevel;

            // ---- hardware slots: control always advances (silent clock policy) ----
            Source* src[2] = { nullptr, nullptr };
            for (int s = 0; s < 2; ++s)
            {
                if (slotType[s] == SourceType::None) continue;
                src[s] = sources[(int) slotType[s]];
                src[s]->control (g[s], ctx);
                const float target = slotOn[s] ? 1.0f : 0.0f;
                slotMix[s] = rampTo (slotMix[s], target);
            }

            // shared noise budget (probabilistic union instead of summing two hum pickups)
            float envScale = 1.0f;
            {
                const float c0 = src[0] && slotOn[0] ? src[0]->pickupCoupling() : 0.0f;
                const float c1 = src[1] && slotOn[1] ? src[1]->pickupCoupling() : 0.0f;
                const float sum = c0 + c1;
                if (sum > 1.0e-6f)
                {
                    const float uni = 1.0f - (1.0f - std::min (1.0f, c0)) * (1.0f - std::min (1.0f, c1));
                    envScale = uni / sum;
                    const float humMax = sum * limit (limits::humPeak, policy) * envScale * std::max (ctx.hum, std::max (ctx.buzz, ctx.rf));
                    if (humMax > humPeakCap) envScale *= humPeakCap / humMax;
                }
            }

            float paths[3][2];
            for (int ch = 0; ch < 2; ++ch)
                paths[0][ch] = paths[1][ch] = paths[2][ch] = p[ch];

            // ---- ground loop + 2G phone: injected into the complete path only (part of bus A) ----
            float envInj = 0.0f;
            if (glGear > 0)
            {
                // a worn / partly open sleeve (Loose Jack) or shield (Cable Handling) adds resistance, so
                // the hum breathes with movement; a fully open ground breaks the loop
                double open = 0.0;
                for (int s = 0; s < 2; ++s)
                    if (src[s] != nullptr && slotOn[s] && (slotType[s] == SourceType::LooseJack || slotType[s] == SourceType::CableHandling))
                        open = std::max (open, (double) std::clamp (src[s]->tele.auxOpenness, 0.0f, 1.0f));
                const double r = glRloop + 1.0 * open;
                const double v = (glGear < 4 ? glLeak[glGear] * r : glVd * r / (r + 0.5)) * (1.0 - open);
                const float glLvl = std::min ((float) (v / 0.3 * 0.126), glCap); // 300 mV nominal = -18 dBFS
                const float cm = glLvl * (glGear < 4 ? ctx.env.humBright : ctx.env.humClean); // leakage buzz vs IR hum
                const float res = glG0 * (cm + (cm - glPrev) * glDiffK);
                glPrev = cm;
                glPeak = std::max (glPeak, std::abs (res));
                envInj += res;
            }
            else
                glPrev = 0.0f;
            if (gsmLevel > 0.0f)
                envInj += gsmLevel * ctx.env.gsm;
            if (envInj != 0.0f)
                for (int ch = 0; ch < nch; ++ch)
                    paths[2][ch] += envInj;

            const int order[2] = { effSwap ? 1 : 0, effSwap ? 0 : 1 };
            for (int oi = 0; oi < 2; ++oi)
            {
                const int s = order[oi];
                if (src[s] == nullptr || slotMix[s] <= 0.0f)
                    continue;
                if (slotMix[s] >= 1.0f)
                    src[s]->process (paths, nch, ctx, envScale);
                else
                {
                    float dry[3][2];
                    std::memcpy (dry, paths, sizeof (dry));
                    src[s]->process (paths, nch, ctx, envScale);
                    const float k = slotMix[s];
                    for (int pi = 0; pi < 3; ++pi)
                        for (int ch = 0; ch < nch; ++ch)
                            paths[pi][ch] = dry[pi][ch] + k * (paths[pi][ch] - dry[pi][ch]);
                }
            }

            // ---- optional input-stage saturation (all three paths, ADAA) ---------
            if (preMix > 0.0f)
            {
                for (int pi = 0; pi < 3; ++pi)
                    for (int ch = 0; ch < nch; ++ch)
                    {
                        const float sat = preClipS * preSat[pi][ch].process (paths[pi][ch] * preDriveS / preClipS) / preDriveS;
                        paths[pi][ch] = preMix >= 1.0f ? sat : paths[pi][ch] + preMix * (sat - paths[pi][ch]);
                    }
            }
            else
                for (auto& row : preSat) for (auto& sat : row) sat.reset();

            // ---- canonical buses --------------------------------------------------
            float R[2], C0[2], C[2], D[2], A[2], F[2], U[2], P[2], W[2], y[2];
            for (int ch = 0; ch < nch; ++ch)
            {
                R[ch] = paths[0][ch];
                C0[ch] = paths[1][ch];
                C[ch] = paths[2][ch];
                D[ch] = C0[ch] - R[ch];
                A[ch] = C[ch] - C0[ch];
                selectStudioSource (curSrc, R[ch], C[ch], D[ch], A[ch], F[ch], U[ch]);
                if (srcFade < 1.0f)
                {
                    float f0, u0;
                    selectStudioSource (prevSrc, R[ch], C[ch], D[ch], A[ch], f0, u0);
                    F[ch] = f0 + srcFade * (F[ch] - f0);
                    U[ch] = u0 + srcFade * (U[ch] - u0);
                }
            }

            // Studio branch
            const float sTarget = studioOn ? 1.0f : 0.0f;
            studioMix = rampTo (studioMix, sTarget);
            if (studioMix > 0.0f)
            {
                float s[2] = { F[0] * sendGain, F[nch - 1] * sendGain };
                studio.process (s, nch);
                for (int ch = 0; ch < nch; ++ch)
                    P[ch] = F[ch] + studioMix * (s[ch] * studioLvlS - F[ch]);
            }
            else
            {
                for (int ch = 0; ch < nch; ++ch) P[ch] = F[ch];
            }

            ghostRamp = rampTo (ghostRamp, ghostOn ? 1.0f : 0.0f);
            float ghostGain = 0.0f, carrier = 0.0f;
            if (ghostRamp > 0.0f)
            {
                const double wander = std::sin (2.0 * kPi * ghostLfo);
                ghostPhase += ghostF * (1.0 + 0.06 * ghostDrift * wander) / fs;
                ghostPhase -= std::floor (ghostPhase);
                ghostLfo += 0.23 / fs;
                ghostLfo -= std::floor (ghostLfo);
                carrier = (float) std::sin (2.0 * kPi * ghostPhase);
                ghostGain = ghostRamp * ghostAmtS;
            }

            // Raw Hardware and slot-change / order declick
            const float rTarget = rawHw ? 1.0f : 0.0f;
            rawMix = rampTo (rawMix, rTarget);
            updateFades (slotChangePending, swap);

            for (int ch = 0; ch < nch; ++ch)
            {
                float w;
                if (studioMix <= 0.0f)
                    w = C[ch];                               // Studio bypassed: exactly C
                else
                    w = R[ch] + U[ch] + P[ch];
                if (ghostGain > 0.0f)
                    w += ghostGain * (w * carrier - w);   // hidden effect: bypassed by Raw Hardware below
                w = w + rawMix * (C[ch] - w);
                w *= orderFade * slotFade;
                W[ch] = w;
                y[ch] = dryS * x[ch] + wetS * W[ch];
            }

            // ---- taps, trim, protection ------------------------------------------
            float pre[2] = { 0.0f, 0.0f }, peakAll = 0.0f;
            for (int ch = 0; ch < nch; ++ch)
            {
                const float tapVals[7] = { y[ch], C[ch], A[ch], D[ch], P[ch], W[ch], x[ch] };
                float v = tapVals[curTap];
                if (tapFade < 1.0f)
                    v = tapVals[prevTap] + tapFade * (v - tapVals[prevTap]);
                pre[ch] = v * outS;
                peakAll = std::max (peakAll, std::abs (pre[ch]));
            }
            // Ceiling: stereo-linked gain rider, instant attack, ~45 ms release, then a clamp
            if (ceilMix > 0.0f)
            {
                const float need = peakAll > ceilS ? ceilS / peakAll : 1.0f;
                ceilGain = need < ceilGain ? need : ceilGain + (1.0f - ceilGain) * ceilRel;
            }
            else
                ceilGain = 1.0f;
            for (int ch = 0; ch < nch; ++ch)
            {
                float o = pre[ch];
                if (ceilMix > 0.0f)
                {
                    const float lim = std::clamp (o * ceilGain, -ceilS, ceilS);
                    o = ceilMix >= 1.0f ? lim : o + ceilMix * (lim - o);
                }
                const float bTarget = bypass ? 1.0f : 0.0f;
                if (ch == 0)
                {
                    bypassMix = rampTo (bypassMix, bTarget);
                }
                if (bypassMix >= 1.0f)
                    o = x[ch];
                else if (bypassMix > 0.0f)
                    o = o + bypassMix * (x[ch] - o);
                // numeric failsafe only (invalid values), never routine colouring
                if (! std::isfinite (o)) o = 0.0f;
                o = std::clamp (o, -kSafetyClamp, kSafetyClamp);
                out[ch][i] = o;

                if (capture != nullptr && capture[ch] != nullptr)
                {
                    const float capVals[7] = { o, C[ch], A[ch], D[ch], P[ch], W[ch], x[ch] };
                    capture[ch][i] = captureTap == Tap::FullOutput ? o : capVals[(int) captureTap];
                }

                pk[0] = std::max (pk[0], std::abs (x[ch]));
                pk[1] = std::max (pk[1], std::abs (R[ch]));
                pk[2] = std::max (pk[2], std::abs (D[ch]));
                pk[3] = std::max (pk[3], std::abs (A[ch]));
                pk[4] = std::max (pk[4], std::abs (C[ch]));
                pk[5] = std::max (pk[5], std::abs (P[ch]));
                pk[6] = std::max (pk[6], std::abs (W[ch]));
                pk[7] = std::max (pk[7], std::abs (o));
            }

            // ---- sidechain detector (control only) --------------------------------
            if (sidechain != nullptr && sidechainChannels > 0)
            {
                float a = 0.0f;
                for (int ch = 0; ch < sidechainChannels; ++ch)
                    if (sidechain[ch] != nullptr) a = std::max (a, std::abs (sidechain[ch][i]));
                if (! std::isfinite (a)) a = 0.0f;
                scFast = a > scFast ? a : scFast * scFastDecay;
                scEnv = a > scEnv ? scEnv + (a - scEnv) * scAttack : scEnv * scEnvCoeff;
                if (reactRefractory > 0) --reactRefractory;
                if (reactAmt > 0.0f && reactRefractory == 0 && scFast > reactThr && scFast > scEnv * 1.8f)
                {
                    const float lvl2 = std::clamp (scFast * 2.0f, 0.2f, 1.0f) * reactAmt;
                    for (int s = 0; s < 2; ++s)
                        triggerSlot (s, (GestureKind) presetGesture[s], lvl2, -1.0f, 0.25f, chaos, hash3 (seedEff, 0x5EACull, ++reactCounter), policy);
                    telemetry.push (LogReact, 0, lvl2);
                    reactRefractory = (int) (0.08 * fs);
                }
            }
            else
                scEnv = scFast = 0.0f;
        }

        // ---- clocks --------------------------------------------------------------
        lastPpqEnd = ppqEnd;
        if (internalRunning) internalPpq += numSamples / samplesPerBeat;
        if (oneShotActive)
        {
            oneShotPpq += (numSamples - oneShotStartSample) / samplesPerBeat;
            int longest = 1;
            for (const auto& l : pattern.lanes) longest = std::max (longest, l.length);
            if (oneShotPpq >= longest * ss.stepBeats)
                oneShotActive = false;
        }
        if (! clockRunning) lastBar = -1;

        // ---- telemetry ------------------------------------------------------------
        auto& t = telemetry;
        t.inPeak.store (pk[0]); t.rPeak.store (pk[1]); t.dPeak.store (pk[2]); t.aPeak.store (pk[3]);
        t.cPeak.store (pk[4]); t.pPeak.store (pk[5]); t.wPeak.store (pk[6]); t.outPeak.store (pk[7]);
        t.outPeakDb.store (gainToDb (pk[7]));
        t.fbEffective.store (studio.effectiveFeedback());
        t.fbLimited.store (studio.feedbackLimited());
        t.ppq.store (ppqEnd);
        t.bpm.store ((float) bpm);
        t.clockRunning.store (clockRunning);
        t.internalClock.store (! hostPlaying && clockRunning);
        t.hostTransport.store (tr.hasHostTransport);
        t.sidechainPresent.store (sidechain != nullptr && sidechainChannels > 0);
        t.sidechainEnv.store (scEnv);
        t.excitationActive.store (excGain > 1.0e-6f);
        t.groundLoopDb.store (glGear > 0 ? gainToDb (glPeak) : -120.0f);
        t.tailSeconds.store (studioOn ? (float) studio.tailSeconds() : 0.0f); // Studio off / Raw Hardware: no tail
        if (studioMix > 0.0f)
            studio.takeStagePeaks (t.stageIn, t.stageOut);
        else
            for (int st = 0; st < 7; ++st) { t.stageIn[st].store (0.0f); t.stageOut[st].store (0.0f); }
        t.effectiveSeed.store (seedEff);
        for (int li = 0; li < kNumLanes; ++li)
        {
            const int n = std::clamp (pattern.lanes[li].length, 1, kMaxSteps);
            t.laneStep[li].store (clockRunning && laneActive ? (int) ((((int64_t) std::floor (ppqEnd / ss.stepBeats)) % n + n) % n) : -1);
        }
        for (int s = 0; s < 2; ++s)
        {
            auto& st = t.slot[s];
            st.type.store ((int) slotType[s]);
            st.enabled.store (slotOn[s]);
            if (slotType[s] == SourceType::None) continue;
            const auto& te = sources[(int) slotType[s]]->tele;
            st.position.store (te.position); st.secondary.store (te.secondary); st.velocity.store (te.velocity);
            st.openness.store (te.openness); st.auxOpenness.store (te.auxOpenness); st.activity.store (te.activity);
            st.wearHere.store (te.wearHere); st.state.store (te.state); st.shorted.store (te.shorted);
            st.openEvents.store (te.openEvents); st.resistiveEvents.store (te.resistiveEvents); st.mechEvents.store (te.mechEvents);
            st.quiet.store (te.quiet | (slotOn[s] ? 0u : (uint32_t) QuietBypassed));
            st.gesture.store (gen[s].last().envelope);
            st.motionLimited.store (gen[s].motionLimited);
            st.gestures.store (gen[s].started);
            st.electricalPeak.store (te.activity);
            st.mechanicalPeak.store (te.mechActivity);
        }
    }

    double getTailSeconds() const noexcept { return telemetry.tailSeconds.load(); }

    EngineTelemetry telemetry;

    // test / render introspection
    uint64_t effectiveSeed() const noexcept { return currentSeed; }
    const GestureGenerator& gesture (int slot) const noexcept { return gen[slot]; }

private:
    //==========================================================================
    /** A one-shot plays exactly one cycle of the longest lane: never schedule past its end. */
    int oneShotSamplesLeft (double ppqStart, int numSamples, double samplesPerBeat, double stepBeats) const noexcept
    {
        int longest = 1;
        for (const auto& l : pattern.lanes) longest = std::max (longest, l.length);
        const double left = (longest * stepBeats - ppqStart) * samplesPerBeat;
        return std::clamp ((int) std::ceil (left - 1.0e-6), 0, numSamples);
    }

    void triggerSlot (int s, GestureKind k, float intensity, float target, float durSec, float chaos, uint64_t var, Policy policy) noexcept
    {
        if (slotType[s] == SourceType::None) return;
        const float base = sources[(int) slotType[s]]->basePosition();
        const bool ok = gen[s].trigger (k, intensity, durSec, target, base, chaos, var, policy);
        telemetry.push (ok ? LogGesture : LogCoalesced, s * 100 + (int) k, intensity);
    }

    void handleLaneEvent (const LaneEvent& e, const int* presetGesture, float chaos, Policy policy, const ParamSnapshot& prm, double beatsPerBar) noexcept
    {
        const int s = e.lane;
        if (slotType[s] == SourceType::None) return;
        GestureKind k = e.step.gesture < 3 ? patternGesture (slotType[s], e.step.gesture) : (GestureKind) presetGesture[s];
        // Evolve: seeded per-bar change of gesture choice (positions and fault map stay)
        const float evolve = prm[P::evolve];
        if (evolve > 0.0f && e.step.gesture < 3)
        {
            const double bar = std::floor ((double) e.globalStep * divisionBeats (prm.choice (P::laneDiv)) / beatsPerBar);
            const uint64_t h = hash4 (currentSeed, (uint64_t) bar, (uint64_t) s, (uint64_t) e.stepIndex);
            if (hashToUnit (h) < evolve * 0.35)
                k = patternGesture (slotType[s], (int) (hashToUnit (hashCombine (h, 1)) * 3.0));
        }
        const uint64_t var = hash4 (currentSeed, (uint64_t) s, (uint64_t) e.globalStep, (uint64_t) e.ratchetIndex);
        triggerSlot (s, k, e.intensity, e.step.target, e.durationSec, chaos, var, policy);
        telemetry.push (LogLaneStep, s * 100 + e.stepIndex, e.intensity);
        sendTarget = e.send;
        sendSamples = std::max (1, (int) (e.durationSec * fs));
        if (e.step.choke)
        {
            studio.setChoke (true, prm.on (P::gtChokeFb), prm.on (P::gtChokeEcho));
            chokeSamples = std::max (1, (int) (e.durationSec * fs));
            telemetry.push (LogChoke, s, 0.0f);
        }
    }

    void handleControlEvent (const ControlEvent& ev, const int* presetGesture, float chaos, Policy policy) noexcept
    {
        const uint64_t var = hash3 (currentSeed, 0xC0DEull, ++manualCounter);
        switch (ev.type)
        {
            case ControlEvent::NoteOn:
            {
                const int n = std::clamp (ev.number, 0, 127);
                const float vel = std::clamp (ev.value, 0.0f, 1.0f);
                telemetry.push (LogNoteOn, n, vel);
                ++notesHeld;
                uint8_t mask = 0;
                auto dur = [] (GestureKind k) { return k == GestureKind::Tap ? 0.12f : (k == GestureKind::SlowTurn || k == GestureKind::Sweep ? 0.8f : -1.0f); };
                if (n >= 36 && n <= 39)
                {
                    const GestureKind k = n == 39 ? (GestureKind) presetGesture[0] : patternGesture (slotType[0], n - 36);
                    triggerSlot (0, k, vel, -1.0f, dur (k), chaos, var, policy); mask = 1;
                }
                else if (n >= 48 && n <= 51)
                {
                    const GestureKind k = n == 51 ? (GestureKind) presetGesture[1] : patternGesture (slotType[1], n - 48);
                    triggerSlot (1, k, vel, -1.0f, dur (k), chaos, var, policy); mask = 2;
                }
                else if (n == 62)
                {
                    oneShotActive = true;
                    oneShotStartPending = true;
                    oneShotPpq = 0.0;
                    lastBar = -1;
                }
                else if (n == 63)
                    fillHeld = true;
                else
                {
                    for (int s = 0; s < 2; ++s)
                    {
                        const GestureKind k = (GestureKind) presetGesture[s];
                        triggerSlot (s, k, vel, -1.0f, dur (k), chaos, hashCombine (var, (uint64_t) s), policy);
                    }
                    mask = 3;
                    lastNote = n;
                }
                noteSlots[n] = mask;
                break;
            }
            case ControlEvent::NoteOff:
            {
                const int n = std::clamp (ev.number, 0, 127);
                telemetry.push (LogNoteOff, n, 0.0f);
                notesHeld = std::max (0, notesHeld - 1);
                if (noteSlots[n] & 1) gen[0].release();
                if (noteSlots[n] & 2) gen[1].release();
                noteSlots[n] = 0;
                if (n == 62) { oneShotActive = false; oneShotStartPending = false; }   // documented note-off policy: stop at once
                if (n == 63) fillHeld = false;
                break;
            }
            case ControlEvent::Controller:
                telemetry.push (LogCC, ev.number, ev.value);
                if (ev.number == 1) ccMod = std::clamp (ev.value, 0.0f, 1.0f);
                if (ev.number == 123 || ev.number == 120) { gen[0].release(); gen[1].release(); notesHeld = 0; }
                break;
            case ControlEvent::Pressure:
                ccPressure = std::clamp (ev.value, 0.0f, 1.0f);
                break;
            case ControlEvent::GestureA:
            case ControlEvent::GestureB:
            {
                const int s = ev.type == ControlEvent::GestureA ? 0 : 1;
                triggerSlot (s, (GestureKind) std::clamp (ev.number, 0, (int) GestureKind::Count - 1), ev.value, -1.0f, 0.35f, chaos, var, policy);
                break;
            }
            case ControlEvent::ReleaseA: gen[0].release(); break;
            case ControlEvent::ReleaseB: gen[1].release(); break;
            default: break;
        }
    }

    float excitation (int type) noexcept
    {
        switch (type)
        {
            case 1: // bounded broadband (pinkish) noise
            {
                const float w = (float) excRng.bipolar();
                const float a = excPink1.lp (w), b = excPink2.lp (w);
                return std::clamp (0.9f * a * 6.0f + 0.5f * b + 0.25f * w, -1.0f, 1.0f) * 0.8f;
            }
            case 2: // filtered low-level buzz: band-limited saw at 110 Hz
            {
                const double inc = 110.0 / fs;
                excPhase += inc;
                if (excPhase >= 1.0) excPhase -= 1.0;
                double s = 2.0 * excPhase - 1.0;
                // polyBLEP
                if (excPhase < inc) { const double t = excPhase / inc; s -= t + t - t * t - 1.0; }
                else if (excPhase > 1.0 - inc) { const double t = (excPhase - 1.0) / inc; s -= t * t + t + t + 1.0; }
                return excLp.lp ((float) s) * 1.4f;
            }
            case 3: // bias impulses: sparse DC-free charge/discharge doublets
            {
                if (--excCounter <= 0)
                {
                    excCounter = (int) (fs * excRng.range (0.12, 0.4));
                    excImpulse = (float) (excRng.uniform() < 0.5 ? -1.0 : 1.0);
                    excImpulseAge = 0;
                }
                const double t = (double) excImpulseAge++ / fs;
                if (t > 0.05) return 0.0f;
                const double v = std::exp (-t / 0.002) - std::exp (-t / 0.008) * 0.25 * 1.0;
                return (float) (excImpulse * v);
            }
            case 4: // calibration test tone, 1 kHz sine
            {
                excPhase += 1000.0 / fs;
                if (excPhase >= 1.0) excPhase -= 1.0;
                return (float) std::sin (2.0 * kPi * excPhase);
            }
            default: return 0.0f;
        }
    }

    void updatePassive (const ParamSnapshot& prm) noexcept
    {
        const float load = prm[P::cableLoad];
        if (std::abs (load - lastPassiveLoad) < 0.5f) return;
        lastPassiveLoad = load;
        // simplified pickup + cable: series Rp + L into shunt C || Rl (volume/tone pots || amp)
        const double L = 2.5, Rp = 6500.0, Rl = 200000.0;
        const double C = (double) load * 1.0e-12 + 100.0e-12; // + pickup self capacitance
        const double d0 = 1.0 + Rp / Rl, d1 = L / Rl + Rp * C, d2 = L * C;
        const double f0 = 1.0 / (2.0 * kPi * std::sqrt (L * C));
        for (auto& b : passive)
            b.bilinear (1.0, 0.0, 0.0, d0, d1, d2, f0, fs);
        passiveNorm = d0; // unity DC gain: no level jump when switching circuit mode
    }

    /** One-pole smoothing step that lands exactly on the target (so identities hold once settled). */
    static float smoothTo (float v, float target, float c) noexcept
    {
        const float n = target + (v - target) * c;
        return std::abs (n - target) <= 1.0e-7f * std::max (1.0f, std::abs (target)) ? target : n;
    }

    static void selectStudioSource (int src, float r, float c, float d, float a, float& f, float& u) noexcept
    {
        switch (src)
        {
            case 0:  f = a; u = d; break;       // Artifacts
            case 1:  f = d; u = a; break;       // Breaks
            default: f = c - r; u = 0.0f; break; // Both
        }
    }

    /** Fixed-length (10 ms) linear declick ramp: reaches its target exactly, so bypass
        identities are exact once a switch has settled. */
    float rampTo (float v, float target) const noexcept
    {
        return v < target ? std::min (target, v + rampStep) : std::max (target, v - rampStep);
    }

    void updateFades (bool slotChangePending, bool swap) noexcept
    {
        const float step = (float) (1.0 / (0.005 * fs));
        if (orderSwitching)
        {
            orderFade -= step;
            if (orderFade <= 0.0f)
            {
                orderFade = 0.0f;
                orderSwitching = false;
                orderReturning = true;
                effSwap = swap;
            }
        }
        else if (orderReturning || orderFade < 1.0f)
        {
            orderFade = std::min (1.0f, orderFade + step);
            if (orderFade >= 1.0f) orderReturning = false;
        }
        if (slotChangePending)
            slotFade = std::max (0.0f, slotFade - step);
        else
            slotFade = std::min (1.0f, slotFade + step);
    }

    void reseedAll (uint64_t barKey) noexcept
    {
        for (int t = 1; t <= kNumSourceTypes; ++t)
            sources[t]->seed (hash3 (currentSeed, (uint64_t) t, barKey));
        env.seed (hash3 (currentSeed, 0xE7ull, barKey));
        excRng.seed (hash3 (currentSeed, 0xE8ull, barKey));
        ccRng.seed (hash3 (currentSeed, 0xE9ull, barKey));
    }

    /** Transport start / forward seek / prepare / explicit reset: everything restarts. */
    void fullReset (double hostSeconds, int reason) noexcept
    {
        softReset (hostSeconds, reason);
        studio.reset();
        snapGains = true;
        for (auto& b : passive) b.reset();
        for (auto& row : preSat) for (auto& s : row) s.reset();
        excPhase = 0.0;
        excCounter = 0;
        excPink1.reset(); excPink2.reset(); excLp.reset();
    }

    /** Loop jump: restart the fast physical state and the random streams, keep Studio tails. */
    void softReset (double hostSeconds, int reason) noexcept
    {
        for (int t = 1; t <= kNumSourceTypes; ++t)
            sources[t]->reset();
        for (auto& g : gen) g.reset();
        env.reset (hostSeconds);
        lastBar = -1;
        chokeSamples = 0;
        ghostPhase = ghostLfo = 0.0;
        studio.setChoke (false, false, false);
        sendTarget = sendGain = 1.0f;
        sendSamples = 0;
        reseedAll (0);
        telemetry.resets.fetch_add (1);
        telemetry.push (LogReset, reason, 0.0f);
    }

    //==========================================================================
    double fs = 48000.0;
    int maxBlock = 512, numChannels = 2;
    bool prepared = false;

    LooseJack jack;
    PlugTouch touch;
    CableHandling cable;
    DirtyPot pot;
    Source* sources[kNumSourceTypes + 1] { nullptr, nullptr, nullptr, nullptr, nullptr };
    SourceType slotType[2] { SourceType::LooseJack, SourceType::None };
    std::atomic<int> requestedSlots[2] { { (int) SourceType::LooseJack }, { (int) SourceType::None } };
    std::atomic<int> pendingSlots { 0 };
    std::atomic<bool> resetRequested { false };

    GestureGenerator gen[2];
    Environment env;
    StudioChain studio;
    Biquad passive[2];
    double passiveNorm = 1.0;
    float lastPassiveLoad = -1.0f;
    AdaaSaturator preSat[3][2];

    std::atomic<uint64_t> seedParam { 0x1F2E3D4Cull };
    std::atomic<bool> seedLock { true };
    uint64_t currentSeed = 0, mapSeedBase = ~0ull;
    uint32_t takeCounter = 0;
    std::atomic<uint32_t> takeParam { 0 };

    PatternExchange exchange;
    Pattern pattern;
    uint32_t patternVersion = 0;
    std::array<LaneEvent, kMaxEvents> laneEvents {};

    // clocks
    bool wasHostPlaying = false, internalRunning = false, oneShotActive = false, fillHeld = false, oneShotStartPending = false;
    int oneShotStartSample = 0;
    double lastPpqEnd = 0.0, internalPpq = 0.0, oneShotPpq = 0.0;
    int64_t lastBar = -1;

    // smoothing / state
    float smoothCoeff = 0.99f, slowCoeff = 0.999f, rampStep = 0.002f;
    float trim = 1.0f, excGain = 0.0f, studioMix = 0.0f, rawMix = 0.0f, bypassMix = 0.0f, ceilGain = 1.0f;
    float slotMix[2] { 1.0f, 1.0f };
    float ghostRamp = 0.0f;
    float glPrev = 0.0f;
    // smoothed block targets and switch crossfades
    bool snapGains = true;
    float dryS = 0.0f, wetS = 1.0f, outS = 1.0f, studioLvlS = 1.0f, ceilS = 1.0f, preDriveS = 1.0f, preClipS = 1.0f,
          ghostAmtS = 0.0f, humS = 0.0f, buzzS = 0.0f, rfS = 0.0f;
    int curTap = 0, prevTap = 0, curSrc = 0, prevSrc = 0, excTypeActive = 0;
    float tapFade = 1.0f, srcFade = 1.0f, passiveMix = 0.0f, preMix = 0.0f, ceilMix = 0.0f;
    float ceilRel = 0.0005f, scFastDecay = 0.999f, scAttack = 0.01f;
    double ghostPhase = 0.0, ghostLfo = 0.0;
    float orderFade = 1.0f, slotFade = 1.0f;
    bool orderSwitching = false, orderReturning = false, effSwap = false;
    float inputLevel = 0.0f;
    float sendGain = 1.0f, sendTarget = 1.0f;
    int sendSamples = 0, chokeSamples = 0;
    bool trigLatch[2] { false, false };
    uint64_t manualCounter = 0, reactCounter = 0;

    // excitation
    Rng excRng;
    OnePole excLp, excPink1, excPink2;
    double excPhase = 0.0;
    int excCounter = 0, excImpulseAge = 1 << 20;
    float excImpulse = 0.0f;

    // MIDI
    int notesHeld = 0, lastNote = -1;
    uint8_t noteSlots[128] {};
    float ccMod = 0.0f, ccPressure = 0.0f;
    Rng ccRng;
    OnePole ccLp[2];

    // sidechain
    float scEnv = 0.0f, scFast = 0.0f, scEnvCoeff = 0.999f;
    int reactRefractory = 0;
};

} // namespace ifx
