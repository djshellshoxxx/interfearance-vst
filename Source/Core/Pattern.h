#pragma once

#include "Random.h"
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <cstring>

namespace ifx
{

constexpr int kMaxSteps = 64;
constexpr int kNumLanes = 2; // lane 0 drives slot A, lane 1 drives slot B

/** One Machine Pattern step. Plain data, bounded, versioned in the state. */
struct LaneStep
{
    uint8_t active = 0;
    uint8_t gesture = 3;      // 0..2 = the loaded source's three pattern gestures, 3 = the slot's preset gesture
    uint8_t accent = 0;
    uint8_t ratchet = 1;      // 1..4 retriggers inside the step
    uint8_t choke = 0;        // Studio choke on this step
    float intensity = 0.7f;   // 0..1
    float duration = 1.0f;    // in steps, 0.1..8
    float target = -1.0f;     // absolute position target, < 0 = gesture default
    float send = 0.5f;        // Studio send 0..1 (0.5 = unity)
    float probability = 1.0f; // 0..1
    float offset = 0.0f;      // microtiming, fraction of a step -0.5..0.5

    // field-wise: memcmp would also compare indeterminate padding bytes
    bool operator== (const LaneStep& o) const noexcept
    {
        return active == o.active && gesture == o.gesture && accent == o.accent && ratchet == o.ratchet && choke == o.choke
            && intensity == o.intensity && duration == o.duration && target == o.target && send == o.send
            && probability == o.probability && offset == o.offset;
    }
    bool operator!= (const LaneStep& o) const noexcept { return ! (*this == o); }
};

struct Lane
{
    int length = 16;          // 1..64
    uint8_t mute = 0, solo = 0;
    LaneStep steps[kMaxSteps];

    bool operator== (const Lane& o) const noexcept
    {
        if (length != o.length || mute != o.mute || solo != o.solo) return false;
        for (int i = 0; i < kMaxSteps; ++i)
            if (steps[i] != o.steps[i]) return false;
        return true;
    }
};

struct Pattern
{
    Lane lanes[kNumLanes];

    void clear() noexcept
    {
        for (auto& l : lanes)
        {
            l = Lane();
        }
    }

    /** Shift / rotate / clear helpers for the editor (message thread only). */
    void rotate (int lane, int by) noexcept
    {
        auto& l = lanes[lane];
        const int n = std::clamp (l.length, 1, kMaxSteps);
        LaneStep tmp[kMaxSteps];
        for (int i = 0; i < n; ++i)
            tmp[((i + by) % n + n) % n] = l.steps[i];
        for (int i = 0; i < n; ++i)
            l.steps[i] = tmp[i];
    }

    void shift (int lane, int by) noexcept
    {
        auto& l = lanes[lane];
        const int n = std::clamp (l.length, 1, kMaxSteps);
        LaneStep tmp[kMaxSteps];
        for (int i = 0; i < n; ++i)
            tmp[i] = (i - by >= 0 && i - by < n) ? l.steps[i - by] : LaneStep();
        for (int i = 0; i < n; ++i)
            l.steps[i] = tmp[i];
    }

    bool operator== (const Pattern& o) const noexcept
    {
        for (int i = 0; i < kNumLanes; ++i)
            if (! (lanes[i] == o.lanes[i])) return false;
        return true;
    }
    bool operator!= (const Pattern& o) const noexcept { return ! (*this == o); }
};

/** Single-writer (message thread) / single-reader (audio thread) exchange of the pattern.
    Seqlock: the audio thread copies only a consistent even version and never waits. */
class PatternExchange
{
public:
    void publish (const Pattern& p) noexcept
    {
        version.fetch_add (1, std::memory_order_acq_rel); // odd: writing
        std::memcpy (&shared, &p, sizeof (Pattern));
        version.fetch_add (1, std::memory_order_acq_rel); // even: done
    }

    /** Returns true if `dest` was updated to a newer consistent pattern. */
    bool fetch (Pattern& dest, uint32_t& lastVersion) noexcept
    {
        const uint32_t v1 = version.load (std::memory_order_acquire);
        if (v1 == lastVersion || (v1 & 1u))
            return false;
        std::memcpy (&scratch, &shared, sizeof (Pattern));
        std::atomic_thread_fence (std::memory_order_acquire);
        const uint32_t v2 = version.load (std::memory_order_acquire);
        if (v1 != v2)
            return false;
        std::memcpy (&dest, &scratch, sizeof (Pattern));
        lastVersion = v1;
        return true;
    }

private:
    std::atomic<uint32_t> version { 0 };
    Pattern shared;
    Pattern scratch;
};

/** Beats per lane step for the division choice (1/4 .. 1/32T). */
inline double divisionBeats (int choice) noexcept
{
    static const double d[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0 };
    return d[std::clamp (choice, 0, 6)];
}

/** A scheduled lane event (produced by the scheduler for one block). */
struct LaneEvent
{
    int sampleOffset = 0;
    int lane = 0;
    int stepIndex = 0;        // index inside the lane
    int64_t globalStep = 0;   // absolute step count on the musical timeline
    int ratchetIndex = 0;
    LaneStep step;
    float durationSec = 0.1f;
    float intensity = 0.7f;
    float send = 1.0f;        // studio send gain
    bool isChokeRelease = false;
};

struct SchedulerSettings
{
    uint64_t seed = 1;
    double stepBeats = 0.25;
    double beatsPerBar = 4.0;
    float swing = 0.0f, humanize = 0.0f, density = 1.0f, evolve = 0.0f, patternLevel = 1.0f, studioSend = 1.0f;
    bool fill = false;
};

/** Pure function of (pattern, settings, ppq window): the same musical position always
    produces the same events, which is what makes seek / loop / bounce deterministic.
    Writes at most `maxEvents` events, sorted by sample offset, and returns the count.
    Mechanical spacing limits are applied later by the gesture generator (coalescing). */
inline int scheduleLane (const Pattern& pat, const SchedulerSettings& s, double ppqStart, int numSamples,
                         double samplesPerBeat, double sampleRate, LaneEvent* out, int maxEvents) noexcept
{
    int count = 0;
    if (numSamples <= 0 || s.stepBeats <= 0.0 || samplesPerBeat <= 0.0)
        return 0;
    // Work on integer sample indices of the musical timeline so that the block partition
    // can never move an event by a sample.
    const int64_t blockStart = (int64_t) std::llround (ppqStart * samplesPerBeat);
    const int64_t blockEnd = blockStart + numSamples;
    const double ppqEnd = (double) blockEnd / samplesPerBeat;

    bool anySolo = false;
    for (const auto& l : pat.lanes) anySolo |= l.solo != 0;

    const double L = s.stepBeats;
    const int64_t k0 = (int64_t) std::floor ((double) blockStart / samplesPerBeat / L) - 1;
    const int64_t k1 = (int64_t) std::floor (ppqEnd / L) + 1;

    for (int li = 0; li < kNumLanes; ++li)
    {
        const auto& lane = pat.lanes[li];
        if (lane.mute || (anySolo && ! lane.solo))
            continue;
        const int n = std::clamp (lane.length, 1, kMaxSteps);

        for (int64_t k = k0; k <= k1; ++k)
        {
            if (k < 0) continue;
            const int idx = (int) (k % n);
            const int64_t cycle = k / n;
            const LaneStep& st = lane.steps[idx];
            const bool fillStep = s.fill && ! st.active && (k % 2 == 1);
            if (! st.active && ! fillStep)
                continue;

            const double bar = std::floor ((double) k * L / s.beatsPerBar);
            const uint64_t h = hash4 (s.seed, (uint64_t) li, (uint64_t) k, 0x57E9ull);
            // probability: seeded per absolute step (cycle + lane + step index)
            float prob = fillStep ? 0.55f : st.probability;
            prob *= s.density;
            if (s.evolve > 0.0f)
            {
                const double ev = hashToUnit (hash3 (s.seed, (uint64_t) bar, (uint64_t) (li * 131 + idx))) * 2.0 - 1.0;
                prob = std::clamp (prob + (float) ev * 0.5f * s.evolve, 0.0f, 1.0f);
            }
            if (s.fill && st.active) prob = std::min (1.0f, prob + 0.3f);
            if (hashToUnit (h) >= prob)
                continue;

            // timing: swing on offbeat steps, seeded humanize, programmed microtiming
            double t = (double) k * L;
            if (k % 2 == 1) t += (double) s.swing * L;
            t += (double) std::clamp (st.offset, -0.5f, 0.5f) * L;
            if (s.humanize > 0.0f)
                t += (hashToUnit (hash3 (h, 0xB0Bull, 1)) * 2.0 - 1.0) * 0.25 * (double) s.humanize * L;

            int ratchets = fillStep ? 1 : std::clamp ((int) st.ratchet, 1, 4);
            if (s.fill && st.active && ratchets == 1 && hashToUnit (hash3 (h, 0xF111ull, 2)) < 0.4)
                ratchets = 2;
            const double sub = L / (double) ratchets;

            for (int r = 0; r < ratchets; ++r)
            {
                const double te = t + r * sub;
                const int64_t es = (int64_t) std::llround (te * samplesPerBeat);
                if (es < blockStart || es >= blockEnd)
                    continue;
                if (count >= maxEvents)
                    return count;
                LaneEvent e;
                e.sampleOffset = (int) (es - blockStart);
                e.lane = li;
                e.stepIndex = idx;
                e.globalStep = k;
                e.ratchetIndex = r;
                e.step = st;
                if (fillStep)
                {
                    e.step.active = 1;
                    e.step.gesture = 3;
                    e.step.intensity = 0.45f;
                    e.step.duration = 0.5f;
                }
                const float acc = e.step.accent ? 1.35f : 1.0f;
                e.intensity = std::clamp (e.step.intensity * acc * s.patternLevel, 0.0f, 1.0f);
                e.durationSec = (float) (std::clamp (e.step.duration, 0.1f, 8.0f) * L / (double) ratchets * samplesPerBeat / sampleRate);
                e.send = std::clamp (2.0f * e.step.send * s.studioSend * (e.step.accent ? 1.25f : 1.0f), 0.0f, 2.5f);
                out[count++] = e;
            }
            (void) cycle;
        }
    }

    // insertion sort by sample offset (small bounded arrays, no allocation)
    for (int i = 1; i < count; ++i)
    {
        LaneEvent e = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].sampleOffset > e.sampleOffset) { out[j + 1] = out[j]; --j; }
        out[j + 1] = e;
    }
    return count;
}

} // namespace ifx
