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
        switchStep = (float) (1.0 / (0.01 * fs));
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
        contact.seed (hashCombine (s, 0x4A));
        wiper.seed (hashCombine (s, 0x4B));
        jitRng.seed (hashCombine (s, 0x4C));
    }

    void setParams (const ParamSnapshot& p, float disturbance) noexcept override
    {
        setCommon (p);
        turn = p[P::ptTurn];
        taper = p.choice (P::ptTaper);
        toneCircuit = p.choice (P::ptCircuit) == 1;
        dirt = p[P::ptDirt];
        scratch = p[P::ptScratch];
        bias = p[P::ptBias];
        vibration = p[P::ptVibration] * disturbance;
        dist = disturbance;
        const auto ws = wearSettingsFor (SourceType::DirtyPot, mapSeed, p);
        wear.update (ws);
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
            // taper and circuit switches crossfade; after a prepare / reset they start settled
            for (int k = 0; k < 3; ++k) taperW[k] = k == taper ? 1.0f : 0.0f;
            toneW = toneCircuit ? 1.0f : 0.0f;
            toneRunning = toneCircuit;
            primed = true;
        }
        stepSmoothing();

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
        const float depth = lim (limits::scratchGain) * scratch;
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

        // Taper / Level-Tone switches crossfade over 10 ms between the old and new law (a real
        // switch would click; the spec asks that switches never click unintentionally)
        bool taperSettled = true;
        for (int k = 0; k < 3; ++k)
        {
            const float t = k == std::clamp (taper, 0, 2) ? 1.0f : 0.0f;
            taperW[k] = taperW[k] < t ? std::min (t, taperW[k] + switchStep) : std::max (t, taperW[k] - switchStep);
            taperSettled = taperSettled && taperW[k] == t;
        }
        if (taperSettled)
            nominal = taperLaw (q, taper);
        else
        {
            float sum = 0.0f, law = 0.0f;
            for (int k = 0; k < 3; ++k)
                if (taperW[k] > 0.0f) { law += taperW[k] * taperLaw (q, k); sum += taperW[k]; }
            nominal = sum > 0.0f ? law / sum : taperLaw (q, taper);
        }
        actual = nominal * modulation * wiperG;   // the Level circuit's wiper gain
        const float toneT = toneCircuit ? 1.0f : 0.0f;
        toneW = toneW < toneT ? std::min (toneT, toneW + switchStep) : std::max (toneT, toneW - switchStep);

        const float popPeak = lim (limits::popPeak);
        biasOut = bias > 0.0f ? charge.process (nominal * modulation * wiperG, bias * popPeak, popPeak) : 0.0f;
        if (bias <= 0.0f) charge.reset();

        if (toneW <= 0.0f && toneRunning)
        {
            resetTone();                 // the Tone network is out of circuit: restart it clean
            toneRunning = false;
        }
        if (toneW > 0.0f)
        {
            toneRunning = true;
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
            const float lvl[3] = { paths[0][ch] * nominal, paths[1][ch] * actual, paths[2][ch] * actual };
            if (toneW > 0.0f)
            {
                const float tn[3] = { toneNom.process (ch, 0, paths[0][ch]), toneAct.process (ch, 1, paths[1][ch]),
                                      toneAct.process (ch, 2, paths[2][ch]) };
                for (int p = 0; p < 3; ++p)
                    paths[p][ch] = toneW >= 1.0f ? tn[p] : lvl[p] + toneW * (tn[p] - lvl[p]);
            }
            else
                for (int p = 0; p < 3; ++p)
                    paths[p][ch] = lvl[p];
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

    MotionDriver wiper;
    ContactModel contact;
    ChargeCircuit charge;
    WearMap wear;
    Rng jitRng;
    OnePole jitterLp, jitterHp;
    Tone toneNom, toneAct;
    float turn = 0.75f, dirt = 0.3f, scratch = 0.5f, bias = 0, vibration = 0, dist = 1;
    int taper = 1;
    bool toneCircuit = false, toneRunning = false;
    float taperW[3] { 0.0f, 1.0f, 0.0f }, toneW = 0.0f, switchStep = 0.002f;
    float wiperG = 1, modulation = 1, nominal = 1, actual = 1, biasOut = 0;
};

} // namespace ifx
