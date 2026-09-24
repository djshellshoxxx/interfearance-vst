#pragma once

// Centralised engineering caps (realism spec section 2, "Calibrating numeric limits").
// Every number here is a LISTENING-TEST STARTING POINT, not a measurement. Each has a
// Natural and an Expressive column; the causal invariants are enforced in code and can
// not be switched off by any column. Every bound has a test: Tests/TestLimits.cpp, plus the
// contact / motion / retrigger caps in Tests/TestCore.cpp and the source-level caps in
// Tests/TestSources.cpp and Tests/TestEngine.cpp.
namespace ifx
{

enum class Policy { Natural = 0, Expressive = 1 };

struct Limit
{
    const char* id;
    const char* unit;
    float natural;
    float expressive;
    const char* status;
    const char* meaning;

    float get (Policy p) const noexcept { return p == Policy::Natural ? natural : expressive; }
};

namespace limits
{
    // indices into table()
    enum Id
    {
        maxEventRate = 0,     // contact state changes per second per contact
        maxOpenMs,            // longest single intermittent open (a withdrawn plug is not an event)
        maxResistiveMs,       // longest single resistive excursion
        popPeak,              // bias/charge transient peak, linear full scale
        humPeak,              // peak of mains pickup of one fully exposed path
        backgroundCeiling,    // stationary hum + interference of the whole core
        scratchGain,          // dirty-wiper modulation depth relative to the through signal
        mechanicalPeak,       // cable handling (microphonic) peak
        injectionBandwidth,   // lowpass on every injected bus, Hz
        maxMotionSpeed,       // normalised travel per second
        maxAcceleration,      // normalised travel per second^2
        excitationPeak,       // internal excitation peak
        noiseBudget,          // combined two-slot injected energy cap (linear peak sum)
        gestureRate,          // gesture starts per second per slot
        minRetriggerMs,       // ratchet / retrigger minimum spacing
        numLimits
    };
}

inline const Limit* limitTable() noexcept
{
    static const Limit table[limits::numLimits] = {
        { "maxEventRate",      "events/s", 60.0f,   150.0f,  "tunable, unmeasured", "Hard ceiling on contact state changes" },
        { "maxOpenMs",         "ms",       60.0f,   180.0f,  "tunable, unmeasured", "Longest intermittent open event" },
        { "maxResistiveMs",    "ms",       40.0f,   100.0f,  "tunable, unmeasured", "Longest resistive excursion" },
        { "popPeak",           "FS",       0.25f,   0.5f,    "tunable, unmeasured", "Charge pop peak" },
        { "humPeak",           "FS",       0.08f,   0.2f,    "tunable, unmeasured", "Mains pickup of one exposed path" },
        { "backgroundCeiling", "FS",       0.1f,    0.25f,   "tunable, unmeasured", "Stationary hum+interference of the core" },
        { "scratchGain",       "ratio",    0.35f,   0.6f,    "tunable, unmeasured", "Dirty wiper modulation depth" },
        { "mechanicalPeak",    "FS",       0.2f,    0.4f,    "tunable, unmeasured", "Cable handling microphonic peak" },
        { "injectionBandwidth","Hz",       9000.0f, 16000.0f,"tunable, unmeasured", "Lowpass on injected buses" },
        { "maxMotionSpeed",    "1/s",      6.0f,    14.0f,   "tunable, unmeasured", "Mechanical travel speed" },
        { "maxAcceleration",   "1/s^2",    400.0f,  1200.0f, "tunable, unmeasured", "Mechanical acceleration" },
        { "excitationPeak",    "FS",       0.1f,    0.3f,    "tunable, unmeasured", "Internal excitation peak" },
        { "noiseBudget",       "FS",       0.3f,    0.6f,    "tunable, unmeasured", "Combined two-slot injection" },
        { "gestureRate",       "1/s",      20.0f,   40.0f,    "tunable, unmeasured", "Gesture starts per slot" },
        { "minRetriggerMs",    "ms",       60.0f,   30.0f,   "tunable, unmeasured", "Ratchet minimum spacing" },
    };
    return table;
}

inline float limit (limits::Id id, Policy p) noexcept { return limitTable()[id].get (p); }

/** Digital safety limit (not perceptual): the numeric failsafe clamps to this. */
constexpr float kSafetyClamp = 4.0f;

} // namespace ifx
