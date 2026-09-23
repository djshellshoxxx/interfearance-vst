#pragma once

#include "Source.h"
#include "ChargeCircuit.h"

namespace ifx
{

/** Loose Jack: a TS plug in a worn jack (version 1 models one tip/sleeve relationship only).
    Axial travel q: sleeve (ground) engages first (8-25 %), the tip spring engages at 55-88 %.
    The partly engaged zone and the seeded worn zone are where motion causes excursions.
    Contact changes gate the actual signal; an open tip leaves a floating input that picks up
    hum according to shielding; a tip-to-sleeve short goes quiet instead. */
class LooseJack final : public Source
{
public:
    SourceType type() const noexcept override { return SourceType::LooseJack; }

    void prepare (double sampleRate) noexcept override
    {
        Source::prepare (sampleRate);
        axial.prepare (fs);
        lateral.prepare (fs);
        contact.prepare (fs);
        charge.prepare (fs);
        lateral.microDepth = 0.3f;
        axial.microDepth = 0.01f;
    }

    void reset() noexcept override
    {
        Source::reset();
        contact.reset();
        charge.reset();
    }

    void seed (uint64_t s) noexcept override
    {
        seedValue = s;
        contact.seed (hashCombine (s, 0x1A));
        axial.seed (hashCombine (s, 0x1B));
        lateral.seed (hashCombine (s, 0x1C));
    }

    void setParams (const ParamSnapshot& p, float disturbance) noexcept override
    {
        policy = (Policy) p.choice (P::policy);
        insert = p[P::jkInsert];
        wiggle = p[P::jkWiggle] * disturbance;
        pressure = p[P::jkPressure];
        intermit = p[P::jkIntermit];
        recovery = p[P::jkRecovery];
        shortProb = p[P::jkShort];
        popAmount = p[P::jkPop];
        shield = p[P::jkShield];
        WearMap::Settings ws;
        ws.seed = hashCombine (seedValue, 0x3A);
        ws.wear = p[P::jkWear];
        ws.centre = p[P::jkWearPos];
        ws.width = 0.1f;
        ws.background = 0.08f;
        wear.update (ws);
        setBandwidth (policy);
    }

    const WearMap& wearMap() const noexcept { return wear; }

    /** Pure geometry helpers shared with the illustration and tests. */
    static float tipEngagement (float q) noexcept { return smoothstep (0.55f, 0.88f, q); }
    static float sleeveEngagement (float q) noexcept { return smoothstep (0.08f, 0.25f, q); }

    float basePosition() const noexcept override { return insert; }

    void control (const GestureOut& g, const SampleCtx& ctx) noexcept override
    {
        if (! primed)
        {
            axial.reset (insert);
            lateral.reset (0.5f);
            contact.reset();
            charge.reset();
            primed = true;
        }

        const float axialTarget = insert + g.steer * (g.steerPos - insert);
        axial.step (axialTarget, 0.02f * g.tapImpulse, 0.0f, policy);
        lateral.step (0.5f, g.displacement * 0.5f + 0.05f * g.tapImpulse, wiggle, policy);

        const float q = axial.q;
        const float speed = std::abs (axial.v) + std::abs (lateral.v) * (1.0f - 0.7f * pressure);
        const float W = wear.wearAt (q);
        const float e = tipEngagement (q);
        const float z = 4.0f * e * (1.0f - e);
        const float motion = std::min (1.0f, speed / 0.5f);
        const float stability = 1.2f - pressure;
        const float maxRate = limit (limits::maxEventRate, policy);

        ContactModel::Inputs in;
        in.baseline = e * (1.0f - 0.15f * W * (1.0f - pressure));
        in.hazard = maxRate * 0.5f * (W + 0.6f * z) * motion * (0.3f + 0.7f * intermit) * stability
                  + intermit * intermit * z * 4.0f * stability;
        in.openProbability = std::clamp (0.15f + 0.6f * W + 0.3f * z, 0.0f, 0.95f);
        in.shortProbability = shortProb;
        in.recovery = recovery;
        in.intermittency = intermit;
        in.depthScale = 0.4f + 0.6f * std::max (W, z);

        tipG = contact.step (in, policy);
        sleeveG = sleeveEngagement (q);
        gain = tipG * sleeveG;

        const bool shorted = contact.isShorted();
        // In the partly inserted zone the tip can rest across the sleeve spring: `Short` is also
        // the share of that zone spent grounded (quiet) instead of floating (hum pickup).
        const float groundedShare = shortProb * z * 0.8f;
        exposure = shorted ? 0.0f : (1.0f - tipG) * (0.45f + 0.55f * (1.0f - sleeveG)) * (1.0f - 0.95f * shield) * (1.0f - groundedShare);
        bright = 1.0f - sleeveG;
        popValue = charge.process (tipG * sleeveG, popAmount * limit (limits::popPeak, policy), limit (limits::popPeak, policy));

        tele.position = q;
        tele.secondary = lateral.q;
        tele.velocity = speed;
        tele.openness = 1.0f - tipG;
        tele.auxOpenness = 1.0f - sleeveG;
        tele.wearHere = W;
        tele.state = (int) contact.getState();
        tele.shorted = shorted;
        tele.openEvents = contact.openEvents;
        tele.resistiveEvents = contact.resistiveEvents;
        tele.effective[0] = insert;
        tele.effective[1] = wiggle;
        tele.limited = false;

        uint32_t quiet = 0;
        if (speed < 0.01f && in.hazard < 0.05f) quiet |= QuietStationary;
        if (e > 0.99f && W < 0.05f) quiet |= QuietHealthy;
        if (shield > 0.9f) quiet |= QuietShielded;
        if (ctx.hum <= 0.0f && ctx.buzz <= 0.0f && ctx.rf <= 0.0f) quiet |= QuietHumOff;
        if (gain < 0.05f && ctx.inputLevel > 1.0e-4f) quiet |= QuietNoInput;
        tele.quiet = quiet;
    }

    float pickupCoupling() const noexcept override { return exposure; }

    void process (float (&paths)[3][2], int numChannels, const SampleCtx& ctx, float envScale) noexcept override
    {
        const float humPeak = limit (limits::humPeak, ctx.policy);
        float peakInj = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // [0] R: healthy seated plug = identity
            paths[1][ch] *= gain;
            const float inj = injLp[ch].lp (exposure * envScale * humPeak * envWave (ctx, bright, ch)) + popValue;
            paths[2][ch] = paths[2][ch] * gain + inj;
            peakInj = std::max (peakInj, std::abs (inj));
        }
        trackActivity (peakInj);
    }

private:
    uint64_t seedValue = 1;
    MotionDriver axial, lateral;
    ContactModel contact;
    ChargeCircuit charge;
    WearMap wear;
    float insert = 1, wiggle = 0, pressure = 0.6f, intermit = 0.3f, recovery = 0.6f, shortProb = 0.2f, popAmount = 0, shield = 0.8f;
    float tipG = 1, sleeveG = 1, gain = 1, exposure = 0, bright = 0, popValue = 0;
};

} // namespace ifx
