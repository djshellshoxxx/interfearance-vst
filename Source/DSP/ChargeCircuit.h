#pragma once

#include "Filters.h"

namespace ifx
{

/** Bias/charge model: a DC bias V behind a contact of conductance g charges a coupling
    capacitor. The audible output is the capacitor-coupled (high-passed) node voltage, so a
    pop exists only when g (or V) CHANGES; a steady contact relaxes to silence and a zero
    bias gives no pop at all. The pop is bounded and band-limited. */
struct ChargeCircuit
{
    void prepare (double fs) noexcept
    {
        coupling.setCutoff (7.0, fs);
        smooth.setCutoff (7000.0, fs);
        biasK = (float) (1.0 - std::exp (-1.0 / (0.05 * fs)));
        primed = false;
    }

    void reset() noexcept { primed = false; }

    /** Only a CHANGE of the contact conductance g makes a pop: the coupling high-pass acts on
        g and the (slowly smoothed) bias scales the result, so turning Pop / Circuit Bias or
        switching the range policy on a still contact is silent (realism spec: no event
        without a cause). */
    float process (float g, float bias, float peak) noexcept
    {
        if (! primed)
        {
            coupling.reset (g); // steady state: no start-up pop
            smooth.reset (0.0f);
            biasS = bias;
            primed = true;
        }
        biasS += (bias - biasS) * biasK;
        const float hp = (g - coupling.lp (g)) * biasS;
        const float y = smooth.lp (hp);
        return std::clamp (y, -peak, peak);
    }

    OnePole coupling, smooth;
    float biasS = 0.0f, biasK = 0.001f;
    bool primed = false;
};

} // namespace ifx
