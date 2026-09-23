#pragma once

#include <JuceHeader.h>
#include "Plugin/StateIO.h"
#include "DSP/Engine.h"

namespace ifx
{

/** Offline render of the internal excitation / gesture path with a CLONED engine
    (routing spec 5): x = 0, s = 0, fixed tempo, same route / seed / pattern as the state.
    Never touches the live engine, never runs on the audio thread. */
struct RenderSettings
{
    bool useBars = true;
    int bars = 2;               // 1..16
    double seconds = 2.0;       // 0.1..30
    double bpm = 120.0;         // fixed render tempo
    int beatsPerBar = 4;
    double sampleRate = 48000.0; // 44100 / 48000 / 96000
    int bitDepth = 24;          // 16 / 24 / 32 (float)
    int channels = 2;           // 1 / 2
    double tailSeconds = 0.0;   // 0..10: extra time after the end with no new gestures
    bool normalize = false;
    float headroomDb = 1.0f;    // peak after normalisation = -headroom dBFS
    Tap tap = Tap::FullOutput;
    bool triggerAtStart = true; // with the lane off, play both slots' gestures at 0
    bool writeSidecar = false;  // optional privacy-safe .txt with seed / tempo / sources

    double bodySeconds() const noexcept { return useBars ? bars * beatsPerBar * 60.0 / bpm : seconds; }
    void sanitise() noexcept;
};

struct RenderResult
{
    bool ok = false;
    bool cancelled = false;
    bool silent = false;
    juce::String silentReason;   // plain-language explanation when silent
    float peak = 0.0f;
    float appliedGainDb = 0.0f;
    double seconds = 0.0;
    bool tailIncluded = false;
    juce::String error;
    juce::AudioBuffer<float> audio;
};

namespace OfflineRenderer
{
    /** Render into memory. `cancel` may be set from another thread; `progress` (0..1) is
        written as rendering proceeds. */
    RenderResult render (const PluginState& state, RenderSettings settings,
                         const std::atomic<bool>* cancel = nullptr, std::atomic<float>* progress = nullptr);

    /** Why a render is silent, from the state's actual dependencies. */
    juce::String explainSilence (const PluginState& state);

    /** Predictable file name: Interference_<preset>_<seed>_<length>_<bpm>bpm.wav */
    juce::String suggestedFileName (const PluginState& state, const RenderSettings& settings);

    /** Write a rendered buffer as WAV. Refuses to overwrite unless `allowOverwrite`. */
    bool writeWav (const juce::AudioBuffer<float>& audio, const RenderSettings& settings, const juce::File& file,
                   bool allowOverwrite, juce::String& error);

    /** Optional sidecar (only on request): seed, take, tempo, sources, policy. No paths. */
    bool writeSidecar (const PluginState& state, const RenderSettings& settings, const juce::File& wavFile, juce::String& error);

    /** Human-readable quality string, e.g. "48 kHz / 24-bit PCM / stereo". */
    juce::String describeFormat (const RenderSettings& settings);
}

/** Live record of the output (or a chosen tap) into a WAV. The audio thread writes into a
    preallocated FIFO inside JUCE's ThreadedWriter (no allocation, no file IO, no locks on
    the audio thread); a background thread drains it to disk. */
class LiveRecorder
{
public:
    LiveRecorder();
    ~LiveRecorder();

    /** Message thread. Refuses to overwrite unless `allowOverwrite`. */
    bool start (const juce::File& file, double sampleRate, int channels, int bitDepth, bool allowOverwrite, juce::String& error);
    /** Message thread. Returns the recorded length in samples. */
    juce::int64 stop();

    bool isRecording() const noexcept { return active.load() != nullptr; }
    juce::File getFile() const { return currentFile; }
    juce::int64 samplesWritten() const noexcept { return written.load(); }
    juce::int64 samplesDropped() const noexcept { return dropped.load(); }

    /** Audio thread. */
    void push (const float* const* data, int numChannels, int numSamples) noexcept;

private:
    juce::TimeSliceThread thread { "Interference recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> active { nullptr };
    std::atomic<int> users { 0 };
    std::atomic<juce::int64> written { 0 }, dropped { 0 };
    int recChannels = 2;
    juce::File currentFile;
};

} // namespace ifx
