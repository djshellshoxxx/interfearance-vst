#pragma once

#include "Source.h"

namespace ifx
{

/** Cable Handling: two separate sublayers.
    Electrical - conductor and shield are independent contacts whose susceptibility is the
    seeded weak-spot map at the current bend position; conductor faults interrupt the signal,
    shield faults admit hum while the conductor stays live.
    Handling  - only through an explicitly enabled microphonic path (Mic): stick-slip friction
    impulses whose rate and size follow motion speed, and discrete taps, both exciting a small
    bank of damped cable/pickup resonances. No uniform white noise; silence when motion stops
    apart from a short resonant tail. */
class CableHandling final : public Source
{
public:
    SourceType type() const noexcept override { return SourceType::CableHandling; }

    void prepare (double sampleRate) noexcept override
    {
        Source::prepare (sampleRate);
        flex.prepare (fs);
        conductor.prepare (fs);
        shieldC.prepare (fs);
        flex.microDepth = 0.25f;
        modes[0].bandpass (140.0, 3.0, fs);
        modes[1].bandpass (900.0, 4.0, fs);
        modes[2].bandpass (2800.0, 2.5, fs);
        for (auto& m : modes) m.reset();
        triboLp.reset();
        lastTriboFc = -1.0;   // recompute the corner at the new rate
    }

    void reset() noexcept override
    {
        Source::reset();
        conductor.reset();
        shieldC.reset();
        for (auto& m : modes) m.reset();
        mechOut = 0.0f;
    }

    void seed (uint64_t s) noexcept override
    {
        conductor.seed (hashCombine (s, 0x3A));
        shieldC.seed (hashCombine (s, 0x3B));
        flex.seed (hashCombine (s, 0x3C));
        mechRng.seed (hashCombine (s, 0x3D));
    }

    void setParams (const ParamSnapshot& p, float disturbance) noexcept override
    {
        setCommon (p);
        bend = p[P::cbBend];
        shake = p[P::cbShake] * disturbance;
        damage = p[P::cbDamage];
        shieldDamage = p[P::cbShieldDmg];
        mic = p[P::cbMic];
        cableLoadPf = std::clamp (p[P::cableLoad], 50.0f, 5000.0f);
        intermit = p[P::cbIntermit];
        dist = disturbance;
        const auto ws = wearSettingsFor (SourceType::CableHandling, mapSeed, p);
        weak.update (ws);
    }

    const WearMap& weakSpotMap() const noexcept { return weak; }

    float basePosition() const noexcept override { return bend; }

    void control (const GestureOut& g, const SampleCtx& ctx) noexcept override
    {
        if (! primed)
        {
            flex.reset (bend);
            conductor.reset();
            shieldC.reset();
            triboLp.reset();
            primed = true;
        }
        stepSmoothing();

        const float target = bend + g.steer * (g.steerPos - bend);
        flex.step (target, g.displacement * 0.3f * dist, shake, policy);
        const float q = flex.q;
        const float speed = std::abs (flex.v);
        const float W = weak.wearAt (q);
        const float motion = std::min (1.0f, speed / 0.6f);
        const float maxRate = limit (limits::maxEventRate, policy);

        ContactModel::Inputs ci;
        ci.baseline = 1.0f - 0.25f * damage * W;
        ci.hazard = maxRate * 0.7f * W * damage * motion * (0.3f + 0.7f * intermit);
        ci.openProbability = std::clamp (0.2f + 0.7f * damage * W, 0.0f, 0.95f);
        ci.recovery = 0.6f - 0.4f * damage;
        ci.intermittency = intermit;
        ci.depthScale = 0.5f + 0.5f * damage;
        condG = conductor.step (ci, policy);

        ContactModel::Inputs si;
        si.baseline = 1.0f - 0.8f * shieldDamage * W; // bending over broken braid opens it
        si.hazard = maxRate * 0.5f * W * shieldDamage * motion * (0.3f + 0.7f * intermit);
        si.openProbability = 0.6f;
        si.recovery = 0.5f;
        si.intermittency = intermit;
        si.depthScale = 0.8f;
        shieldG = shieldC.step (si, policy);
        humExposure = 1.0f - shieldG;

        // --- microphonic handling (only with Mic) ---
        float excite = 0.0f;
        if (mic > 0.0f)
        {
            const double rate = std::min (2500.0, (double) speed * 900.0);   // stick-slip events per second
            if (rate > 0.0 && mechRng.uniform() < rate / fs)
            {
                excite += (float) (mechRng.bipolar() * std::min (1.0, (double) speed * 0.8));
                ++mechEvents;
            }
            if (g.tapImpulse > 0.0f)
            {
                tapPending = g.tapImpulse * dist;
                ++mechEvents;
            }
        }
        if (tapPending != 0.0f)
        {
            excite += tapPending * 6.0f;
            tapPending = 0.0f;
        }
        const float m = modes[0].process (excite * 0.9f) + modes[1].process (excite * 0.6f) + modes[2].process (excite * 0.35f);
        // triboelectric part of the microphonics (research note 1.8 / 5.10): friction and impact
        // separate charge, i.e. a current into the conductor node. Below the node's RC corner
        // (node impedance x cable capacitance) the voltage is proportional to the node
        // impedance: a passive source (~50 kOhm) lets it through, a buffered / line source
        // (~1 kOhm) holds it down ~34 dB and pushes the corner far above the audio band. Like
        // the rest of the handling path it exists only with Mic (realism spec 5: a sealed cable
        // with Mic at zero makes no handling sound).
        float tribo = 0.0f;
        if (mic > 0.0f)
        {
            const double zNode = kPassiveNodeOhms * (double) nodeS;
            const double fc = 1.0 / (2.0 * kPi * zNode * (double) cableLoadPf * 1.0e-12);
            if (std::abs (fc - lastTriboFc) > 0.5)
            {
                triboLp.setCutoff (fc, fs);
                lastTriboFc = fc;
            }
            tribo = triboLp.lp (excite) * nodeS * kTribo;
        }
        else
            triboLp.reset();
        const float mechPeak = lim (limits::mechanicalPeak);
        mechOut = std::clamp ((m + tribo) * mic * 0.6f, -mechPeak, mechPeak);

        mechEnv = std::abs (mechOut) > mechEnv ? std::abs (mechOut) : mechEnv * 0.9995f;
        tele.mechActivity = mechEnv;
        tele.position = q;
        tele.secondary = 0.5f + g.displacement * 0.5f;
        tele.velocity = speed;
        tele.openness = 1.0f - condG;
        tele.auxOpenness = 1.0f - shieldG;
        tele.wearHere = W;
        tele.state = (int) conductor.getState();
        tele.openEvents = conductor.openEvents + shieldC.openEvents;
        tele.resistiveEvents = conductor.resistiveEvents + shieldC.resistiveEvents;
        tele.mechEvents = mechEvents;
        tele.effective[0] = q;
        tele.effective[1] = shake;
        tele.limited = false;

        uint32_t quiet = 0;
        if (speed < 0.01f) quiet |= QuietStationary;
        if (mic <= 0.0f) quiet |= QuietMicOff;
        if (damage < 0.02f && shieldDamage < 0.02f) quiet |= QuietHealthy;
        if (shieldDamage >= 0.02f && nodeTarget < 1.0f) quiet |= QuietLowImpedance;
        if (ctx.hum <= 0.0f && ctx.buzz <= 0.0f && ctx.rf <= 0.0f) quiet |= QuietHumOff;
        tele.quiet = quiet;
    }

    float pickupCoupling() const noexcept override { return humExposure; }

    void process (float (&paths)[3][2], int numChannels, const SampleCtx& ctx, float envScale) noexcept override
    {
        const float humPeak = lim (limits::humPeak);
        float peakInj = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            paths[1][ch] *= condG;
            // broken braid exposes the inner conductor, which the source drives: the node
            // impedance scales the electric-field hum. The Mic path is an explicit transducer
            // (realism spec 5) and does not depend on the source.
            const float inj = injLp[ch].lp (humExposure * envScale * humPeak * envWave (ctx, 0.5f, ch, nodeS) + mechOut);
            paths[2][ch] = paths[2][ch] * condG + inj;
            peakInj = std::max (peakInj, std::abs (inj));
        }
        trackActivity (peakInj);
    }

private:
    MotionDriver flex;
    ContactModel conductor, shieldC;
    WearMap weak;
    Rng mechRng;
    Biquad modes[3];
    OnePole triboLp;
    static constexpr double kPassiveNodeOhms = 50000.0; // the calibration node of Source::kBufferedNode
    static constexpr float kTribo = 0.33f;              // charge-to-voltage scale (unmeasured, by ear)
    float cableLoadPf = 600.0f;
    double lastTriboFc = -1.0;
    float bend = 0.5f, shake = 0, damage = 0.2f, shieldDamage = 0.1f, mic = 0, intermit = 0.4f, dist = 1;
    float condG = 1, shieldG = 1, humExposure = 0, mechOut = 0, tapPending = 0, mechEnv = 0;
    uint32_t mechEvents = 0;
};

} // namespace ifx
