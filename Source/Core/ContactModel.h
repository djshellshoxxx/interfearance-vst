#pragma once

#include "Random.h"
#include "PhysicalLimits.h"
#include <algorithm>
#include <cmath>

namespace ifx
{

/** Semi-Markov contact: GOOD -> RESISTIVE -> (OPEN) -> RECOVER -> GOOD.
    The owning source computes the event hazard from motion, wear and position; this class
    owns dwell times, clustering (hysteresis), the hard rate cap and the bandlimited
    conductance transition. With zero hazard it never leaves GOOD, so a healthy stationary
    contact is exactly stable. */
class ContactModel
{
public:
    enum class State { Good = 0, Resistive, Open, Recover };

    struct Inputs
    {
        float baseline = 1.0f;     // g0: conductance the geometry allows right now (0..1)
        float hazard = 0.0f;       // eligible excursion rate, events/second (already causal)
        float openProbability = 0.3f; // given an excursion, chance it reaches OPEN
        float shortProbability = 0.0f; // given an OPEN, chance it is a tip-to-sleeve short (quiet)
        float recovery = 0.5f;     // 0 = slow sticky recovery with chatter, 1 = snaps back
        float intermittency = 0.3f; // clustering / chatter tendency
        float depthScale = 1.0f;   // how deep resistive dips go (0..1)
    };

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        // two cascaded one-poles, ~50 us each: a fast but bandlimited physical edge
        edgeCoeff = (float) std::exp (-1.0 / (0.00005 * fs));
        clusterDecay = (float) std::exp (-1.0 / (0.15 * fs));
        reset();
    }

    void reset() noexcept
    {
        state = State::Good;
        dwell = 0;
        refractory = 0;
        cluster = 0.0f;
        g1 = g2 = 1.0f;
        target = 1.0f;
        recoverFrom = 1.0f;
        recoverLen = recoverPos = 0;
        shorted = false;
        primed = false;
    }

    void seed (uint64_t s) noexcept { rng.seed (s); }
    Rng& random() noexcept { return rng; }

    /** Advance one sample, returns the effective conductance in [0,1]. */
    float step (const Inputs& in, Policy policy) noexcept
    {
        const float g0 = std::clamp (in.baseline, 0.0f, 1.0f);
        if (! primed)
        {
            g1 = g2 = target = g0;
            primed = true;
        }

        cluster *= clusterDecay;
        if (refractory > 0)
            --refractory;

        const float maxRate = limit (limits::maxEventRate, policy);
        const int refractorySamples = (int) (fs / (double) maxRate);

        switch (state)
        {
            case State::Good:
            {
                target = g0;
                const float lambda = std::max (0.0f, in.hazard) * (1.0f + 4.0f * cluster * in.intermittency);
                if (lambda > 0.0f && g0 > 0.0f && refractory == 0)
                {
                    const double p = std::min (1.0, (double) lambda / fs);
                    if (rng.uniform() < p)
                        beginExcursion (in, policy, refractorySamples);
                }
                break;
            }
            case State::Resistive:
                target = g0 * depth;
                if (--dwell <= 0)
                {
                    if (rng.uniform() < in.openProbability && refractory == 0)
                    {
                        state = State::Open;
                        shorted = rng.uniform() < in.shortProbability;
                        const float maxOpen = limit (limits::maxOpenMs, policy) * 0.001f;
                        const double median = 0.002 + 0.010 * (1.0 - in.recovery);
                        dwell = samples (std::min ((double) maxOpen, rng.logNormal (median, 0.9)));
                        ++openEvents;
                        refractory = refractorySamples;
                        cluster = std::min (1.0f, cluster + 0.35f);
                    }
                    else
                        beginRecover (in);
                }
                break;
            case State::Open:
                target = 0.0f;
                if (--dwell <= 0)
                {
                    shorted = false;
                    beginRecover (in);
                }
                break;
            case State::Recover:
            {
                ++recoverPos;
                const float t = recoverLen > 0 ? std::min (1.0f, (float) recoverPos / (float) recoverLen) : 1.0f;
                target = recoverFrom + (g0 - recoverFrom) * t;
                // sticky recovery: a worn contact may chatter before it settles
                const float chatter = in.intermittency * (1.0f - in.recovery) * 0.004f * std::min (1.0f, in.hazard * 0.05f);
                if (refractory == 0 && chatter > 0.0f && rng.uniform() < chatter)
                    beginExcursion (in, policy, refractorySamples);
                else if (t >= 1.0f)
                    state = State::Good;
                break;
            }
        }

        // bandlimited physical edge (never an ideal digital step)
        g1 = target + (g1 - target) * edgeCoeff;
        g2 = g1 + (g2 - g1) * edgeCoeff;
        return g2;
    }

    State getState() const noexcept { return state; }
    bool isShorted() const noexcept { return shorted && state == State::Open; }
    float conductance() const noexcept { return g2; }

    // telemetry (audio thread writes, snapshot copies)
    uint32_t openEvents = 0;
    uint32_t resistiveEvents = 0;

private:
    int samples (double seconds) const noexcept { return std::max (1, (int) (seconds * fs)); }

    void beginExcursion (const Inputs& in, Policy policy, int refractorySamples) noexcept
    {
        state = State::Resistive;
        depth = (float) (1.0 - in.depthScale * rng.range (0.25, 0.85));
        const float maxRes = limit (limits::maxResistiveMs, policy) * 0.001f;
        const double median = 0.0015 + 0.006 * (1.0 - in.recovery);
        dwell = samples (std::min ((double) maxRes, rng.logNormal (median, 0.8)));
        refractory = refractorySamples;
        cluster = std::min (1.0f, cluster + 0.5f);
        ++resistiveEvents;
    }

    void beginRecover (const Inputs& in) noexcept
    {
        state = State::Recover;
        recoverFrom = target;
        recoverPos = 0;
        recoverLen = samples (0.0004 + 0.008 * (1.0 - in.recovery) * rng.uniform());
    }

    double fs = 48000.0;
    Rng rng;
    State state = State::Good;
    int dwell = 0, refractory = 0, recoverLen = 0, recoverPos = 0;
    float depth = 1.0f, cluster = 0.0f, clusterDecay = 0.99f;
    float target = 1.0f, recoverFrom = 1.0f;
    float g1 = 1.0f, g2 = 1.0f, edgeCoeff = 0.5f;
    bool shorted = false, primed = false;
};

} // namespace ifx
