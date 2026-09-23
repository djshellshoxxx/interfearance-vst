#pragma once

#include <cmath>
#include <algorithm>

namespace ifx
{

constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return db <= -120.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return g <= 1.0e-6f ? -120.0f : 20.0f * std::log10 (g); }

/** Flush tiny values (belt and braces on top of ScopedNoDenormals / FTZ). */
inline float undenorm (float x) noexcept { return std::abs (x) < 1.0e-20f ? 0.0f : x; }

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (double hz, double fs) noexcept { a = (float) (1.0 - std::exp (-2.0 * kPi * std::min (hz, fs * 0.49) / fs)); }
    void setTime (double seconds, double fs) noexcept { a = seconds <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (seconds * fs))); }
    float lp (float x) noexcept { z += a * (x - z); z = undenorm (z); return z; }
    float hp (float x) noexcept { return x - lp (x); }
    void reset (float v = 0.0f) noexcept { z = v; }
};

/** DC blocker: y = x - x1 + R y1 (R from a ~5 Hz corner). */
struct DcBlocker
{
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
    void prepare (double fs, double hz = 5.0) noexcept { r = (float) std::exp (-2.0 * kPi * hz / fs); reset(); }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = undenorm (y);
        return y1;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
};

/** Transposed direct form II biquad (double precision state). */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    float process (float xf) noexcept
    {
        const double x = xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        if (std::abs (z1) < 1e-25) z1 = 0;
        if (std::abs (z2) < 1e-25) z2 = 0;
        return (float) y;
    }
    void reset() noexcept { z1 = z2 = 0; }
    void setIdentity() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void setNormalised (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }

    // RBJ cookbook designs
    void lowpass (double f, double q, double fs) noexcept
    {
        const double w = 2 * kPi * std::clamp (f, 5.0, fs * 0.49) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double f, double q, double fs) noexcept
    {
        const double w = 2 * kPi * std::clamp (f, 5.0, fs * 0.49) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double f, double q, double fs) noexcept
    {
        const double w = 2 * kPi * std::clamp (f, 5.0, fs * 0.49) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (double f, double q, double gainDb, double fs) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * kPi * std::clamp (f, 5.0, fs * 0.49) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double f, double gainDb, double fs) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * kPi * std::clamp (f, 5.0, fs * 0.49) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                       (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (double f, double gainDb, double fs) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * kPi * std::clamp (f, 5.0, fs * 0.49) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                       (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }

    /** Analog prototype via bilinear transform with prewarp at fw:
        H(s) = (n0 + n1 s + n2 s^2) / (d0 + d1 s + d2 s^2) */
    void bilinear (double n0, double n1, double n2, double d0, double d1, double d2, double fw, double fs) noexcept
    {
        const double w = 2 * kPi * std::clamp (fw, 1.0, fs * 0.45);
        const double k = w / std::tan (w / (2 * fs));
        const double k2 = k * k;
        setNormalised (n0 + n1 * k + n2 * k2, 2 * n0 - 2 * n2 * k2, n0 - n1 * k + n2 * k2,
                       d0 + d1 * k + d2 * k2, 2 * d0 - 2 * d2 * k2, d0 - d1 * k + d2 * k2);
    }
};

/** First-order antiderivative antialiasing (Parker, Zavalishin, Le Bivic, DAFx 2016)
    for three memoryless curves. Zero latency; the half-sample delay of ADAA1 is identical
    on every path that uses it, so bus alignment is preserved. */
struct AdaaSaturator
{
    enum Curve { Soft = 0, Hard = 1, Asym = 2 };
    Curve curve = Soft;
    double x1 = 0.0;
    double F1 = 0.0;
    bool primed = false;

    static double f (double x, Curve c) noexcept
    {
        switch (c)
        {
            case Hard: return std::clamp (x, -1.0, 1.0);
            case Asym: return x >= 0 ? std::tanh (x) : std::tanh (0.6 * x) / 0.6 * 0.7 + 0.0; // bias-free but asymmetric
            default:   return std::tanh (x);
        }
    }
    static double logcosh (double x) noexcept
    {
        const double ax = std::abs (x);
        return ax + std::log1p (std::exp (-2.0 * ax)) - 0.6931471805599453;
    }
    static double F (double x, Curve c) noexcept
    {
        switch (c)
        {
            case Hard: return std::abs (x) <= 1.0 ? 0.5 * x * x : std::abs (x) - 0.5;
            case Asym: return x >= 0 ? logcosh (x) : logcosh (0.6 * x) / 0.36 * 0.7;
            default:   return logcosh (x);
        }
    }

    void reset() noexcept { x1 = 0.0; F1 = F (0.0, curve); primed = false; }

    float process (float xf) noexcept
    {
        const double x = xf;
        if (! primed) { x1 = x; F1 = F (x, curve); primed = true; return (float) f (x, curve); }
        const double Fx = F (x, curve);
        const double d = x - x1;
        double y;
        if (std::abs (d) < 1.0e-5)
            y = f (0.5 * (x + x1), curve);
        else
            y = (Fx - F1) / d;
        x1 = x;
        F1 = Fx;
        return (float) y;
    }
};

} // namespace ifx
