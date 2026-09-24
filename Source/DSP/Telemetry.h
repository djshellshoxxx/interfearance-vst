#pragma once

#include <atomic>
#include <cstdint>

namespace ifx
{

/** Lock-free engine -> UI state. Written by the audio thread with relaxed atomics, read by
    the UI timer. The UI never calls DSP setters from paint. */
struct SlotTelemetry
{
    std::atomic<int> type { 0 };
    std::atomic<float> position { 0 }, secondary { 0.5f }, velocity { 0 }, openness { 0 }, auxOpenness { 0 },
                       activity { 0 }, wearHere { 0 }, gesture { 0 };
    std::atomic<int> state { 0 };
    std::atomic<bool> shorted { false }, motionLimited { false }, enabled { true };
    std::atomic<uint32_t> openEvents { 0 }, resistiveEvents { 0 }, mechEvents { 0 }, quiet { 0 }, gestures { 0 };
    std::atomic<float> electricalPeak { 0 }, mechanicalPeak { 0 };
};

struct EngineTelemetry
{
    SlotTelemetry slot[2];
    // bus peaks (held by the UI, reset-on-read semantics are not needed: these are block peaks)
    std::atomic<float> inPeak { 0 }, rPeak { 0 }, dPeak { 0 }, aPeak { 0 }, cPeak { 0 }, pPeak { 0 }, wPeak { 0 }, outPeak { 0 };
    std::atomic<float> outPeakDb { -120.0f };
    std::atomic<float> stageIn[7] {}, stageOut[7] {};
    std::atomic<float> fbEffective { 0 };
    std::atomic<bool> fbLimited { false };
    std::atomic<double> ppq { 0 };
    std::atomic<float> bpm { 120 };
    std::atomic<bool> clockRunning { false }, internalClock { false }, hostTransport { false };
    std::atomic<int> laneStep[2] { {-1}, {-1} };
    std::atomic<bool> sidechainPresent { false };
    std::atomic<float> sidechainEnv { 0 };
    std::atomic<bool> excitationActive { false };
    std::atomic<bool> excitationLimited { false };      // requested excitation level above the policy cap
    std::atomic<float> excitationEffectiveDb { -120.0f }; // level actually used (after the cap)
    std::atomic<float> groundLoopDb { -120.0f };        // ground-loop hum peak after the interface (dBFS)
    std::atomic<float> tailSeconds { 0 };
    std::atomic<uint64_t> effectiveSeed { 0 };
    std::atomic<uint32_t> resets { 0 };
    std::atomic<int> numChannels { 2 };
    std::atomic<double> sampleRate { 48000 };

    // tiny event log for the data stream / debug window (single producer, lossy reader)
    struct LogEntry { uint32_t code; int32_t a; float b; };
    static constexpr int kLogSize = 256;
    LogEntry log[kLogSize] {};
    std::atomic<uint32_t> logWrite { 0 };

    void push (uint32_t code, int32_t a, float b) noexcept
    {
        const uint32_t w = logWrite.load (std::memory_order_relaxed);
        log[w % kLogSize] = { code, a, b };
        logWrite.store (w + 1, std::memory_order_release);
    }
};

enum LogCode : uint32_t
{
    LogGesture = 1,    // a = slot*100 + kind, b = intensity
    LogCoalesced,      // a = slot
    LogNoteOn,         // a = note, b = velocity
    LogNoteOff,        // a = note
    LogCC,             // a = cc, b = value
    LogLaneStep,       // a = lane*100 + step, b = intensity
    LogChoke,          // a = lane
    LogReset,          // a = reason (0 start, 1 seek, 2 loop, 3 prepare)
    LogReact,          // b = level
    LogBar,            // a = bar
};

} // namespace ifx
