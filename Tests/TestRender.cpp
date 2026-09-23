// Offline render, WAV export and live record (master spec 7, routing spec 5; WP7).
#include "TestHelpers.h"
#include "Render/OfflineRenderer.h"
#include "Plugin/FactoryPresets.h"
#include <thread>

using namespace ifx;
using namespace ifxtest;

namespace
{
PluginState noisyState()
{
    PluginState s;
    s.slotA = SourceType::PlugTouch;
    s.slotB = SourceType::DirtyPot;
    s.params[P::mode] = 2.0f;
    s.params[P::excType] = 1.0f;
    s.params[P::excLevel] = -24.0f;
    s.params[P::hum] = 0.5f;
    s.params[P::tcExposure] = 0.9f;
    s.params[P::gestureA] = 4.0f;
    s.params[P::gestureB] = 1.0f;
    s.params[P::ptDirt] = 0.8f;
    return s;
}

juce::File tempDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::tempDirectory)
                 .getChildFile ("InterferenceRender_" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    d.createDirectory();
    return d;
}

struct WavInfo { double rate = 0; int bits = 0; int channels = 0; juce::int64 length = 0; bool isFloat = false; float peak = 0; };

WavInfo readWav (const juce::File& f)
{
    WavInfo w;
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatReader> r (fmt.createReaderFor (f.createInputStream().release(), true));
    if (r == nullptr) return w;
    w.rate = r->sampleRate;
    w.bits = (int) r->bitsPerSample;
    w.channels = (int) r->numChannels;
    w.length = r->lengthInSamples;
    w.isFloat = r->usesFloatingPointData;
    juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
    w.peak = b.getMagnitude (0, b.getNumSamples());
    return w;
}
}

IFX_TEST (Render, durationFollowsBarsAndSeconds)
{
    RenderSettings rs;
    rs.useBars = true;
    rs.bars = 2;
    rs.bpm = 120.0;
    rs.sampleRate = 44100.0;
    auto r = OfflineRenderer::render (noisyState(), rs);
    IFX_CHECK (r.ok && ! r.silent);
    IFX_CHECK (r.audio.getNumSamples() == 4 * 44100); // 2 bars of 4/4 at 120 BPM = 4 s
    rs.useBars = false;
    rs.seconds = 0.1;
    rs.sampleRate = 96000.0;
    rs.tailSeconds = 0.5;
    r = OfflineRenderer::render (noisyState(), rs);
    IFX_CHECK (r.audio.getNumSamples() == 9600 + 48000);
    IFX_CHECK (r.tailIncluded);
    rs.seconds = 1000.0; // clamped to 30 s
    rs.tailSeconds = 0.0;
    rs.sampleRate = 12345.0; // illegal -> 48 kHz
    rs.sanitise();
    IFX_CHECK (rs.seconds == 30.0 && rs.sampleRate == 48000.0);
}

IFX_TEST (Render, isDeterministicAndIndependentOfLiveEngine)
{
    RenderSettings rs;
    rs.useBars = false;
    rs.seconds = 1.0;
    auto a = OfflineRenderer::render (noisyState(), rs);
    auto b = OfflineRenderer::render (noisyState(), rs);
    IFX_CHECK (maxAbsDiff (a.audio, b.audio) == 0.0);
    auto s2 = noisyState();
    s2.take = 1; // New Take: same settings, different micro-events
    auto c = OfflineRenderer::render (s2, rs);
    IFX_CHECK (maxAbsDiff (a.audio, c.audio) > 0.0);
}

IFX_TEST (Render, silentResultIsDetectedAndExplained)
{
    PluginState s; // Init: jack seated, no excitation
    s.params[P::hum] = 0.0f;
    RenderSettings rs;
    rs.useBars = false;
    rs.seconds = 0.5;
    auto r = OfflineRenderer::render (s, rs);
    IFX_CHECK (r.ok && r.silent);
    IFX_CHECK (r.silentReason.contains ("Excitation is off"));
}

IFX_TEST (Render, normalisationHitsHeadroom)
{
    RenderSettings rs;
    rs.useBars = false;
    rs.seconds = 1.0;
    rs.normalize = true;
    rs.headroomDb = 3.0f;
    auto r = OfflineRenderer::render (noisyState(), rs);
    IFX_CLOSE (r.audio.getMagnitude (0, r.audio.getNumSamples()), juce::Decibels::decibelsToGain (-3.0f), 1e-4f);
}

IFX_TEST (Render, faultDifferenceTapKeepsSignAndMonoWorks)
{
    auto s = noisyState();
    s.slotA = SourceType::LooseJack;
    s.params[P::jkInsert] = 0.75f;
    s.params[P::jkWear] = 1.0f;
    s.params[P::jkWiggle] = 1.0f;
    s.params[P::excType] = 4.0f; // test tone
    RenderSettings rs;
    rs.useBars = false;
    rs.seconds = 1.0;
    rs.channels = 1;
    rs.tap = Tap::FaultDifference;
    auto r = OfflineRenderer::render (s, rs);
    IFX_CHECK (r.audio.getNumChannels() == 1);
    float mn = 0, mx = 0;
    for (int i = 0; i < r.audio.getNumSamples(); ++i) { mn = std::min (mn, r.audio.getSample (0, i)); mx = std::max (mx, r.audio.getSample (0, i)); }
    IFX_CHECK (mn < 0.0f && mx > 0.0f); // D is signed: not rectified
}

IFX_TEST (Render, wavHeadersMatchEveryFormat)
{
    const auto dir = tempDir();
    int k = 0;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (int bits : { 16, 24, 32 })
            for (int ch : { 1, 2 })
            {
                RenderSettings rs;
                rs.useBars = false;
                rs.seconds = 0.25;
                rs.sampleRate = rate;
                rs.bitDepth = bits;
                rs.channels = ch;
                auto r = OfflineRenderer::render (noisyState(), rs);
                const auto f = dir.getChildFile ("t" + juce::String (k++) + ".wav");
                juce::String err;
                IFX_CHECK_MSG (OfflineRenderer::writeWav (r.audio, rs, f, false, err), err);
                const auto w = readWav (f);
                IFX_CHECK (w.rate == rate);
                IFX_CHECK (w.bits == bits);
                IFX_CHECK (w.channels == ch);
                IFX_CHECK (w.length == (juce::int64) std::llround (0.25 * rate));
                IFX_CHECK (w.isFloat == (bits == 32));
                IFX_CHECK (w.peak > 0.0f);
            }
    dir.deleteRecursively();
}

IFX_TEST (Render, neverOverwritesWithoutConfirmation)
{
    const auto dir = tempDir();
    const auto f = dir.getChildFile ("exists.wav");
    f.replaceWithText ("precious");
    RenderSettings rs;
    rs.useBars = false;
    rs.seconds = 0.1;
    auto r = OfflineRenderer::render (noisyState(), rs);
    juce::String err;
    IFX_CHECK (! OfflineRenderer::writeWav (r.audio, rs, f, false, err));
    IFX_CHECK (f.loadFileAsString() == "precious");
    IFX_CHECK (OfflineRenderer::writeWav (r.audio, rs, f, true, err));
    IFX_CHECK (readWav (f).length == 4800);
    dir.deleteRecursively();
}

IFX_TEST (Render, badPathsFailCleanly)
{
    RenderSettings rs;
    juce::AudioBuffer<float> empty;
    juce::String err;
    IFX_CHECK (! OfflineRenderer::writeWav (empty, rs, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("x.wav"), true, err));
    juce::AudioBuffer<float> one (2, 10);
    one.clear();
    // a path whose parent is an existing FILE cannot be created
    const auto dir = tempDir();
    const auto blocker = dir.getChildFile ("blocker");
    blocker.replaceWithText ("x");
    IFX_CHECK (! OfflineRenderer::writeWav (one, rs, blocker.getChildFile ("sub").getChildFile ("a.wav"), true, err));
    IFX_CHECK (err.isNotEmpty());
    dir.deleteRecursively();
}

IFX_TEST (Render, cancellationStopsEarly)
{
    std::atomic<bool> cancel { true };
    std::atomic<float> progress { 0.0f };
    RenderSettings rs;
    rs.useBars = false;
    rs.seconds = 30.0;
    auto r = OfflineRenderer::render (noisyState(), rs, &cancel, &progress);
    IFX_CHECK (r.cancelled && ! r.ok && r.audio.getNumSamples() == 0);
}

IFX_TEST (Render, fileNameIsPredictableAndSafe)
{
    auto s = noisyState();
    s.presetName = "My/Preset:*?";
    RenderSettings rs;
    const auto n = OfflineRenderer::suggestedFileName (s, rs);
    IFX_CHECK (n.startsWith ("Interference_MyPreset_"));
    IFX_CHECK (n.endsWith ("_2bar_120bpm.wav"));
    IFX_CHECK (! n.containsAnyOf ("/\\:*?\"<>|"));
}

IFX_TEST (Render, sidecarOnlyOnRequestAndNoPaths)
{
    const auto dir = tempDir();
    const auto wav = dir.getChildFile ("a.wav");
    RenderSettings rs;
    juce::String err;
    IFX_CHECK (OfflineRenderer::writeSidecar (noisyState(), rs, wav, err));
    const auto txt = wav.withFileExtension ("txt").loadFileAsString();
    IFX_CHECK (txt.contains ("Seed:") && txt.contains ("Tempo:"));
    IFX_CHECK (! txt.contains (dir.getFullPathName()));
    dir.deleteRecursively();
}

IFX_TEST (Record, liveRecorderWritesWhatItIsFed)
{
    const auto dir = tempDir();
    const auto f = dir.getChildFile ("rec.wav");
    LiveRecorder rec;
    juce::String err;
    IFX_CHECK (rec.start (f, 48000.0, 2, 24, false, err));
    auto sig = sine (2, 48000, 440.0, 48000.0);
    for (int pos = 0; pos < 48000; pos += 480)
    {
        const float* p[2] = { sig.getReadPointer (0, pos), sig.getReadPointer (1, pos) };
        rec.push (p, 2, 480);
    }
    IFX_CHECK (rec.stop() == 48000);
    IFX_CHECK (! rec.isRecording());
    const auto w = readWav (f);
    IFX_CHECK (w.length == 48000 && w.channels == 2);
    IFX_CLOSE (w.peak, 0.5f, 1e-3f);
    // refuses to overwrite
    IFX_CHECK (! rec.start (f, 48000.0, 2, 24, false, err));
    dir.deleteRecursively();
}

IFX_TEST (Record, teardownWhileAudioThreadIsPushing)
{
    const auto dir = tempDir();
    for (int rep = 0; rep < 5; ++rep)
    {
        auto rec = std::make_unique<LiveRecorder>();
        juce::String err;
        IFX_CHECK (rec->start (dir.getChildFile ("t" + juce::String (rep) + ".wav"), 48000.0, 2, 16, true, err));
        std::atomic<bool> run { true };
        std::atomic<LiveRecorder*> target { rec.get() };
        std::thread audio ([&]
        {
            float buf[256] {};
            const float* p[2] = { buf, buf };
            while (run.load())
                if (auto* r = target.load()) r->push (p, 2, 256);
        });
        juce::Thread::sleep (20);
        rec->stop();          // stop while pushes are in flight
        run.store (false);
        audio.join();
        target.store (nullptr);
        rec.reset();
    }
    IFX_CHECK (true);
    dir.deleteRecursively();
}
