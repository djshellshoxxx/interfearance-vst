#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <vector>
#include <cmath>

//==============================================================================
/** A very small test runner. Deliberately not a dependency: the plugin ships
    one console binary and it should build with nothing but JUCE. */
namespace ifxtest
{

struct Failure
{
    juce::String suite, test, message, file;
    int line = 0;
};

class Registry
{
public:
    struct Entry
    {
        juce::String suite;
        juce::String name;
        std::function<void()> fn;
        bool slow = false;
    };

    static Registry& get()
    {
        static Registry r;
        return r;
    }

    void add (const juce::String& suite, const juce::String& name,
              std::function<void()> fn, bool slow = false)
    {
        entries.push_back ({ suite, name, std::move (fn), slow });
    }

    const std::vector<Entry>& all() const { return entries; }

    void recordPass() { ++passCount; }
    void recordFail (const Failure& f) { failures.push_back (f); }

    int getPassCount() const { return passCount; }
    const std::vector<Failure>& getFailures() const { return failures; }
    void resetCounts() { passCount = 0; failures.clear(); }

    juce::String currentSuite, currentTest;

private:
    std::vector<Entry> entries;
    std::vector<Failure> failures;
    int passCount = 0;
};

struct Registrar
{
    Registrar (const char* suite, const char* name, std::function<void()> fn, bool slow = false)
    {
        Registry::get().add (suite, name, std::move (fn), slow);
    }
};

inline void check (bool condition, const juce::String& message, const char* file, int line)
{
    auto& r = Registry::get();
    if (condition)
        r.recordPass();
    else
        r.recordFail ({ r.currentSuite, r.currentTest, message, juce::String (file), line });
}

inline void checkClose (double a, double b, double tol, const juce::String& message,
                        const char* file, int line)
{
    const bool ok = std::abs (a - b) <= tol;
    check (ok, message + " (got " + juce::String (a, 6) + ", expected "
                       + juce::String (b, 6) + " +/- " + juce::String (tol, 6) + ")",
           file, line);
}

//==============================================================================
// Shared audio helpers.

inline float peakOf (const juce::AudioBuffer<float>& b)
{
    float p = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const auto* d = b.getReadPointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (std::isfinite (d[i]))
                p = juce::jmax (p, std::abs (d[i]));
    }
    return p;
}

inline double rmsOf (const juce::AudioBuffer<float>& b)
{
    double s = 0.0;
    int n = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const auto* d = b.getReadPointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (std::isfinite (d[i])) { s += (double) d[i] * d[i]; ++n; }
    }
    return n > 0 ? std::sqrt (s / (double) n) : 0.0;
}

inline bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const auto* d = b.getReadPointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (d[i]))
                return false;
    }
    return true;
}

inline bool isSilent (const juce::AudioBuffer<float>& b, float threshold = 1.0e-7f)
{
    return peakOf (b) < threshold;
}

/** Energy in a frequency band, measured by a Goertzel-style narrow sweep. */
inline double bandEnergy (const juce::AudioBuffer<float>& b, double sampleRate,
                          double loHz, double hiHz, int numBins = 24)
{
    if (b.getNumSamples() < 64)
        return 0.0;

    const int n = b.getNumSamples();
    double total = 0.0;

    for (int k = 0; k < numBins; ++k)
    {
        const double f = loHz * std::pow (hiHz / loHz, (double) k / (double) juce::jmax (1, numBins - 1));
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sampleRate;
        double re = 0.0, im = 0.0;
        for (int ch = 0; ch < juce::jmin (1, b.getNumChannels()) + 0; ++ch) {}

        const auto* d = b.getReadPointer (0);
        for (int i = 0; i < n; ++i)
        {
            const double v = std::isfinite (d[i]) ? (double) d[i] : 0.0;
            re += v * std::cos (w * (double) i);
            im += v * std::sin (w * (double) i);
        }
        total += (re * re + im * im) / ((double) n * (double) n);
    }
    return total / (double) numBins;
}

} // namespace ifxtest

//==============================================================================
#define IFX_TEST(suiteName, testName) \
    static void ifx_test_##suiteName##_##testName(); \
    static ::ifxtest::Registrar ifx_reg_##suiteName##_##testName \
        (#suiteName, #testName, ifx_test_##suiteName##_##testName); \
    static void ifx_test_##suiteName##_##testName()

#define IFX_SLOW_TEST(suiteName, testName) \
    static void ifx_test_##suiteName##_##testName(); \
    static ::ifxtest::Registrar ifx_reg_##suiteName##_##testName \
        (#suiteName, #testName, ifx_test_##suiteName##_##testName, true); \
    static void ifx_test_##suiteName##_##testName()

#define IFX_CHECK(cond)              ::ifxtest::check ((cond), #cond, __FILE__, __LINE__)
#define IFX_CHECK_MSG(cond, msg)     ::ifxtest::check ((cond), juce::String (msg), __FILE__, __LINE__)
#define IFX_CLOSE(a, b, tol)         ::ifxtest::checkClose ((a), (b), (tol), #a " ~= " #b, __FILE__, __LINE__)
#define IFX_CLOSE_MSG(a, b, tol, m)  ::ifxtest::checkClose ((a), (b), (tol), juce::String (m), __FILE__, __LINE__)
