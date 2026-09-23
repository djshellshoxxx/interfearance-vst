#pragma once

#include "Random.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace ifx
{

/** Smooth, seeded value noise on [0,1] with `cells` lattice points; returns [-1,1]. */
inline float valueNoise (uint64_t seed, float x, int cells) noexcept
{
    const float fx = std::clamp (x, 0.0f, 1.0f) * (float) cells;
    const int i0 = std::min ((int) fx, cells - 1);
    const float t = fx - (float) i0;
    const float a = (float) hashToUnit (hash3 (seed, 0x5157ull, (uint64_t) i0)) * 2.0f - 1.0f;
    const float b = (float) hashToUnit (hash3 (seed, 0x5157ull, (uint64_t) (i0 + 1))) * 2.0f - 1.0f;
    const float s = (1.0f - std::cos (t * 3.14159265f)) * 0.5f;
    return a + (b - a) * s;
}

/** A persistent, seeded susceptibility map W(q) in [0,1] over normalised travel q, plus a
    fine surface roughness R(q) in [-1,1]. The UI draws exactly this map (it is a pure function
    of seed + parameters), so the worn arc on screen is the one the DSP uses. */
class WearMap
{
public:
    static constexpr int kSize = 512;

    struct Settings
    {
        uint64_t seed = 1;
        float wear = 0.0f;       // 0 = pristine, 1 = badly worn
        float centre = 0.5f;     // location of the worn region along the travel
        float width = 0.15f;     // half width of the worn region (normalised)
        float background = 0.05f; // proportion of wear spread over the whole travel (ageing)

        bool operator== (const Settings& o) const noexcept
        {
            return seed == o.seed && wear == o.wear && centre == o.centre && width == o.width && background == o.background;
        }
        bool operator!= (const Settings& o) const noexcept { return ! (*this == o); }
    };

    /** Rebuild only when the settings changed; cheap enough (512 exp) for a block boundary. */
    void update (const Settings& s) noexcept
    {
        if (valid && s == current)
            return;
        current = s;
        valid = true;
        const float w = std::max (0.02f, s.width);
        for (int i = 0; i < kSize; ++i)
        {
            const float q = (float) i / (float) (kSize - 1);
            wearTable[(size_t) i] = compute (s, q, w);
            roughTable[(size_t) i] = 0.65f * valueNoise (hashCombine (s.seed, 0xA11ull), q, 97)
                                   + 0.35f * valueNoise (hashCombine (s.seed, 0xB22ull), q, 331);
        }
    }

    /** Pure evaluation (used by the UI and tests). */
    static float compute (const Settings& s, float q, float w) noexcept
    {
        const float d = (q - s.centre) / w;
        float bump = std::exp (-0.5f * d * d * 2.2f);
        // lumps inside the region so a worn arc has 2-3 distinguishable rough spots
        const float lumps = 0.7f + 0.3f * valueNoise (hashCombine (s.seed, 0x77ull), q, 23);
        bump *= lumps;
        const float ageing = s.background * (0.5f + 0.5f * valueNoise (hashCombine (s.seed, 0x99ull), q, 11));
        return std::clamp (s.wear * (bump + ageing), 0.0f, 1.0f);
    }

    float wearAt (float q) const noexcept { return lookup (wearTable, q); }
    float roughAt (float q) const noexcept { return lookup (roughTable, q); }
    const Settings& settings() const noexcept { return current; }

private:
    static float lookup (const std::array<float, kSize>& t, float q) noexcept
    {
        const float x = std::clamp (q, 0.0f, 1.0f) * (float) (kSize - 1);
        const int i = std::min ((int) x, kSize - 2);
        const float f = x - (float) i;
        return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
    }

    Settings current;
    bool valid = false;
    std::array<float, kSize> wearTable {};
    std::array<float, kSize> roughTable {};
};

} // namespace ifx
