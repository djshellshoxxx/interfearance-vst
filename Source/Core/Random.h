#pragma once

#include <cstdint>
#include <cmath>

// Portable, explicitly implemented random numbers. Never std::random_device, never
// anything that locks. Every stream is derived from (seed, source, slot, index) by
// stable integer hashing so renders are reproducible on any platform.
namespace ifx
{

inline uint64_t splitmix64 (uint64_t x) noexcept
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

inline uint64_t hashCombine (uint64_t a, uint64_t b) noexcept
{
    return splitmix64 (a ^ (splitmix64 (b) + 0x632BE59BD9B4E019ull + (a << 6) + (a >> 2)));
}

inline uint64_t hash3 (uint64_t a, uint64_t b, uint64_t c) noexcept { return hashCombine (hashCombine (a, b), c); }
inline uint64_t hash4 (uint64_t a, uint64_t b, uint64_t c, uint64_t d) noexcept { return hashCombine (hash3 (a, b, c), d); }

/** Uniform [0,1) from a hash value. */
inline double hashToUnit (uint64_t h) noexcept { return (double) (h >> 11) * (1.0 / 9007199254740992.0); }

/** xoshiro256** — small, fast, good statistical quality, trivially copyable. */
class Rng
{
public:
    Rng() noexcept { seed (1); }
    explicit Rng (uint64_t s) noexcept { seed (s); }

    void seed (uint64_t s) noexcept
    {
        uint64_t x = s;
        for (auto& v : st)
        {
            x = splitmix64 (x);
            v = x;
        }
        if ((st[0] | st[1] | st[2] | st[3]) == 0)
            st[0] = 1;
    }

    uint64_t next() noexcept
    {
        const uint64_t result = rotl (st[1] * 5, 7) * 9;
        const uint64_t t = st[1] << 17;
        st[2] ^= st[0];
        st[3] ^= st[1];
        st[1] ^= st[2];
        st[0] ^= st[3];
        st[2] ^= t;
        st[3] = rotl (st[3], 45);
        return result;
    }

    double uniform() noexcept { return hashToUnit (next()); }
    float uniformf() noexcept { return (float) uniform(); }
    double bipolar() noexcept { return uniform() * 2.0 - 1.0; }
    double range (double lo, double hi) noexcept { return lo + (hi - lo) * uniform(); }

    /** Exponential with the given mean. */
    double exponential (double mean) noexcept
    {
        double u = uniform();
        if (u < 1.0e-12) u = 1.0e-12;
        return -std::log (u) * mean;
    }

    /** Approximate standard normal (sum of 4 uniforms, bounded to +-3.46). */
    double gaussian() noexcept
    {
        return (uniform() + uniform() + uniform() + uniform() - 2.0) * 1.7320508075688772;
    }

    /** Log-normal-ish dwell: median * exp(spread * gaussian). */
    double logNormal (double median, double spread) noexcept { return median * std::exp (spread * gaussian()); }

    bool chance (double p) noexcept { return uniform() < p; }

private:
    static uint64_t rotl (uint64_t x, int k) noexcept { return (x << k) | (x >> (64 - k)); }
    uint64_t st[4] {};
};

} // namespace ifx
