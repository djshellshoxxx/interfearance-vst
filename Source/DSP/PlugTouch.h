#pragma once

#include "Source.h"
#include "ChargeCircuit.h"

namespace ifx
{

/** Exposed Plug Touch: an artistic, simplified input-coupling model (not an exact
    human-body circuit). A finger on a floating, high-impedance tip couples the local mains
    field into the input; contact area sets coupling, pressure sets stability, Body the
    coupling colour, Shield and Circuit Exposure reduce pickup. An untouched exposed tip
    picks up a weaker ambient hum. Touch onset/offset can chatter and click (charge). */
class PlugTouch final : public Source
{
public:
    SourceType type() const noexcept override { return SourceType::PlugTouch; }

    void prepare (double sampleRate) noexcept override
    {
        Source::prepare (sampleRate);
        finger.prepare (fs);
        contact.prepare (fs);
        charge.prepare (fs);
        finger.microDepth = 0.05f;
        for (auto& f : loadLp) for (auto& l : f) l.reset();
    }

    void reset() noexcept override
    {
        Source::reset();
        contact.reset();
        charge.reset();
        for (auto& f : loadLp) for (auto& l : f) l.reset();
    }

    void seed (uint64_t s) noexcept override
    {
        contact.seed (hashCombine (s, 0x2A));
        finger.seed (hashCombine (s, 0x2B));
    }

    void setParams (const ParamSnapshot& p, float disturbance) noexcept override
    {
        setCommon (p);
        touch = p[P::tcTouch];
        pressure = p[P::tcPressure];
        body = p[P::tcBody];
        shield = p[P::tcShield];
        exposureParam = p[P::tcExposure];
        popAmount = p[P::tcPop];
        dist = disturbance;
    }

    float basePosition() const noexcept override { return touch; }

    void control (const GestureOut& g, const SampleCtx& ctx) noexcept override
    {
        if (! primed)
        {
            finger.reset (touch);
            contact.reset();
            charge.reset();
            primed = true;
        }
        stepSmoothing();

        const float target = std::clamp (touch + g.pressure * dist, 0.0f, 1.0f);
        const bool touching = target > 0.02f || finger.q > 0.02f;
        // a light finger trembles; a firm one is steady. Tremor exists only while touching.
        const float tremor = touching ? (1.0f - pressure) * 0.6f : 0.0f;
        finger.step (target, g.displacement * 0.15f * dist, tremor, policy);

        const float area = finger.q;
        const float speed = std::abs (finger.v);
        const float edge = 1.0f - smoothstep (0.05f, 0.35f, area); // making/breaking region
        const float maxRate = limit (limits::maxEventRate, policy);

        ContactModel::Inputs in;
        in.baseline = smoothstep (0.01f, 0.2f, area);
        in.hazard = maxRate * 0.6f * std::min (1.0f, speed / 0.8f) * (0.3f + 0.7f * edge) * (0.4f + 0.6f * (1.0f - pressure));
        in.openProbability = 0.5f;
        in.shortProbability = 0.0f;
        in.recovery = 0.3f + 0.6f * pressure;
        in.intermittency = 1.0f - pressure;
        in.depthScale = 0.8f;
        contactG = contact.step (in, policy);

        const float exposure = exposureParam * (1.0f - 0.92f * shield);
        const float ambient = 0.12f;
        fingerCoupling = contactG * std::sqrt (area);
        coupling = exposure * (ambient + (1.0f - ambient) * fingerCoupling);
        // finger to ground loads a high-Z source; a buffered / line source barely notices it
        load = contactG * area * exposureParam; // the floating tip's node impedance is Circuit Exposure, not the source
        brightness = std::clamp (0.25f + 0.75f * body * (0.4f + 0.6f * fingerCoupling), 0.0f, 1.0f);
        const float popPeak = lim (limits::popPeak);
        popValue = charge.process (contactG, popAmount * 0.6f * popPeak, popPeak);

        tele.position = area;
        tele.secondary = 0.5f + finger.v * 0.1f;
        tele.velocity = speed;
        tele.openness = 1.0f - contactG;
        tele.auxOpenness = coupling;
        tele.wearHere = 0.0f;
        tele.state = (int) contact.getState();
        tele.openEvents = contact.openEvents;
        tele.resistiveEvents = contact.resistiveEvents;
        tele.effective[0] = area;
        tele.effective[1] = coupling;
        tele.limited = false;

        uint32_t quiet = 0;
        if (area < 0.02f) quiet |= QuietNotTouched;
        if (exposure < 0.1f) quiet |= QuietShielded;
        if (ctx.hum <= 0.0f && ctx.buzz <= 0.0f && ctx.rf <= 0.0f) quiet |= QuietHumOff;
        if (speed < 0.01f) quiet |= QuietStationary;
        tele.quiet = quiet;
    }

    float pickupCoupling() const noexcept override { return coupling; }

    void process (float (&paths)[3][2], int numChannels, const SampleCtx& ctx, float envScale) noexcept override
    {
        const float humPeak = lim (limits::humPeak);
        const float loadGain = 1.0f - 0.25f * load;
        const bool loaded = load > 1.0e-4f;
        if (loaded)
        {
            loadLp[0][0].setCutoff (20000.0 - 14000.0 * (double) load, fs);
            for (auto& f : loadLp) for (auto& l : f) l.a = loadLp[0][0].a;
        }
        float peakInj = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // [0] R: untouched, terminated tip = identity
            for (int p = 1; p < 3; ++p)
            {
                auto& lp = loadLp[p - 1][ch];
                if (loaded)
                    paths[p][ch] = lp.lp (paths[p][ch]) * loadGain;
                else
                    lp.reset (paths[p][ch]); // untouched: exact identity, filter tracks the signal
            }
            // the touched tip is driven by the source: its impedance sets the mains pickup
            const float inj = injLp[ch].lp (coupling * envScale * humPeak * envWave (ctx, brightness, ch, 1.0f)) + popValue;
            paths[2][ch] += inj;
            peakInj = std::max (peakInj, std::abs (inj));
        }
        trackActivity (peakInj);
    }

private:
    MotionDriver finger;
    ContactModel contact;
    ChargeCircuit charge;
    OnePole loadLp[2][2];
    float touch = 0, pressure = 0.5f, body = 0.4f, shield = 0.2f, exposureParam = 0.7f, popAmount = 0.2f, dist = 1;
    float contactG = 0, fingerCoupling = 0, coupling = 0, load = 0, brightness = 0.3f, popValue = 0;
};

} // namespace ifx
