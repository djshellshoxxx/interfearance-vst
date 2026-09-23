#pragma once

#include "Random.h"
#include "PhysicalLimits.h"
#include <cmath>
#include <algorithm>

namespace ifx
{

/** Every gesture the plugin can perform. The first five are the generic presets; the rest
    are the source-aware Machine Pattern events. The loaded source decides how a gesture acts. */
enum class GestureKind : int
{
    Wiggle = 0, SlowTurn, Tap, Shake, TouchRelease,     // generic presets (master spec 7)
    Seat, Pull,                                          // Jack (Wiggle is shared)
    Hold, Release,                                       // Touch (Tap is shared)
    Bend,                                                // Cable (Shake, Tap shared)
    TurnTo, Sweep, HoldPos,                              // Pot
    Count
};

inline const char* gestureName (GestureKind k) noexcept
{
    static const char* names[] = { "Wiggle", "Slow Turn", "Tap", "Shake", "Touch and Release",
                                   "Seat", "Pull", "Hold", "Release", "Bend", "Turn to Angle", "Sweep", "Hold Position" };
    const int i = (int) k;
    return (i >= 0 && i < (int) GestureKind::Count) ? names[i] : "?";
}

/** One gesture's output for a sample. Sources interpret the fields physically. */
struct GestureOut
{
    float displacement = 0.0f; // bipolar lateral / flex / jiggle motion
    float pressure = 0.0f;     // -1..1 added contact pressure / touch (negative = release)
    float steer = 0.0f;        // 0..1 weight pulling the position towards steerPos
    float steerPos = 0.5f;     // absolute position target
    float tapImpulse = 0.0f;   // one-sample mechanical impulse (cable tap / knock), 0 otherwise
    float envelope = 0.0f;     // overall activity 0..1 (for UI / Studio send)
    bool active = false;
};

class GestureGenerator
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        active = false;
        t = 0;
        heldUntilRelease = false;
        released = false;
        tokens = 1.0e9f;
        sinceStart = 1 << 30;
        motionLimited = false;
        lastOut = {};
    }

    /** Start a gesture. Returns false (and sets motionLimited) when it would violate the
        mechanical retrigger spacing or rate caps: the event is coalesced, not queued.
        @param durationSec  <= 0 means "until release()" (bounded to 8 s)
        @param target       absolute position target for Seat/Pull/Bend/TurnTo/Sweep, or < 0 for default
        @param basePos      the current commanded position (for relative gestures) */
    bool trigger (GestureKind k, float intensity, float durationSec, float target, float basePos,
                  float chaos, uint64_t variationSeed, Policy policy) noexcept
    {
        const float minSpacing = limit (limits::minRetriggerMs, policy) * 0.001f;
        const float rate = limit (limits::gestureRate, policy);
        tokenCap = rate;
        refill = rate / (float) fs;
        if (tokens > tokenCap) tokens = tokenCap;
        if ((float) sinceStart < minSpacing * (float) fs || tokens < 1.0f)
        {
            motionLimited = true;
            ++coalesced;
            return false;
        }
        tokens -= 1.0f;
        sinceStart = 0;
        motionLimited = false;

        rng.seed (variationSeed);
        kind = k;
        amp = std::clamp (intensity, 0.0f, 1.0f);
        const float c = std::clamp (chaos, 0.0f, 1.0f);
        rate1 = (float) (1.0 + c * 0.35 * rng.bipolar());
        rate2 = (float) (1.0 + c * 0.35 * rng.bipolar());
        phase0 = (float) (rng.uniform() * 6.2831853);
        ampJitter = (float) (1.0 + c * 0.25 * rng.bipolar());
        dir = rng.uniform() < 0.5 ? -1.0f : 1.0f;
        heldUntilRelease = durationSec <= 0.0f;
        len = heldUntilRelease ? (int) (8.0 * fs) : std::max (1, (int) (std::min (durationSec, 8.0f) * (float) fs));
        from = basePos;
        tgt = target;
        t = 0;
        released = false;
        releaseAt = -1;
        active = true;
        ++started;
        return true;
    }

    void release() noexcept
    {
        if (active && ! released)
        {
            released = true;
            releaseAt = t;
        }
    }

    GestureOut step() noexcept
    {
        ++sinceStart;
        tokens = std::min (tokenCap, tokens + refill);
        GestureOut o;
        if (! active)
        {
            lastOut = o;
            return o;
        }

        const double sec = (double) t / fs;
        const int end = releaseAt >= 0 ? std::min (len, releaseAt) : len;
        // attack / sustain / release envelope
        const double att = 0.005, rel = kind == GestureKind::TouchRelease || kind == GestureKind::Hold ? 0.08 : 0.03;
        double env = std::min (1.0, sec / att);
        if (t >= end)
        {
            const double r = (double) (t - end) / fs;
            env *= std::max (0.0, 1.0 - r / rel);
            if (r >= rel)
            {
                active = false;
                lastOut = o;
                return o;
            }
        }
        const double T = (double) len / fs;
        const double A = (double) amp * (double) ampJitter;
        const double twoPi = 6.283185307179586;

        switch (kind)
        {
            case GestureKind::Wiggle:
                o.displacement = (float) (env * A * 0.5 * std::sin (twoPi * 7.0 * rate1 * sec + phase0)
                                          * (0.8 + 0.2 * std::sin (twoPi * 1.3 * rate2 * sec)));
                break;
            case GestureKind::Shake:
                o.displacement = (float) (env * A * 0.9 * (std::sin (twoPi * 3.5 * rate1 * sec + phase0)
                                          + 0.4 * std::sin (twoPi * 7.3 * rate2 * sec)) / 1.4);
                break;
            case GestureKind::Tap:
                if (t == 0) o.tapImpulse = (float) A;
                o.displacement = (float) (A * 0.35 * std::exp (-sec / 0.015) * std::sin (twoPi * 25.0 * sec));
                o.pressure = (float) (A * std::exp (-sec / 0.04));
                if (sec > 0.12) { active = false; }
                break;
            case GestureKind::SlowTurn:
            {
                const double s = std::sin (3.141592653589793 * std::min (1.0, sec / std::max (0.05, T)));
                o.steer = 1.0f;
                o.steerPos = (float) std::clamp ((double) from + dir * 0.4 * A * s, 0.0, 1.0);
                o.pressure = (float) (0.6 * A * s);
                o.displacement = (float) (0.15 * A * s);
                if (sec >= T) active = false;
                break;
            }
            case GestureKind::TouchRelease:
            case GestureKind::Hold:
            {
                const double a2 = std::min (1.0, sec / 0.02);
                o.pressure = (float) (A * std::min (a2, env));
                o.displacement = (float) (A * 0.05 * std::sin (twoPi * 4.0 * rate1 * sec + phase0) * env);
                break;
            }
            case GestureKind::Release:
                o.pressure = (float) (-A * env);
                break;
            case GestureKind::Seat:
                o.steer = (float) env;
                o.steerPos = tgt >= 0.0f ? tgt : 1.0f;
                o.displacement = (float) (A * 0.1 * std::exp (-sec / 0.03) * std::sin (twoPi * 18.0 * sec));
                break;
            case GestureKind::Pull:
                o.steer = (float) env;
                o.steerPos = tgt >= 0.0f ? tgt : std::max (0.0f, from - 0.5f * amp);
                break;
            case GestureKind::Bend:
                o.steer = (float) env;
                o.steerPos = tgt >= 0.0f ? tgt : (float) std::clamp ((double) from + dir * 0.25 * A, 0.0, 1.0);
                o.displacement = (float) (env * A * 0.25 * std::sin (twoPi * 2.0 * rate1 * sec + phase0));
                break;
            case GestureKind::TurnTo:
                o.steer = (float) env;
                o.steerPos = tgt >= 0.0f ? tgt : (float) std::clamp ((double) from + dir * 0.3 * A, 0.0, 1.0);
                break;
            case GestureKind::Sweep:
            {
                const double s = std::sin (3.141592653589793 * std::min (1.0, sec / std::max (0.05, T)));
                const double to = tgt >= 0.0f ? (double) tgt : std::clamp ((double) from + dir * 0.5 * A, 0.0, 1.0);
                o.steer = 1.0f;
                o.steerPos = (float) ((double) from + (to - (double) from) * s);
                if (sec >= T) active = false;
                break;
            }
            case GestureKind::HoldPos:
                o.steer = (float) env;
                o.steerPos = from;
                break;
            default: break;
        }

        o.envelope = (float) env * amp;
        o.active = active;
        ++t;
        lastOut = o;
        return o;
    }

    bool isActive() const noexcept { return active; }
    GestureKind currentKind() const noexcept { return kind; }
    const GestureOut& last() const noexcept { return lastOut; }

    bool motionLimited = false;
    uint32_t coalesced = 0, started = 0;

private:
    double fs = 48000.0;
    Rng rng;
    GestureKind kind = GestureKind::Wiggle;
    bool active = false, heldUntilRelease = false, released = false;
    int t = 0, len = 1, releaseAt = -1, sinceStart = 1 << 30;
    float amp = 0, rate1 = 1, rate2 = 1, phase0 = 0, ampJitter = 1, dir = 1, from = 0.5f, tgt = -1.0f;
    float tokens = 1.0e9f, tokenCap = 1.0e9f, refill = 0.0f;
    GestureOut lastOut;
};

} // namespace ifx
