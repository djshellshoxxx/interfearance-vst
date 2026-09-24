#include "OfflineRenderer.h"

namespace ifx
{

void RenderSettings::sanitise() noexcept
{
    bars = std::clamp (bars, 1, 16);
    seconds = std::clamp (std::isfinite (seconds) ? seconds : 2.0, 0.1, 30.0);
    bpm = std::clamp (std::isfinite (bpm) ? bpm : 120.0, 20.0, 400.0);
    beatsPerBar = std::clamp (beatsPerBar, 1, 16);
    if (sampleRate != 44100.0 && sampleRate != 48000.0 && sampleRate != 96000.0)
        sampleRate = 48000.0;
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
        bitDepth = 24;
    channels = std::clamp (channels, 1, 2);
    tailSeconds = std::clamp (std::isfinite (tailSeconds) ? tailSeconds : 0.0, 0.0, 10.0);
    headroomDb = std::clamp (std::isfinite (headroomDb) ? headroomDb : 1.0f, 0.0f, 24.0f);
}

namespace OfflineRenderer
{

juce::String explainSilence (const PluginState& s)
{
    juce::StringArray why;
    const auto& p = s.params;
    const bool exc = p.choice (P::excType) != 0 && (! p.on (P::rawHw) || p.on (P::keepExc));
    if (! exc)
        why.add ("Excitation is off: an offline render has no input audio, so only injected artifacts (hum, pops, handling) can be heard.");
    const bool anySource = (s.slotA != SourceType::None && p.on (P::slotAOn)) || (s.slotB != SourceType::None && p.on (P::slotBOn));
    if (! anySource)
        why.add ("No source is loaded and enabled.");
    const bool env = p[P::hum] > 0.0f || p[P::buzz] > 0.0f || p[P::rf] > 0.0f;
    const bool pops = p[P::jkPop] > 0.0f || p[P::tcPop] > 0.0f || p[P::ptBias] > 0.0f;
    const bool mic = p[P::cbMic] > 0.0f && (s.slotA == SourceType::CableHandling || s.slotB == SourceType::CableHandling);
    if (! exc && ! env && ! pops && ! mic)
        why.add ("Hum, Pop / Bias and cable Mic are all off: a healthy silent circuit stays silent.");
    const bool gestures = p.on (P::laneOn) || p[P::jkWiggle] > 0.0f || p[P::cbShake] > 0.0f || p[P::tcTouch] > 0.0f;
    if (! gestures)
        why.add ("Nothing moves: turn on the Gesture Lane or use the triggers so contacts are disturbed.");
    if (p[P::mix] <= 0.0f)
        why.add ("Dry/Wet is at 0%: the output is the (silent) dry input.");
    if (why.isEmpty())
        why.add ("The contacts stayed healthy for the whole render (no eligible fault events).");
    return why.joinIntoString ("\n");
}

juce::String suggestedFileName (const PluginState& s, const RenderSettings& r)
{
    auto clean = s.presetName.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_").trim().replaceCharacter (' ', '-');
    if (clean.isEmpty()) clean = "Init";
    const juce::String len = r.useBars ? juce::String (r.bars) + "bar" : juce::String (r.seconds, 1) + "s";
    return "Interference_" + clean + "_" + juce::String::toHexString ((juce::int64) s.seed).toUpperCase()
         + (s.take > 0 ? "-t" + juce::String ((int) s.take) : juce::String())
         + "_" + len + "_" + juce::String ((int) std::round (r.bpm)) + "bpm.wav";
}

juce::String describeFormat (const RenderSettings& r)
{
    const juce::String rate = r.sampleRate == 44100.0 ? "44.1 kHz" : juce::String ((int) (r.sampleRate / 1000.0)) + " kHz";
    const juce::String depth = r.bitDepth == 32 ? "32-bit float" : juce::String (r.bitDepth) + "-bit PCM";
    return rate + " / " + depth + " / " + (r.channels == 1 ? "mono" : "stereo");
}

RenderResult render (const PluginState& state, RenderSettings rs, const std::atomic<bool>* cancel, std::atomic<float>* progress)
{
    juce::ScopedNoDenormals noDenormals; // same arithmetic as the live processBlock
    rs.sanitise();
    RenderResult res;
    const double fs = rs.sampleRate;
    const int block = 512;
    const int body = std::max (1, (int) std::llround (rs.bodySeconds() * fs));
    const int tail = (int) std::llround (rs.tailSeconds * fs);
    const int total = body + tail;

    auto engine = std::make_unique<Engine>();
    engine->prepare (fs, block, rs.channels);
    engine->setSeed (state.seed);
    engine->setSeedLock (true); // an export is always the locked take shown in the UI
    engine->setTake (state.take);
    engine->setSlotTypes (state.slotA, state.slotB);
    engine->patternExchange().publish (state.pattern);
    engine->applyPendingNow();

    ParamSnapshot prm = state.params;
    prm[P::bypass] = 0.0f;
    prm[P::monitor] = (float) (int) rs.tap;
    prm[P::runInternal] = 0.0f;
    prm[P::trigA] = prm[P::trigB] = 0.0f;
    prm[P::fill] = 0.0f;

    TransportInfo tr;
    tr.hasHostTransport = true;
    tr.playing = true;
    tr.ppq = 0.0;
    tr.bpm = rs.bpm;
    tr.sigNum = rs.beatsPerBar;
    tr.sigDen = 4;
    tr.offline = true;

    juce::AudioBuffer<float> in (rs.channels, block), out (rs.channels, block);
    in.clear();
    res.audio.setSize (rs.channels, total);
    res.audio.clear();

    ControlEvent startEvents[2];
    int numStart = 0;
    if (rs.triggerAtStart && ! prm.on (P::laneOn))
    {
        startEvents[numStart++] = { 0, ControlEvent::GestureA, prm.choice (P::gestureA), 0.85f };
        startEvents[numStart++] = { 0, ControlEvent::GestureB, prm.choice (P::gestureB), 0.85f };
    }

    for (int pos = 0; pos < total; pos += block)
    {
        if (cancel != nullptr && cancel->load())
        {
            res.cancelled = true;
            res.audio.setSize (0, 0);
            return res;
        }
        const int n = std::min (block, total - pos);
        if (pos >= body)
            prm[P::laneOn] = 0.0f; // tail: no new gestures, tails ring out
        const float* ip[2] = { in.getReadPointer (0), in.getReadPointer (rs.channels - 1) };
        float* op[2] = { out.getWritePointer (0), out.getWritePointer (rs.channels - 1) };
        engine->process (ip, op, n, prm, tr, pos == 0 ? startEvents : nullptr, pos == 0 ? numStart : 0, nullptr, 0);
        for (int ch = 0; ch < rs.channels; ++ch)
            res.audio.copyFrom (ch, pos, out, ch, 0, n);
        tr.ppq += n / fs * rs.bpm / 60.0;
        tr.hostSeconds += n / fs;
        if (progress != nullptr)
            progress->store ((float) (pos + n) / (float) total);
    }

    res.peak = res.audio.getMagnitude (0, total);
    res.seconds = total / fs;
    res.tailIncluded = tail > 0;
    res.silent = res.peak < 1.0e-5f;
    if (res.silent)
        res.silentReason = explainSilence (state);
    else if (rs.normalize)
    {
        const float target = juce::Decibels::decibelsToGain (-rs.headroomDb);
        const float g = target / res.peak;
        res.audio.applyGain (g);
        res.appliedGainDb = juce::Decibels::gainToDecibels (g);
        res.peak = target;
    }
    res.ok = true;
    return res;
}

bool writeWav (const juce::AudioBuffer<float>& audio, const RenderSettings& settingsIn, const juce::File& file,
               bool allowOverwrite, juce::String& error)
{
    auto rs = settingsIn;
    rs.sanitise();
    if (audio.getNumSamples() <= 0 || audio.getNumChannels() <= 0)
    {
        error = "Nothing to write (empty render).";
        return false;
    }
    if (file.getFullPathName().isEmpty() || file.isDirectory())
    {
        error = "Choose a file name for the WAV.";
        return false;
    }
    if (file.exists() && ! allowOverwrite)
    {
        error = "The file already exists: " + file.getFullPathName();
        return false;
    }
    if (! file.getParentDirectory().createDirectory())
    {
        error = "Cannot create the folder " + file.getParentDirectory().getFullPathName();
        return false;
    }

    juce::TemporaryFile tmp (file);
    {
        auto stream = tmp.getFile().createOutputStream();
        if (stream == nullptr)
        {
            error = "Cannot write to " + file.getParentDirectory().getFullPathName() + " (read-only or no permission).";
            return false;
        }
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), rs.sampleRate,
                                                                              (unsigned int) audio.getNumChannels(), rs.bitDepth, {}, 0));
        if (writer == nullptr)
        {
            error = "The WAV writer rejected this format (" + describeFormat (rs) + ").";
            return false;
        }
        stream.release(); // the writer owns it now
        if (! writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
        {
            error = "Writing failed (disk full?).";
            return false;
        }
    }
    if (! tmp.overwriteTargetFileWithTemporary())
    {
        error = "Could not move the finished file into place: " + file.getFullPathName();
        return false;
    }
    return true;
}

bool writeSidecar (const PluginState& s, const RenderSettings& r, const juce::File& wavFile, juce::String& error)
{
    juce::String txt;
    txt << "Interference render" << juce::newLine
        << "Plugin version: " << IFX_VERSION_STRING << juce::newLine
        << "Preset: " << s.presetName << juce::newLine
        << "Seed: " << juce::String::toHexString ((juce::int64) s.seed).toUpperCase() << "  Take: " << (int) s.take << juce::newLine
        << "Tempo: " << r.bpm << " BPM, " << r.beatsPerBar << "/4" << juce::newLine
        << "Slot A: " << sourceName (s.slotA) << juce::newLine
        << "Slot B: " << sourceName (s.slotB) << juce::newLine
        << "Policy: " << (s.isExpressive() ? "Expressive (artistic extension)" : "Natural") << juce::newLine
        << "Studio processing: " << (s.usesStudio() ? "yes" : "no") << juce::newLine
        << "Internal excitation: " << (s.usesExcitation() ? "yes" : "no") << juce::newLine
        << "Format: " << describeFormat (r) << juce::newLine;
    const auto f = wavFile.withFileExtension ("txt");
    if (! f.replaceWithText (txt))
    {
        error = "Could not write " + f.getFullPathName();
        return false;
    }
    return true;
}

} // namespace OfflineRenderer

//==============================================================================
LiveRecorder::LiveRecorder() { thread.startThread(); }

LiveRecorder::~LiveRecorder()
{
    stop();
    thread.stopThread (2000);
}

bool LiveRecorder::start (const juce::File& file, double sampleRate, int channels, int bitDepth, bool allowOverwrite, juce::String& error)
{
    stop();
    if (file.exists() && ! allowOverwrite)
    {
        error = "The file already exists: " + file.getFullPathName();
        return false;
    }
    if (! file.getParentDirectory().createDirectory())
    {
        error = "Cannot create the folder " + file.getParentDirectory().getFullPathName();
        return false;
    }
    file.deleteFile();
    auto stream = file.createOutputStream();
    if (stream == nullptr)
    {
        error = "Cannot write to " + file.getFullPathName();
        return false;
    }
    recChannels = std::clamp (channels, 1, 2);
    juce::WavAudioFormat wav;
    auto* w = wav.createWriterFor (stream.get(), sampleRate > 0 ? sampleRate : 48000.0, (unsigned int) recChannels,
                                   bitDepth == 16 || bitDepth == 24 || bitDepth == 32 ? bitDepth : 24, {}, 0);
    if (w == nullptr)
    {
        error = "The WAV writer rejected this format.";
        return false;
    }
    stream.release();
    // ~10 s of FIFO at the host rate: drained continuously by the background thread
    writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w, thread, (int) std::max (48000.0, sampleRate * 10.0));
    written.store (0);
    dropped.store (0);
    currentFile = file;
    active.store (writer.get());
    return true;
}

juce::int64 LiveRecorder::stop()
{
    if (writer == nullptr)
        return 0;
    active.store (nullptr);
    while (users.load() > 0)
        juce::Thread::yield(); // the audio thread finishes its current push
    writer.reset();            // flushes the FIFO and closes the file
    return written.load();
}

void LiveRecorder::push (const float* const* data, int numChannels, int numSamples) noexcept
{
    users.fetch_add (1);
    if (auto* w = active.load())
    {
        const float* chans[2] = { data[0], data[std::min (numChannels, recChannels) - 1] };
        if (w->write (chans, numSamples))
            written.fetch_add (numSamples);
        else
            dropped.fetch_add (numSamples);
    }
    users.fetch_sub (1);
}

} // namespace ifx
