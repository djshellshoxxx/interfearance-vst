#pragma once

#include <array>
#include <cmath>

namespace ifx
{

/** Musical scale helpers (pure functions, no allocation, safe on the audio thread). Used to keep the
    Studio resonator in key. */
namespace Scale
{
    constexpr int kNumScales = 8;

    /** Choice list for the Scale parameter (order is the stored choice index; append only). */
    constexpr const char* kScaleNames = "Chromatic|Major|Minor|Dorian|Mixolydian|Pentatonic Major|Pentatonic Minor|Whole Tone";
    constexpr const char* kKeyNames = "C|C#|D|D#|E|F|F#|G|G#|A|A#|B";

    /** Pitch classes (semitones above the key) that belong to each scale. */
    inline const std::array<bool, 12>& mask (int scale) noexcept
    {
        static const std::array<std::array<bool, 12>, kNumScales> masks = [] {
            const int degrees[kNumScales][8] = {
                { 0, 1, 2, 3, 4, 5, 6, 7 },   // Chromatic: replaced below
                { 0, 2, 4, 5, 7, 9, 11, -1 },
                { 0, 2, 3, 5, 7, 8, 10, -1 },
                { 0, 2, 3, 5, 7, 9, 10, -1 },
                { 0, 2, 4, 5, 7, 9, 10, -1 },
                { 0, 2, 4, 7, 9, -1, -1, -1 },
                { 0, 3, 5, 7, 10, -1, -1, -1 },
                { 0, 2, 4, 6, 8, 10, -1, -1 } };
            std::array<std::array<bool, 12>, kNumScales> m {};
            for (int s = 0; s < kNumScales; ++s)
                for (int d : degrees[s])
                    if (d >= 0) m[(size_t) s][(size_t) d] = true;
            m[0].fill (true);
            return m;
        }();
        return masks[(size_t) (scale < 0 ? 0 : scale >= kNumScales ? kNumScales - 1 : scale)];
    }

    /** Nearest in-scale MIDI note (fractional input allowed) for `key` (0 = C) and `scale`. Ties go down. */
    inline int snapMidi (double midi, int key, int scale) noexcept
    {
        const auto& m = mask (scale);
        const int centre = (int) std::lround (midi);
        for (int d = 0; d <= 6; ++d)
            for (int sign : { -1, 1 })
            {
                const int n = centre + sign * d;
                if (m[(size_t) (((n - key) % 12 + 12) % 12)])
                {
                    // prefer the candidate closest to the unrounded input
                    const int other = centre + (-sign) * d;
                    if (d > 0 && m[(size_t) (((other - key) % 12 + 12) % 12)]
                        && std::abs (midi - other) < std::abs (midi - n))
                        return other;
                    return n;
                }
            }
        return centre;
    }

    /** Snap a frequency in Hz to the nearest in-scale note (A4 = 440 Hz, equal temperament). */
    inline double snapHz (double hz, int key, int scale) noexcept
    {
        if (hz <= 0.0) return hz;
        const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
        return 440.0 * std::pow (2.0, (snapMidi (midi, key, scale) - 69) / 12.0);
    }
}

} // namespace ifx
