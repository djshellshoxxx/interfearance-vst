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
        primed = false;
    }

    void reset() noexcept { primed = false; }

    float process (float g, float bias, float peak) noexcept
    {
        const float node = g * bias;
        if (! primed)
        {
            coupling.reset (node); // steady state: no start-up pop
            smooth.reset (0.0f);
            primed = true;
        }
        const float hp = node - coupling.lp (node);
        const float y = smooth.lp (hp);
        return std::clamp (y, -peak, peak);
    }

    OnePole coupling, smooth;
    bool primed = false;
};

} // namespace ifx
