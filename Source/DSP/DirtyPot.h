#pragma once

#include "Source.h"
#include "ChargeCircuit.h"

namespace ifx
{

/** Dirty Pot: finite rotary track + wiper, Level or Tone circuit. Taper law, dirty-wiper
    contact and circuit bias are separate mechanisms:
      - taper sets the nominal wiper law (also applied to the healthy reference R);
      - the wiper contact follows the seeded worn arc and only produces excursions while the
        wiper moves (a severely dirty pot can stay intermittent under explicit Vibration);
      - scratch is a multiplicative resistive irregularity of the through signal, plus an
        optional DC bias across the pot that makes it audible on silence (powered circuit).
    Tone circuit (documented topology): source impedance Rs = 15 kOhm into a shunt of the
    pot's resistance R (250 kOhm, via taper) in series with C = 22 nF to ground,
        H(s) = (1 + s R C) / (1 + s (R + Rs) C)
    so R = 0 is a treble cut at ~480 Hz and full R is nearly flat. An open wiper lifts the
    capacitor (R -> infinity), i.e. brightness jumps. */
class DirtyPot final : public Source
{
public:
    SourceType type() const noexcept override { return SourceType::DirtyPot; }

    static constexpr double kRpot = 250000.0, kRs = 15000.0, kC = 22.0e-9;

    void prepare (double sampleRate) noexcept override
    {
        Source::prepare (sampleRate);
        wiper.prepare (fs);
        contact.prepare (fs);
        charge.prepare (fs);
        wiper.microDepth = 0.004f;
        jitterLp.setCutoff (3500.0, fs);
        jitterHp.setCutoff (300.0, fs);
        resetTone();
    }

    void reset() noexcept override
    {
        Source::reset();
        contact.reset();
        charge.reset();
        resetTone();
    }

    void seed (uint64_t s) noexcept override
    {
        seedValue = s;
        contact.seed (hashCombine (s, 0x4A));
        wiper.seed (hashCombine (s, 0x4B));
        jitRng.seed (hashCombine (s, 0x4C));
    }

    void setParams (const ParamSnapshot& p, float disturbance) noexcept override
    {
        policy = (Policy) p.choice (P::policy);
        turn = p[P::ptTurn];
        taper = p.choice (P::ptTaper);
        toneCircuit = p.choice (P::ptCircuit) == 1;
        dirt = p[P::ptDirt];
        scratch = p[P::ptScratch];
        bias = p[P::ptBias];
        vibration = p[P::ptVibration] * disturbance;
        dist = disturbance;
        WearMap::Settings ws;
        ws.seed = hashCombine (seedValue, 0x5A);
        ws.wear = dirt;
        ws.centre = p[P::ptWearPos];
        ws.width = p[P::ptWearWidth];
        ws.background = 0.06f;
        wear.update (ws);
        setBandwidth (policy);
    }

    const WearMap& wearMap() const noexcept { return wear; }

    /** Nominal wiper law (Holmes & van Walstijn motivate keeping taper separate from dirt). */
    static float taperLaw (float q, int taperType) noexcept
    {
        q = std::clamp (q, 0.0f, 1.0f);
        constexpr float b = 81.0f; // law(0.5) = 0.1 (10 % at half rotation, typical A taper)
        switch (taperType)
        {
            case 1: return (std::pow (b, q) - 1.0f) / (b - 1.0f);
            case 2: return 1.0f - (std::pow (b, 1.0f - q) - 1.0f) / (b - 1.0f);
            default: return q;
        }
    }

    float basePosition() const noexcept override { return turn; }

    void control (const GestureOut& g, const SampleCtx& ctx) noexcept override
    {
        if (! primed)
        {
            wiper.reset (turn);
            contact.reset();
            charge.reset();
            resetTone();
            primed = true;
        }

        const float target = turn + g.steer * (g.steerPos - turn);
        // a knob has no tremor of its own: only explicit vibration moves a still knob
        wiper.step (target, g.displacement * 0.08f * dist, vibration, policy);
        const float q = wiper.q;
        const float speed = std::abs (wiper.v);
        const float W = wear.wearAt (q);
        // deliberate turning only: vibration of a still knob is handled by the `severe` term
        const float motion = std::min (1.0f, std::min (speed, std::abs (wiper.vIntent)) / 0.3f);
        const float maxRate = limit (limits::maxEventRate, policy);

        ContactModel::Inputs in;
        in.baseline = 1.0f;
        const float severe = std::max (0.0f, (dirt - 0.7f) / 0.3f);
        in.hazard = maxRate * 0.8f * W * motion + severe * vibration * W * 25.0f;
        in.openProbability = std::clamp (0.1f + 0.6f * W, 0.0f, 0.9f);
        in.recovery = 0.7f - 0.5f * dirt;
        in.intermittency = dirt;
        in.depthScale = 0.3f + 0.6f * dirt;
        wiperG = contact.step (in, policy);

        // fine resistive irregularity: exists only while the wiper travels over dirt
        const float depth = limit (limits::scratchGain, policy) * scratch;
        const float jitAmp = motion * W * depth;
        float jit = 0.0f;
        if (jitAmp > 0.0f)
            jit = jitterHp.hp (jitterLp.lp ((float) jitRng.bipolar())) * 2.5f * jitAmp;
        else
        {
            jitterLp.reset();
            jitterHp.reset();
        }
        // stable resistive error at rest (position dependent, no noise when still)
        const float staticErr = depth * W * 0.3f * (0.5f + 0.5f * wear.roughAt (q));
        modulation = std::clamp (1.0f - staticErr + jit, 0.0f, 1.5f);

        nominal = taperLaw (q, taper);
        actual = toneCircuit ? nominal : nominal * modulation * wiperG;

        const float popPeak = limit (limits::popPeak, policy);
        biasOut = bias > 0.0f ? charge.process (nominal * modulation * wiperG, bias * popPeak, popPeak) : 0.0f;
        if (bias <= 0.0f) charge.reset();

        if (toneCircuit)
        {
            const double Rnom = kRpot * (double) nominal;
            const double gEff = std::max (1.0e-3, (double) (modulation * wiperG));
            const double Ract = std::min (1.0e7, Rnom / gEff);
            setTone (toneNom, Rnom);
            setTone (toneAct, Ract);
        }

        tele.position = q;
        tele.secondary = nominal;
        tele.velocity = speed;
        tele.openness = 1.0f - wiperG * std::min (1.0f, modulation);
        tele.wearHere = W;
        tele.state = (int) contact.getState();
        tele.openEvents = contact.openEvents;
        tele.resistiveEvents = contact.resistiveEvents;
        tele.effective[0] = q;
        tele.effective[1] = nominal;
        tele.limited = false;

        uint32_t quiet = 0;
        if (speed < 0.005f) quiet |= QuietStationary;
        if (dirt < 0.02f) quiet |= QuietHealthy;
        if (ctx.inputLevel < 1.0e-5f && bias <= 0.0f) quiet |= QuietNoInput;
        tele.quiet = quiet;
    }

    void process (float (&paths)[3][2], int numChannels, const SampleCtx&, float) noexcept override
    {
        float peakInj = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (toneCircuit)
            {
                paths[0][ch] = toneNom.process (ch, 0, paths[0][ch]);
                paths[1][ch] = toneAct.process (ch, 1, paths[1][ch]);
                paths[2][ch] = toneAct.process (ch, 2, paths[2][ch]);
            }
            else
            {
                paths[0][ch] *= nominal;
                paths[1][ch] *= actual;
                paths[2][ch] *= actual;
            }
            const float inj = injLp[ch].lp (biasOut);
            paths[2][ch] += inj;
            peakInj = std::max (peakInj, std::abs (inj));
        }
        trackActivity (peakInj);
    }

private:
    /** First-order tone section with separate state per (channel, path). */
    struct Tone
    {
        double b0 = 1, b1 = 0, a1 = 0;
        double z[2][3] {};
        float process (int ch, int path, float x) noexcept
        {
            const double y = b0 * x + z[ch][path];
            z[ch][path] = b1 * x - a1 * y;
            if (std::abs (z[ch][path]) < 1e-25) z[ch][path] = 0;
            return (float) y;
        }
    };

    void setTone (Tone& t, double R) const noexcept
    {
        const double tz = R * kC, tp = (R + kRs) * kC;
        const double k = 2.0 * fs;
        const double d = 1.0 + k * tp;
        t.b0 = (1.0 + k * tz) / d;
        t.b1 = (1.0 - k * tz) / d;
        t.a1 = (1.0 - k * tp) / d;
    }

    void resetTone() noexcept
    {
        for (auto* t : { &toneNom, &toneAct })
            for (auto& c : t->z) for (auto& v : c) v = 0.0;
    }

    uint64_t seedValue = 1;
    MotionDriver wiper;
    ContactModel contact;
    ChargeCircuit charge;
    WearMap wear;
    Rng jitRng;
    OnePole jitterLp, jitterHp;
    Tone toneNom, toneAct;
    float turn = 0.75f, dirt = 0.3f, scratch = 0.5f, bias = 0, vibration = 0, dist = 1;
    int taper = 1;
    bool toneCircuit = false;
    float wiperG = 1, modulation = 1, nominal = 1, actual = 1, biasOut = 0;
};

} // namespace ifx
