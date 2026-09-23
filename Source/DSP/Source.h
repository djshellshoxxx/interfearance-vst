#pragma once

#include "../Core/Random.h"
#include "../Core/PhysicalLimits.h"
#include "../Core/Params.h"
#include "../Core/Gesture.h"
#include "../Core/Environment.h"
#include "../Core/WearMap.h"
#include "../Core/ContactModel.h"
#include "../Core/MotionDriver.h"
#include "Filters.h"
#include <atomic>

namespace ifx
{

enum class SourceType : int { None = 0, LooseJack = 1, PlugTouch = 2, CableHandling = 3, DirtyPot = 4 };
constexpr int kNumSourceTypes = 4;

inline const char* sourceName (SourceType t) noexcept
{
    switch (t)
    {
        case SourceType::LooseJack:     return "Loose Jack";
        case SourceType::PlugTouch:     return "Exposed Plug Touch";
        case SourceType::CableHandling: return "Cable Handling";
        case SourceType::DirtyPot:      return "Dirty Pot";
        default:                        return "Empty";
    }
}

inline const char* sourceCause (SourceType t) noexcept
{
    switch (t)
    {
        case SourceType::LooseJack:     return "A TS plug moving in a worn jack: the tip contact opens, turns resistive or shorts.";
        case SourceType::PlugTouch:     return "A finger on an exposed high-impedance tip couples mains hum into the input.";
        case SourceType::CableHandling: return "Flexing a cable over broken strands or braid; handling noise only through a microphonic path.";
        case SourceType::DirtyPot:      return "A dirty wiper crossing a worn arc of the resistive track while the knob turns.";
        default:                        return "Load a source into this slot.";
    }
}

/** The three source-specific gestures used by Machine Pattern steps. */
inline GestureKind patternGesture (SourceType t, int index) noexcept
{
    index = std::clamp (index, 0, 2);
    static const GestureKind jack[3]  = { GestureKind::Wiggle, GestureKind::Seat, GestureKind::Pull };
    static const GestureKind touch[3] = { GestureKind::Tap, GestureKind::Hold, GestureKind::Release };
    static const GestureKind cable[3] = { GestureKind::Bend, GestureKind::Shake, GestureKind::Tap };
    static const GestureKind pot[3]   = { GestureKind::TurnTo, GestureKind::Sweep, GestureKind::HoldPos };
    switch (t)
    {
        case SourceType::LooseJack:     return jack[index];
        case SourceType::PlugTouch:     return touch[index];
        case SourceType::CableHandling: return cable[index];
        case SourceType::DirtyPot:      return pot[index];
        default:                        return GestureKind::Wiggle;
    }
}

/** Seed of a source's persistent fault map, from the user Seed only. */
inline uint64_t mapSeedFor (uint64_t userSeed, SourceType t) noexcept { return hash3 (userSeed, (uint64_t) t, 0x3A9ull); }

/** The fault-map settings of each source: shared by the DSP and the illustrations, so the
    worn arc / weak spot drawn on screen is exactly the one the engine uses. */
inline WearMap::Settings wearSettingsFor (SourceType t, uint64_t mapSeed, const ParamSnapshot& p) noexcept
{
    WearMap::Settings ws;
    switch (t)
    {
        case SourceType::LooseJack:
            ws.seed = hashCombine (mapSeed, 0x3A); ws.wear = p[P::jkWear]; ws.centre = p[P::jkWearPos]; ws.width = 0.1f; ws.background = 0.08f; break;
        case SourceType::CableHandling:
            ws.seed = hashCombine (mapSeed, 0x4A); ws.wear = 1.0f; ws.centre = p[P::cbWeakPos]; ws.width = 0.07f; ws.background = 0.04f; break;
        case SourceType::DirtyPot:
            ws.seed = hashCombine (mapSeed, 0x5A); ws.wear = p[P::ptDirt]; ws.centre = p[P::ptWearPos]; ws.width = p[P::ptWearWidth]; ws.background = 0.06f; break;
        default:
            ws.wear = 0.0f; break;
    }
    return ws;
}

/** Why a source may be quiet (realism spec 7): bit flags, highest priority first. */
enum QuietReason : uint32_t
{
    QuietNone        = 0,
    QuietStationary  = 1u << 0, // "Pot is stationary" / no movement
    QuietHealthy     = 1u << 1, // "Contact stable here"
    QuietShielded    = 1u << 2, // "Circuit is shielded"
    QuietMicOff      = 1u << 3, // "Cable mic is off"
    QuietNoInput     = 1u << 4, // "No input reaches this open contact"
    QuietHumOff      = 1u << 5, // environment has no hum
    QuietNotTouched  = 1u << 6,
    QuietBypassed    = 1u << 7,
};

/** Per-sample shared context. */
struct SampleCtx
{
    Environment::Sample env;
    float hum = 0.0f, buzz = 0.0f, rf = 0.0f; // environment strengths 0..1
    float stereo = 0.0f;
    Policy policy = Policy::Natural;
    float inputLevel = 0.0f;                   // slow |p| estimate for quiet reasons
};

/** Lock-free telemetry copied to the UI snapshot. Written only by the audio thread. */
struct SourceTelemetry
{
    float position = 0.0f;   // mechanical position q
    float secondary = 0.5f;  // lateral / finger / flex displacement shown by the illustration
    float velocity = 0.0f;
    float openness = 0.0f;   // 1 - conductance of the main contact
    float auxOpenness = 0.0f;// shield / sleeve openness where relevant
    float activity = 0.0f;   // injected-noise envelope
    float mechActivity = 0.0f; // mechanical (microphonic) envelope, cable only
    float wearHere = 0.0f;   // wear map value at the current position
    int state = 0;           // ContactModel::State of the main contact
    bool shorted = false;
    uint32_t openEvents = 0, resistiveEvents = 0, mechEvents = 0;
    uint32_t quiet = 0;
    float effective[4] {};   // effective (clamped) values of the primary controls
    bool limited = false;    // a control asked for more than the circuit can produce
};

class Source
{
public:
    virtual ~Source() = default;
    virtual SourceType type() const noexcept = 0;

    virtual void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        for (auto& l : injLp) l.setCutoff (9000.0, fs);
        primed = false;
    }
    virtual void reset() noexcept { primed = false; for (auto& l : injLp) l.reset(); activityEnv = 0.0f; }
    virtual void seed (uint64_t s) noexcept = 0;

    /** Once per block, from the parameter snapshot. */
    virtual void setParams (const ParamSnapshot& p, float disturbance) noexcept = 0;

    /** Once per sample: advance motion and contacts. */
    virtual void control (const GestureOut& g, const SampleCtx& ctx) noexcept = 0;

    /** Commanded (parameter) position, the base for relative gestures. */
    virtual float basePosition() const noexcept = 0;

    /** Environmental pickup coupling (0..1) this sample, before the shared noise budget. */
    virtual float pickupCoupling() const noexcept { return 0.0f; }

    /** Process the three counterfactual paths in place: [0] = R healthy reference,
        [1] = C0 actual contacts without injection, [2] = C complete. `envScale` is the
        shared noise-budget scale for environmental pickup. */
    virtual void process (float (&paths)[3][2], int numChannels, const SampleCtx& ctx, float envScale) noexcept = 0;

    SourceTelemetry tele;

    /** Seed of the persistent wear / weak-spot map. Derived from the user Seed only (never
        from the take or the bar-keyed random streams) so the fault map stays put. */
    uint64_t mapSeed = 1;

protected:
    void setBandwidth (Policy p) noexcept
    {
        const float bw = limit (limits::injectionBandwidth, p);
        if (bw != lastBw)
        {
            lastBw = bw;
            for (auto& l : injLp) l.setCutoff (bw, fs);
        }
    }

    float envWave (const SampleCtx& c, float bright, int ch) const noexcept
    {
        float w = c.env.humClean * (1.0f - bright) + c.env.humBright * bright;
        if (ch == 1)
            w = w * (1.0f + 0.12f * c.stereo) + 0.06f * c.stereo * (c.env.humBright - c.env.humClean);
        return c.hum * w + c.buzz * c.env.buzz * 0.8f + c.rf * c.env.rf * 0.7f;
    }

    void trackActivity (float inj) noexcept
    {
        const float a = std::abs (inj);
        activityEnv = a > activityEnv ? a : activityEnv * 0.9995f;
        tele.activity = activityEnv;
    }

    static float smoothstep (float e0, float e1, float x) noexcept
    {
        const float t = std::clamp ((x - e0) / (e1 - e0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    double fs = 48000.0;
    bool primed = false;
    OnePole injLp[2];
    float lastBw = -1.0f;
    float activityEnv = 0.0f;
    Policy policy = Policy::Natural;
};

} // namespace ifx
