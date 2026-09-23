#pragma once

#include "Random.h"
#include "PhysicalLimits.h"
#include <algorithm>
#include <cmath>

namespace ifx
{

/** Shared mechanical motion: turns a commanded position (knob / automation / GUI drag),
    a gesture displacement and bounded correlated micro-motion into a physical position
    q(t), signed velocity v(t) and an acceleration estimate. Travel speed and acceleration
    are clamped by PhysicalLimits, so no control can make hardware move infinitely fast. */
class MotionDriver
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        dt = 1.0 / fs;
        reset (0.5f);
    }

    void reset (float position) noexcept
    {
        pos = (double) position;
        vel = acc = 0.0;
        microTarget = microA = microB = 0.0;
        microCounter = 0;
        q = position;
        v = a = 0.0f;
        lastV = 0.0f;
    }

    void seed (uint64_t s) noexcept { rng.seed (s); }

    /** @param target     commanded position 0..1
        @param gesture    displacement added by the gesture generator (-1..1, already shaped)
        @param micro      0..1 amount of bounded correlated micro-motion (hand tremor, vibration)
        @param lo, hi     travel limits (automatic motion reverses at a boundary, never wraps) */
    void step (float target, float gesture, float micro, Policy policy, float lo = 0.0f, float hi = 1.0f) noexcept
    {
        // correlated micro-motion: new random point every 30..120 ms, double one-pole smoothing (~5 Hz)
        if (micro > 0.0f)
        {
            if (--microCounter <= 0)
            {
                microTarget = rng.bipolar();
                microCounter = (int) (rng.range (0.03, 0.12) * fs);
            }
        }
        else
            microTarget = 0.0;

        const double mc = 1.0 - std::exp (-2.0 * 3.14159265358979 * 5.0 * dt);
        microA += (microTarget - microA) * mc;
        microB += (microA - microB) * mc;

        const double desired = std::clamp ((double) target + (double) gesture + microB * (double) microDepth * (double) micro, (double) lo, (double) hi);

        // critically damped follower (~25 Hz) with speed / acceleration caps
        const double w = 2.0 * 3.14159265358979 * 25.0;
        double accel = w * w * (desired - pos) - 2.0 * w * vel;
        const double maxA = (double) limit (limits::maxAcceleration, policy);
        const double maxV = (double) limit (limits::maxMotionSpeed, policy);
        accel = std::clamp (accel, -maxA, maxA);
        vel = std::clamp (vel + accel * dt, -maxV, maxV);
        pos += vel * dt;
        if (pos < lo) { pos = lo; if (vel < 0) vel = 0; }
        if (pos > hi) { pos = hi; if (vel > 0) vel = 0; }

        q = (float) pos;
        v = (float) vel;
        a = (float) ((vel - (double) lastV) * fs);
        lastV = (float) vel;
    }

    float microDepth = 0.035f; // travel covered by full micro-motion
    float q = 0.5f;   // position
    float v = 0.0f;   // velocity (1/s)
    float a = 0.0f;   // acceleration (1/s^2)

private:
    double fs = 48000.0, dt = 1.0 / 48000.0;
    double pos = 0.5, vel = 0.0, acc = 0.0;
    double microTarget = 0.0, microA = 0.0, microB = 0.0;
    int microCounter = 0;
    float lastV = 0.0f;
    Rng rng;
};

} // namespace ifx
