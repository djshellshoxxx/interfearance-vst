// Engine-level scenarios from the spec audit (Docs/research/SpecAudit.md 10, P2 "Missing tests"
// and P3 "Test extras"): pair-order distinctness, transport (forward seek, stop/start, sample-rate
// switch, tempo change, odd time signatures), offline vs real-time agreement, DC and sweeps,
// the Ceiling, Fill release, Passive Pickup vs Line, silence over Tone / Expressive, abrupt
// parameter changes, stereo phase and the input stage with the counterfactual buses.
// All gains are smoothed per sample (~10 ms), so comparisons skip the first ~50 ms after a
// change or use tolerances; nothing here depends on block-rate stepping.
#include "TestHelpers.h"
#include "Render/OfflineRenderer.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
constexpr double kFs = 48000.0;
constexpr double kTwoPi = 2.0 * juce::MathConstants<double>::pi;

/** Process `in` in chunks, calling `before (chunkIndex, pos)` ahead of each chunk. */
template <typename Fn>
juce::AudioBuffer<float> runInChunks (Harness& h, const juce::AudioBuffer<float>& in, int chunk, Fn&& before)
{
    const int n = in.getNumSamples();
    juce::AudioBuffer<float> out (h.channels, n);
    for (int pos = 0, k = 0; pos < n; pos += chunk, ++k)
    {
        const int len = std::min (chunk, n - pos);
        before (k, pos);
        auto o = h.run (slice (in, pos, len));
        for (int ch = 0; ch < h.channels; ++ch)
            out.copyFrom (ch, pos, o, ch, 0, len);
    }
    return out;
}

juce::AudioBuffer<float> dcBuffer (int channels, int n, float value)
{
    juce::AudioBuffer<float> b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, value);
    return b;
}

/** Exponential (log) sine sweep f0 -> f1 over n samples. */
juce::AudioBuffer<float> logSweepBuffer (int channels, int n, double f0, double f1, double fs, float amp)
{
    juce::AudioBuffer<float> b (channels, n);
    const double T = n / fs, k = std::log (f1 / f0);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const float v = amp * (float) std::sin (kTwoPi * f0 * T / k * (std::exp (t * k / T) - 1.0));
        for (int ch = 0; ch < channels; ++ch)
            b.setSample (ch, i, v);
    }
    return b;
}

double meanOf (const juce::AudioBuffer<float>& b, int ch, int start, int len)
{
    double s = 0.0;
    for (int i = start; i < start + len; ++i)
        s += b.getSample (ch, i);
    return len > 0 ? s / len : 0.0;
}

double rmsRange (const juce::AudioBuffer<float>& b, int ch, int start, int len)
{
    double s = 0.0;
    for (int i = start; i < start + len; ++i)
        s += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return len > 0 ? std::sqrt (s / len) : 0.0;
}

float peakRange (const juce::AudioBuffer<float>& b, int ch, int start, int len)
{
    float p = 0.0f;
    for (int i = start; i < start + len; ++i)
        p = std::max (p, std::abs (b.getSample (ch, i)));
    return p;
}

/** Largest sample-to-sample change on any channel. */
double maxSampleStep (const juce::AudioBuffer<float>& b)
{
    double m = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 1; i < b.getNumSamples(); ++i)
            m = std::max (m, (double) std::abs (b.getSample (ch, i) - b.getSample (ch, i - 1)));
    return m;
}

/** |DFT| of one channel at frequency f. */
double dftMagnitude (const juce::AudioBuffer<float>& b, int ch, double fs, double f)
{
    const double w = kTwoPi * f / fs;
    double re = 0.0, im = 0.0;
    const auto* d = b.getReadPointer (ch);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        if (d[i] == 0.0f) continue;
        re += d[i] * std::cos (w * i);
        im -= d[i] * std::sin (w * i);
    }
    return std::sqrt (re * re + im * im);
}

void addNoteWiggles (Harness& h, int n, int count = 8)
{
    for (int i = 0; i < count; ++i)
    {
        const int at = (int) (n * (i + 0.5) / count);
        h.pendingEvents.push_back ({ at, ControlEvent::NoteOn, 60 + (i % 3), 0.9f });
        h.pendingEvents.push_back ({ at + 4000, ControlEvent::NoteOff, 60 + (i % 3), 0.0f });
    }
}

/** A rig where every source both gates the signal and injects something, so the order of
    two sources is audible (realism spec 7: a downstream source acts on upstream artifacts). */
void faultyButStill (ParamSnapshot& p)
{
    p = quietParams();
    p[P::hum] = 1.0f;
    p[P::stereo] = 0.0f;
    p[P::inputMode] = 1.0f;    // high-impedance source: touched / exposed nodes are not held down
    p[P::jkInsert] = 0.7f;     // part way in: partly conducting, exposed tip
    p[P::jkWiggle] = 0.0f;
    p[P::jkShield] = 0.0f;
    p[P::jkShort] = 0.0f;
    p[P::tcTouch] = 0.8f;      // firm finger on the exposed tip: hum plus finger load
    p[P::tcPressure] = 1.0f;
    p[P::tcExposure] = 1.0f;
    p[P::tcShield] = 0.0f;
    p[P::cbBend] = 0.5f;       // bent over broken strands and braid: loss plus shield hum
    p[P::cbWeakPos] = 0.5f;
    p[P::cbDamage] = 0.8f;
    p[P::cbShieldDmg] = 0.8f;
    p[P::ptTurn] = 0.5f;       // a clean pot at a quiet setting of the Level circuit
    p[P::ptDirt] = 0.0f;
}

Pattern everyStep (int lane0Every)
{
    Pattern p;
    for (int i = 0; i < 16; ++i)
        p.lanes[0].steps[i].active = (uint8_t) (i % lane0Every == 0 ? 1 : 0);
    p.lanes[1].mute = 1;
    return p;
}

void playHost (Harness& h, double bpm = 120.0)
{
    h.transport.hasHostTransport = true;
    h.transport.playing = true;
    h.transport.bpm = bpm;
}
}

//==============================================================================
// M6.9 / R10.3: all six pairs, both orders: the order must change what you hear.
IFX_TEST (Scenarios, everyPairSoundsDifferentInEachOrder)
{
    const auto in = drums (2, (int) kFs, kFs);
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
        {
            auto render = [&] (bool swap)
            {
                Harness h (kFs, 2, 256);
                faultyButStill (h.params);
                h.params[P::swapOrder] = swap ? 1.0f : 0.0f;
                h.setSlots (kAllSources[i], kAllSources[j]);
                return h.run (in);
            };
            const auto ab = render (false), ba = render (true);
            // skip the 10 ms order-swap declick at the start
            const double d = maxAbsDiff (slice (ab, 4800, 40000), slice (ba, 4800, 40000));
            const juce::String pair = juce::String (sourceName (kAllSources[i])) + " + " + sourceName (kAllSources[j]);
            IFX_CHECK (allFinite (ab) && allFinite (ba));
            IFX_CHECK (peakOf (ab) < 2.0f && peakOf (ba) < 2.0f);
            IFX_CHECK_MSG (d > 5.0e-4, pair + ": both orders sound the same (max diff " + juce::String (d, 7) + ")");
        }
}

//==============================================================================
// M7.17: a forward seek restarts the hardware exactly like starting playback at that position.
IFX_TEST (Scenarios, forwardSeekRestartsExactlyLikeAFreshStart)
{
    const auto song = drums (2, (int) (kFs * 12), kFs);
    auto configure = [] (Harness& h)
    {
        h.params[P::hum] = 0.4f;
        h.params[P::jkInsert] = 0.8f;
        h.params[P::jkWiggle] = 0.7f;
        h.params[P::jkWear] = 0.8f;
        h.params[P::tcTouch] = 0.5f;
        h.setSlots (SourceType::LooseJack, SourceType::PlugTouch);
        playHost (h);
    };
    Harness seeker (kFs, 2, 256);
    configure (seeker);
    seeker.run (slice (song, 0, (int) kFs));         // play beats 0..2
    seeker.transport.ppq = 20.0;                      // the host jumps forwards to beat 20 (10 s)
    seeker.transport.hostSeconds = 10.0;
    const auto afterSeek = seeker.run (slice (song, (int) (kFs * 10), (int) kFs));

    Harness fresh (kFs, 2, 256);
    configure (fresh);
    fresh.transport.ppq = 20.0;
    fresh.transport.hostSeconds = 10.0;
    const auto reference = fresh.run (slice (song, (int) (kFs * 10), (int) kFs));

    IFX_CHECK (rmsOf (reference) > 1.0e-3);
    IFX_CHECK_MSG (maxAbsDiff (afterSeek, reference) < 1.0e-6,
                   "after a forward seek the output differs from a fresh start by " + juce::String (maxAbsDiff (afterSeek, reference), 7));
}

//==============================================================================
// M7.17: stop -> start from the same position replays the locked take exactly; unlocked, each
// playback start draws a new take (same settings, different micro-events).
IFX_TEST (Scenarios, stopThenStartReplaysLockedTakeAndDrawsNewTakeUnlocked)
{
    const auto song = drums (2, (int) (kFs * 2), kFs);
    for (int locked = 1; locked >= 0; --locked)
    {
        Harness h (kFs, 2, 256);
        h.engine->setSeedLock (locked == 1);
        h.params[P::hum] = 0.3f;
        h.params[P::jkInsert] = 0.8f;
        h.params[P::jkWiggle] = 0.8f;
        h.params[P::jkWear] = 0.9f;
        h.params[P::cbShake] = 0.7f;
        h.params[P::cbDamage] = 0.8f;
        h.setSlots (SourceType::LooseJack, SourceType::CableHandling);
        playHost (h);
        const auto first = h.run (song);
        const uint64_t firstSeed = h.engine->effectiveSeed();
        h.transport.playing = false;                  // stopped: audio still flows, no clock
        const auto stopped = h.run (song);
        h.transport.playing = true;                   // start again from the top
        h.transport.ppq = 0.0;
        h.transport.hostSeconds = 0.0;
        const auto second = h.run (song);

        IFX_CHECK (allFinite (stopped) && peakOf (stopped) < 2.0f);
        IFX_CHECK (rmsOf (stopped) > 1.0e-3);        // stopping the transport does not mute the plug-in
        if (locked == 1)
        {
            IFX_CHECK (h.engine->effectiveSeed() == firstSeed);
            IFX_CHECK_MSG (maxAbsDiff (first, second) < 1.0e-6,
                           "locked replay differs by " + juce::String (maxAbsDiff (first, second), 7));
        }
        else
        {
            IFX_CHECK (h.engine->effectiveSeed() != firstSeed);
            IFX_CHECK_MSG (maxAbsDiff (first, second) > 1.0e-4, "unlocked restart should draw a new take");
        }
    }
}

//==============================================================================
// M7.17: after the host changes the sample rate (re-prepare), the engine sounds exactly like a
// freshly prepared one at the new rate, in both range policies.
IFX_TEST (Scenarios, sampleRateSwitchLeavesNoTraceOfTheOldRate)
{
    for (int pol = 0; pol < 2; ++pol)
    {
        auto configure = [pol] (Harness& h)
        {
            h.params = quietParams();
            h.params[P::policy] = (float) pol;
            h.params[P::hum] = 1.0f;
            h.params[P::jkInsert] = 0.0f;     // withdrawn: the open input hums (injection path)
            h.params[P::jkWiggle] = 0.0f;
            h.params[P::jkShield] = 0.3f;
            h.params[P::jkShort] = 0.0f;
            h.params[P::tcTouch] = 0.6f;
            h.setSlots (SourceType::PlugTouch, SourceType::LooseJack);
        };
        Harness switched (44100.0, 2, 256);
        configure (switched);
        switched.run (drums (2, 44100, 44100.0));
        switched.engine->prepare (96000.0, 256, 2);  // the host switches to 96 kHz
        switched.fs = 96000.0;
        const auto a = switched.run (drums (2, 96000, 96000.0));

        Harness fresh (96000.0, 2, 256);
        configure (fresh);
        const auto b = fresh.run (drums (2, 96000, 96000.0));

        IFX_CHECK (allFinite (a));
        IFX_CHECK (rmsOf (b) > 1.0e-4);
        IFX_CHECK_MSG (maxAbsDiff (a, b) < 1.0e-6, juce::String (pol == 0 ? "Natural" : "Expressive")
                                                       + ": re-prepared engine differs from a fresh one by "
                                                       + juce::String (maxAbsDiff (a, b), 7));
    }
}

//==============================================================================
// T11 / I5.6c: tempo automation keeps every lane gesture on the ppq grid.
IFX_TEST (Scenarios, tempoChangesKeepLaneGesturesOnTheGrid)
{
    const int block = 64;
    Harness h (kFs, 2, block);
    h.params = quietParams();
    h.params[P::laneOn] = 1.0f;
    h.params[P::humanize] = 0.0f;
    h.params[P::swing] = 0.0f;
    h.params[P::laneDiv] = 3.0f;                      // 1/16
    h.setSlots (SourceType::LooseJack, SourceType::None);
    h.publish (everyStep (1));
    playHost (h, 120.0);

    const double L = 0.25;
    int starts = 0, offGrid = 0;
    uint32_t last = 0;
    for (double bpm : { 120.0, 90.0, 150.0 })
    {
        h.transport.bpm = bpm;
        const double oneSample = bpm / 60.0 / kFs;    // one sample in beats
        for (int b = 0; b < (int) (kFs * 1.5) / block; ++b)
        {
            const double p0 = h.transport.ppq;
            h.runSilence (block);
            const double p1 = h.transport.ppq;
            const uint32_t now = h.engine->telemetry.slot[0].gestures.load();
            if (now > last)
            {
                starts += (int) (now - last);
                const double k = std::ceil ((p0 - oneSample) / L);  // first grid point at or after the block start
                if (k * L > p1 + oneSample || now - last > 1)
                    ++offGrid;
            }
            last = now;
        }
    }
    const int expected = (int) std::floor (h.transport.ppq / L - 1.0e-9) + 1; // grid points passed, incl. 0
    IFX_CHECK_MSG (offGrid == 0, juce::String (offGrid) + " gesture starts fell between grid points");
    IFX_CHECK_MSG (std::abs (starts - expected) <= 1, "gestures " + juce::String (starts) + ", grid steps " + juce::String (expected));
}

//==============================================================================
// I5.6a: odd time signatures. Bars (which re-key the random streams and Evolve) start on the
// signature's own grid (every 3.5 beats in 7/8, 5 beats in 5/4); lane steps stay on the beat grid.
IFX_TEST (Scenarios, oddTimeSignaturesPutBarsOnTheirOwnGrid)
{
    const std::pair<int, int> signatures[] = { { 7, 8 }, { 5, 4 } };
    for (const auto& sig : signatures)
    {
        const int block = 64;
        Harness h (kFs, 2, block);
        h.params = quietParams();
        h.params[P::laneOn] = 1.0f;
        h.params[P::humanize] = 0.0f;
        h.params[P::swing] = 0.0f;
        h.params[P::laneDiv] = 3.0f;                  // 1/16, a gesture on every beat
        h.setSlots (SourceType::LooseJack, SourceType::None);
        h.publish (everyStep (4));
        playHost (h, 120.0);
        h.transport.sigNum = sig.first;
        h.transport.sigDen = sig.second;

        const double beatsPerBar = sig.first * 4.0 / sig.second;
        const double blockBeats = block * 120.0 / 60.0 / kFs;
        std::vector<std::pair<int, double>> bars;     // (bar index, ppq of the block it started in)
        uint32_t readPos = h.engine->telemetry.logWrite.load();
        for (int b = 0; b < (int) (kFs * 7.0) / block; ++b)  // 14 beats at 120 BPM
        {
            const double p0 = h.transport.ppq;
            h.runSilence (block);
            const uint32_t w = h.engine->telemetry.logWrite.load();
            for (; readPos < w; ++readPos)
            {
                const auto& e = h.engine->telemetry.log[readPos % (uint32_t) EngineTelemetry::kLogSize];
                if (e.code == LogBar)
                    bars.push_back ({ (int) e.a, p0 });
            }
        }
        const juce::String name = juce::String (sig.first) + "/" + juce::String (sig.second);
        const int expectedBars = (int) std::floor (14.0 / beatsPerBar - 1.0e-9) + 1;
        IFX_CHECK_MSG ((int) bars.size() == expectedBars, name + ": " + juce::String ((int) bars.size()) + " bars, expected "
                                                              + juce::String (expectedBars));
        for (size_t k = 0; k < bars.size(); ++k)
        {
            const double boundary = (double) k * beatsPerBar;
            IFX_CHECK_MSG (bars[k].first == (int) k, name + ": bar index " + juce::String (bars[k].first));
            IFX_CHECK_MSG (boundary >= bars[k].second - 1.0e-6 && boundary < bars[k].second + blockBeats + 1.0e-6,
                           name + ": bar " + juce::String ((int) k) + " started near beat " + juce::String (bars[k].second, 3)
                               + ", expected " + juce::String (boundary, 3));
        }
        const auto gestures = (int) h.engine->telemetry.slot[0].gestures.load();
        IFX_CHECK_MSG (std::abs (gestures - 14) <= 1, name + ": " + juce::String (gestures) + " beat gestures in 14 beats");
    }
}

//==============================================================================
// T15: the offline renderer and the real-time engine (different block size) produce the same audio
// for the same state, seed, take and transport.
IFX_TEST (Scenarios, offlineRenderMatchesTheRealTimeEngine)
{
    for (int variant = 0; variant < 2; ++variant)
    {
        PluginState s;
        s.slotA = SourceType::LooseJack;
        s.slotB = SourceType::PlugTouch;
        s.seed = 0xABCDEFull;
        s.take = 2;
        s.params[P::mode] = 2.0f;
        s.params[P::excType] = 1.0f;
        s.params[P::excLevel] = -24.0f;
        s.params[P::hum] = 0.5f;
        s.params[P::jkInsert] = 0.8f;
        s.params[P::jkWear] = 0.8f;
        s.params[P::jkWiggle] = 0.6f;
        s.params[P::tcTouch] = 0.5f;
        s.params[P::gestureA] = 0.0f;
        s.params[P::gestureB] = 4.0f;
        if (variant == 1)                              // lane + Studio (echo is tempo-synced)
        {
            s.params[P::laneOn] = 1.0f;
            s.pattern = everyStep (2);
            s.pattern.lanes[1].mute = 0;
            for (int i = 0; i < 12; i += 3) s.pattern.lanes[1].steps[i].active = 1;
            s.pattern.lanes[1].length = 12;
            s.params[P::studioOn] = 1.0f;
            s.params[P::studioSrc] = 2.0f;
            s.params[P::drOn] = 1.0f;
            s.params[P::ecOn] = 1.0f;
        }
        RenderSettings rs;
        rs.useBars = false;
        rs.seconds = 1.5;
        rs.bpm = 128.0;
        rs.beatsPerBar = 4;
        rs.sampleRate = 48000.0;
        rs.channels = 2;
        rs.tailSeconds = 0.0;
        rs.normalize = false;
        rs.tap = Tap::FullOutput;
        rs.triggerAtStart = true;
        const auto r = OfflineRenderer::render (s, rs);
        IFX_CHECK (r.ok);

        // the same thing, driven like a host would drive the live engine (480-sample blocks)
        Harness h (rs.sampleRate, rs.channels, 480);
        h.engine->setSeed (s.seed);
        h.engine->setSeedLock (true);
        h.engine->setTake (s.take);
        h.params = s.params;
        h.params[P::bypass] = 0.0f;
        h.params[P::monitor] = (float) (int) rs.tap;
        h.params[P::runInternal] = 0.0f;
        h.params[P::trigA] = h.params[P::trigB] = 0.0f;
        h.params[P::fill] = 0.0f;
        h.setSlots (s.slotA, s.slotB);
        h.publish (s.pattern);
        h.transport.hasHostTransport = true;
        h.transport.playing = true;
        h.transport.ppq = 0.0;
        h.transport.bpm = rs.bpm;
        h.transport.sigNum = rs.beatsPerBar;
        h.transport.sigDen = 4;
        h.transport.offline = true;
        if (! h.params.on (P::laneOn))
        {
            h.pendingEvents.push_back ({ 0, ControlEvent::GestureA, h.params.choice (P::gestureA), 0.85f });
            h.pendingEvents.push_back ({ 0, ControlEvent::GestureB, h.params.choice (P::gestureB), 0.85f });
        }
        const auto live = h.runSilence (r.audio.getNumSamples());

        IFX_CHECK (r.audio.getNumSamples() == live.getNumSamples() && r.audio.getNumChannels() == live.getNumChannels());
        IFX_CHECK (rmsOf (live) > 1.0e-4);
        IFX_CHECK_MSG (maxAbsDiff (r.audio, live) < 1.0e-5, "variant " + juce::String (variant) + ": render vs live differ by "
                                                                + juce::String (maxAbsDiff (r.audio, live), 7));
    }
}

//==============================================================================
// T1 / T3: DC input. Healthy hardware passes it unchanged and steady (both circuit modes).
IFX_TEST (Scenarios, dcInputPassesHealthyHardwareSteadily)
{
    for (int circuit = 0; circuit < 2; ++circuit)
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::inputMode] = (float) circuit;     // Passive Pickup is normalised to unity DC gain
        h.params[P::jkInsert] = 1.0f;
        h.params[P::jkWiggle] = 0.0f;
        h.params[P::cbDamage] = 0.0f;
        h.params[P::cbShieldDmg] = 0.0f;
        h.params[P::cbShake] = 0.0f;
        h.setSlots (SourceType::LooseJack, SourceType::CableHandling);
        const auto out = h.run (dcBuffer (2, (int) (kFs * 2), 0.5f));
        const int from = (int) kFs, len = (int) kFs;
        for (int ch = 0; ch < 2; ++ch)
        {
            const double m = meanOf (out, ch, from, len);
            double lo = 1.0e9, hi = -1.0e9;
            for (int i = from; i < from + len; ++i) { lo = std::min (lo, (double) out.getSample (ch, i)); hi = std::max (hi, (double) out.getSample (ch, i)); }
            IFX_CHECK_MSG (std::abs (m - 0.5) < 0.01, "circuit " + juce::String (circuit) + ": DC out " + juce::String (m, 5));
            IFX_CHECK_MSG (hi - lo < 1.0e-4, "circuit " + juce::String (circuit) + ": DC output wanders by " + juce::String (hi - lo, 6));
        }
    }
}

// T3: injected artifacts (hum, pops, bias scratch) carry no DC, with a DC input or on silence.
IFX_TEST (Scenarios, injectedArtifactsCarryNoDc)
{
    for (int withDc = 1; withDc >= 0; --withDc)
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::hum] = 0.5f;
        h.params[P::jkInsert] = 0.75f;
        h.params[P::jkWiggle] = 0.8f;
        h.params[P::jkWear] = 0.8f;
        h.params[P::jkWearPos] = 0.75f;
        h.params[P::jkPop] = 1.0f;
        h.params[P::jkShield] = 0.5f;
        h.params[P::ptDirt] = 1.0f;
        h.params[P::ptBias] = 1.0f;
        h.params[P::ptVibration] = 1.0f;              // severe dirt stays intermittent under vibration
        h.params[P::ptWearPos] = 0.75f;
        h.params[P::monitor] = (float) (int) (withDc ? Tap::Artifacts : Tap::FullOutput);
        h.setSlots (SourceType::LooseJack, SourceType::DirtyPot);
        const int n = (int) (kFs * 3);
        addNoteWiggles (h, n, 12);
        const auto out = withDc ? h.run (dcBuffer (2, n, 0.3f)) : h.runSilence (n);
        IFX_CHECK (allFinite (out));
        IFX_CHECK (peakOf (out) > 1.0e-4);            // something was injected
        for (int ch = 0; ch < 2; ++ch)
        {
            const double m = meanOf (out, ch, (int) kFs, 2 * (int) kFs);
            IFX_CHECK_MSG (std::abs (m) < 0.01, juce::String (withDc ? "A bus with DC input" : "output on silence")
                                                    + ": mean " + juce::String (m, 5));
        }
    }
}

// T1: a log sweep (20 Hz..20 kHz). Healthy: transparent at every frequency. Faulty rig with Studio:
// finite, bounded, no DC build-up.
IFX_TEST (Scenarios, logSweepThroughHealthyAndFaultyRigs)
{
    const int n = (int) (kFs * 4);
    const auto in = logSweepBuffer (2, n, 20.0, 20000.0, kFs, 0.5f);
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::jkInsert] = 1.0f;
        h.params[P::jkWiggle] = 0.0f;
        h.setSlots (SourceType::LooseJack, SourceType::None);
        const auto out = h.run (in);
        const int w = n / 16;
        for (int k = 0; k < 16; ++k)
        {
            const double r = rmsRange (out, 0, k * w, w) / std::max (1.0e-12, rmsRange (in, 0, k * w, w));
            IFX_CHECK_MSG (r > 0.97 && r < 1.01, "seated jack, sweep window " + juce::String (k) + ": gain " + juce::String (r, 4));
        }
    }
    {
        Harness h (kFs, 2, 256);
        h.params[P::hum] = 0.5f;
        h.params[P::inputMode] = 1.0f;                // Passive Pickup resonance in the chain
        h.params[P::jkInsert] = 0.8f;
        h.params[P::jkWiggle] = 0.6f;
        h.params[P::jkWear] = 0.8f;
        h.params[P::ptDirt] = 0.8f;
        h.params[P::ptScratch] = 1.0f;
        h.params[P::studioOn] = 1.0f;
        h.params[P::studioSrc] = 2.0f;
        h.params[P::drOn] = 1.0f;
        h.params[P::drCurve] = 2.0f;                  // asymmetric drive: the Studio must block its DC
        h.params[P::ecOn] = 1.0f;
        h.setSlots (SourceType::LooseJack, SourceType::DirtyPot);
        addNoteWiggles (h, n, 8);
        const auto out = h.run (in);
        IFX_CHECK (allFinite (out));
        IFX_CHECK_MSG (peakOf (out) < 2.0f, "peak " + juce::String (peakOf (out)));
        const int w = (int) (kFs * 0.5);
        for (int k = 0; k < n / w; ++k)
            for (int ch = 0; ch < 2; ++ch)
            {
                const double m = meanOf (out, ch, k * w, w);
                IFX_CHECK_MSG (std::abs (m) < 0.02, "window " + juce::String (k) + " mean " + juce::String (m, 5));
            }
    }
}

//==============================================================================
// H19 / C3.17: the Ceiling holds overs at its level, rides the gain (stereo-linked, not a
// clipper), releases in tens of milliseconds, and is an exact identity when off.
IFX_TEST (Scenarios, ceilingHoldsOversLinkedAndReleases)
{
    const int loud = (int) (kFs * 0.5), quiet = (int) (kFs * 0.5), n = loud + quiet;
    juce::AudioBuffer<float> in (2, n);
    for (int i = 0; i < n; ++i)
    {
        const float s = (float) std::sin (kTwoPi * 1000.0 * i / kFs);
        in.setSample (0, i, (i < loud ? 1.0f : 0.1f) * s);
        in.setSample (1, i, (i < loud ? 0.2f : 0.02f) * s);
    }
    const float ceilingGain = dbToGain (-6.0f);

    Harness h (kFs, 2, 256);
    h.params = quietParams();
    h.params[P::ceilOn] = 1.0f;
    h.params[P::ceiling] = -6.0f;
    h.setSlots (SourceType::None, SourceType::None);
    const auto out = h.run (in);

    // 1. overs are held at the ceiling
    IFX_CHECK_MSG (peakOf (slice (out, 0, loud)) <= ceilingGain * 1.0001f, "peak " + juce::String (peakOf (slice (out, 0, loud))));
    // 2. level riding, not clipping: the steady loud part is a sine just under the ceiling
    const double rmsL = rmsRange (out, 0, loud / 2, loud / 2);
    IFX_CHECK_MSG (rmsL > ceilingGain * 0.66 && rmsL < ceilingGain * 0.75, "loud rms " + juce::String (rmsL, 4) + " (a clipped sine would be ~0.89 x ceiling)");
    // 3. stereo-linked: the quiet right channel is turned down by the same gain
    const double rmsR = rmsRange (out, 1, loud / 2, loud / 2);
    IFX_CHECK_MSG (std::abs (rmsR / rmsL - 0.2) < 0.02, "R/L " + juce::String (rmsR / rmsL, 4));
    // 4. release: 1..3 ms after the level drops the gain is still low; 300 ms later it has recovered
    const double gEarly = peakRange (out, 0, loud + 48, 96) / peakRange (in, 0, loud + 48, 96);
    const double gLate = peakRange (out, 0, loud + (int) (kFs * 0.3), 480) / peakRange (in, 0, loud + (int) (kFs * 0.3), 480);
    IFX_CHECK_MSG (gEarly < 0.75, "gain 1-3 ms after the overs " + juce::String (gEarly, 4) + " (release should not be instant)");
    IFX_CHECK_MSG (gLate > 0.98, "gain 300 ms later " + juce::String (gLate, 4));

    // 5. Ceiling off: exact identity, even for overs
    Harness g (kFs, 2, 256);
    g.params = quietParams();
    g.params[P::ceilOn] = 0.0f;
    g.params[P::ceiling] = -6.0f;
    g.setSlots (SourceType::None, SourceType::None);
    IFX_CHECK (maxAbsDiff (g.run (in), in) == 0.0);
}

//==============================================================================
// I5.6e: Fill (parameter or held MIDI note 63) adds events and the lane returns exactly to the
// base pattern on release.
IFX_TEST (Scenarios, fillReturnsToTheBasePatternOnRelease)
{
    const int half = (int) (kFs * 2);                 // 4 beats = 16 steps at 120 BPM
    auto count = [half] (int mode, uint32_t& firstHalf, uint32_t& secondHalf)
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::laneOn] = 1.0f;
        h.params[P::humanize] = 0.0f;
        h.params[P::swing] = 0.0f;
        h.setSlots (SourceType::LooseJack, SourceType::None);
        h.publish (everyStep (4));
        playHost (h, 120.0);
        if (mode == 1)
            h.params[P::fill] = 1.0f;
        if (mode == 2)
        {
            h.pendingEvents.push_back ({ 0, ControlEvent::NoteOn, 63, 1.0f });
            h.pendingEvents.push_back ({ half, ControlEvent::NoteOff, 63, 0.0f });
        }
        h.runSilence (half);
        firstHalf = h.engine->telemetry.slot[0].gestures.load();
        h.params[P::fill] = 0.0f;
        h.runSilence (half);
        secondHalf = h.engine->telemetry.slot[0].gestures.load() - firstHalf;
    };
    uint32_t base1 = 0, base2 = 0;
    count (0, base1, base2);
    IFX_CHECK_MSG (base1 == 4 && base2 == 4, "base pattern " + juce::String ((int) base1) + " / " + juce::String ((int) base2));
    for (int mode = 1; mode <= 2; ++mode)
    {
        uint32_t f1 = 0, f2 = 0;
        count (mode, f1, f2);
        const juce::String how = mode == 1 ? "Fill parameter" : "MIDI note 63";
        IFX_CHECK_MSG (f1 > base1, how + ": fill added no events (" + juce::String ((int) f1) + ")");
        IFX_CHECK_MSG (f2 == base2, how + ": after release " + juce::String ((int) f2) + " events, base " + juce::String ((int) base2));
    }
}

//==============================================================================
// W1: Passive Pickup is a resonant low-pass whose peak falls as Cable Load rises; Buffered/Line
// has no cable response at all.
IFX_TEST (Scenarios, passivePickupResonatesAndCableLoadLowersThePeak)
{
    auto impulseResponse = [] (int mode, float loadPf)
    {
        Harness h (kFs, 1, 256);
        h.params = quietParams();
        h.params[P::inputMode] = (float) mode;
        h.params[P::cableLoad] = loadPf;
        h.setSlots (SourceType::None, SourceType::None);
        juce::AudioBuffer<float> in (1, 16384);
        in.clear();
        in.setSample (0, 64, 0.5f);
        return h.run (in);
    };
    auto gain = [] (const juce::AudioBuffer<float>& ir, double f) { return dftMagnitude (ir, 0, kFs, f) / 0.5; };
    auto findPeak = [&gain] (const juce::AudioBuffer<float>& ir, double& fPeak)
    {
        double best = 0.0;
        for (double f = 200.0; f <= 12000.0; f *= std::pow (2.0, 1.0 / 48.0))
        {
            const double g = gain (ir, f);
            if (g > best) { best = g; fPeak = f; }
        }
        return best;
    };

    const auto line = impulseResponse (0, 1500.0f);
    for (double f : { 60.0, 1000.0, 5000.0, 15000.0 })
        IFX_CHECK_MSG (std::abs (gain (line, f) - 1.0) < 1.0e-3, "Line mode is not flat at " + juce::String (f) + " Hz: " + juce::String (gain (line, f), 5));

    double fLow = 0.0, fHigh = 0.0;
    const auto light = impulseResponse (1, 200.0f), heavy = impulseResponse (1, 1500.0f);
    const double pkLight = findPeak (light, fHigh), pkHeavy = findPeak (heavy, fLow);
    IFX_CHECK_MSG (pkLight > 1.4 && pkHeavy > 1.4, "resonant peaks " + juce::String (pkLight, 3) + " / " + juce::String (pkHeavy, 3));
    IFX_CHECK_MSG (fLow < 0.7 * fHigh, "peak at " + juce::String (fHigh, 0) + " Hz (200 pF) vs " + juce::String (fLow, 0) + " Hz (1500 pF)");
    for (const auto* ir : { &light, &heavy })
    {
        IFX_CHECK_MSG (std::abs (gain (*ir, 60.0) - 1.0) < 0.05, "passive low-frequency gain " + juce::String (gain (*ir, 60.0), 4));
        IFX_CHECK_MSG (gain (*ir, 16000.0) < 0.5, "passive gain at 16 kHz " + juce::String (gain (*ir, 16000.0), 4));
    }
}

//==============================================================================
// R2.2: the silence invariant also holds with the pot's Tone circuit and in Expressive policy.
IFX_TEST (Scenarios, silenceInvariantWithToneCircuitAndExpressive)
{
    std::vector<std::pair<SourceType, SourceType>> combos;
    combos.push_back ({ SourceType::None, SourceType::None });
    for (auto a : kAllSources)
    {
        combos.push_back ({ a, SourceType::None });
        for (auto b : kAllSources)
            if (a != b) combos.push_back ({ a, b });
    }
    int failures = 0;
    juce::String first;
    for (auto [a, b] : combos)
        for (int circuit = 0; circuit < 2; ++circuit)
            for (int tone = 0; tone < 2; ++tone)
                for (int pol = 0; pol < 2; ++pol)
                {
                    if (tone == 0 && pol == 0) continue;  // covered by Engine.silenceInvariantEverySourcePairModeAndMix
                    Harness h (kFs, 2, 128);
                    h.params = quietParams();
                    h.params[P::inputMode] = (float) circuit;
                    h.params[P::ptCircuit] = (float) tone;
                    h.params[P::policy] = (float) pol;
                    h.params[P::jkInsert] = 0.7f;
                    h.params[P::jkWiggle] = 1.0f;
                    h.params[P::cbShake] = 1.0f;
                    h.params[P::cbDamage] = 1.0f;
                    h.params[P::ptDirt] = 1.0f;
                    h.params[P::ptVibration] = 1.0f;
                    h.setSlots (a, b);
                    const int n = (int) (kFs * 0.5);
                    addNoteWiggles (h, n);
                    const auto out = h.runSilence (n);
                    if (peakOf (out) > 1.0e-7f)
                    {
                        if (failures++ == 0)
                            first = juce::String (sourceName (a)) + " + " + sourceName (b) + ", circuit " + juce::String (circuit)
                                  + ", tone " + juce::String (tone) + ", policy " + juce::String (pol) + ": peak " + juce::String (peakOf (out));
                    }
                }
    IFX_CHECK_MSG (failures == 0, juce::String (failures) + " configurations were not silent; first: " + first);
}

//==============================================================================
// C7.8: abrupt gain / routing changes every few milliseconds never click. On a quiet 220 Hz sine
// the largest natural sample step is about 0.05 (at +24 dB total gain); an unsmoothed switch
// jumps by several tenths.
IFX_TEST (Scenarios, abruptGainAndRoutingChangesAreDeclicked)
{
    Harness h (kFs, 2, 128);
    h.params = quietParams();
    h.params[P::jkInsert] = 1.0f;
    h.params[P::jkWiggle] = 0.0f;
    h.params[P::ptDirt] = 0.0f;                        // a clean pot: turning it changes only the level
    h.setSlots (SourceType::LooseJack, SourceType::DirtyPot);
    Rng r (99);
    auto pick = [&r] (std::initializer_list<float> v) { return *(v.begin() + std::min ((int) v.size() - 1, (int) (r.uniform() * v.size()))); };
    const auto in = sine (2, (int) (kFs * 4), 220.0, kFs, 0.1f);
    const auto out = runInChunks (h, in, 128, [&] (int k, int)
    {
        if (k < 8) return;                             // let the first 20 ms settle
        switch ((int) (r.uniform() * 16.0))
        {
            case 0:  h.params[P::outTrim] = pick ({ -24.0f, 0.0f, 12.0f }); break;
            case 1:  h.params[P::inTrim] = pick ({ -24.0f, 0.0f, 12.0f }); break;
            case 2:  h.params[P::mix] = pick ({ 0.0f, 0.5f, 1.0f }); break;
            case 3:  h.params[P::mixLaw] = pick ({ 0.0f, 1.0f }); break;
            case 4:  h.params[P::dryTrim] = pick ({ -24.0f, 0.0f, 6.0f }); break;
            case 5:  h.params[P::monitor] = pick ({ 0.0f, 1.0f, 2.0f, 3.0f, 5.0f, 6.0f }); break;
            case 6:  h.params[P::bypass] = pick ({ 0.0f, 1.0f }); break;
            case 7:  h.params[P::slotAOn] = pick ({ 0.0f, 1.0f }); break;
            case 8:  h.params[P::slotBOn] = pick ({ 0.0f, 1.0f }); break;
            case 9:  h.params[P::inputMode] = pick ({ 0.0f, 1.0f }); break;
            case 10: h.params[P::preOn] = pick ({ 0.0f, 1.0f });
                     h.params[P::preDrive] = pick ({ 0.0f, 12.0f, 24.0f });
                     h.params[P::preHeadroom] = pick ({ 0.0f, 9.0f, 18.0f }); break;
            case 11: h.params[P::rawHw] = pick ({ 0.0f, 1.0f }); break;
            case 12: h.params[P::studioOn] = pick ({ 0.0f, 1.0f });
                     h.params[P::studioSrc] = pick ({ 0.0f, 1.0f, 2.0f });
                     h.params[P::studioLevel] = pick ({ -24.0f, 0.0f, 12.0f }); break;
            case 13: h.params[P::ceilOn] = pick ({ 0.0f, 1.0f });
                     h.params[P::ceiling] = pick ({ -24.0f, -6.0f, 0.0f }); break;
            case 14: h.params[P::ptTurn] = (float) r.uniform(); break;
            default: h.params[P::stereo] = (float) r.uniform();
                     h.params[P::hum] = (float) r.uniform(); break;
        }
    });
    IFX_CHECK (allFinite (out));
    IFX_CHECK (peakOf (out) <= kSafetyClamp);
    IFX_CHECK_MSG (maxSampleStep (out) < 0.1, "largest sample step " + juce::String (maxSampleStep (out), 4));
}

// C7.8: switching the pot's Taper or its Level/Tone circuit mid-run crossfades (~10 ms) between
// the old and new law. Unramped, a taper switch at 75 % would move the level 0.75 -> 0.33 in one sample.
IFX_TEST (Scenarios, potTaperAndCircuitSwitchesAreDeclicked)
{
    Harness h (kFs, 2, 128);
    h.params = quietParams();
    h.params[P::ptDirt] = 0.0f;
    h.params[P::ptTurn] = 0.75f;
    h.setSlots (SourceType::DirtyPot, SourceType::None);
    const auto in = sine (2, (int) (kFs * 2), 220.0, kFs, 0.3f);   // natural step <= 0.009
    const int every = (int) (kFs * 0.25) / 128;
    const auto out = runInChunks (h, in, 128, [&] (int k, int)
    {
        if (k == 0 || k % every != 0) return;
        const int m = k / every;
        h.params[P::ptTaper] = (float) (m % 3);
        h.params[P::ptCircuit] = (float) ((m / 3) % 2);
    });
    IFX_CHECK (allFinite (out));
    IFX_CHECK_MSG (maxSampleStep (out) < 0.03, "largest sample step " + juce::String (maxSampleStep (out), 4));
}

// Realism spec 1: switching the range policy on still hardware makes no contact event and no
// step in the (legitimately louder) hum. Switched a quarter mains cycle into a period, where the
// hum waveform is far from zero.
IFX_TEST (Scenarios, policySwitchOnStillHardwareIsSmoothAndEventFree)
{
    Harness h (kFs, 2, 256);
    h.params = quietParams();
    h.params[P::hum] = 1.0f;
    h.params[P::stereo] = 0.0f;
    h.params[P::tcTouch] = 1.0f;                       // a firm, motionless finger on the exposed tip
    h.params[P::tcPressure] = 1.0f;
    h.params[P::tcExposure] = 1.0f;
    h.params[P::tcShield] = 0.0f;
    h.params[P::inputMode] = 1.0f;                     // passive source: full pickup, so a step would be large
    h.setSlots (SourceType::PlugTouch, SourceType::None);
    const int switchAt = (int) (kFs * 1.005);          // 50 Hz: a quarter period past 1 s
    const auto before = h.runSilence (switchAt);
    const auto eventsBefore = h.engine->telemetry.slot[0].openEvents.load() + h.engine->telemetry.slot[0].resistiveEvents.load();
    h.params[P::policy] = 1.0f;
    const auto after = h.runSilence ((int) kFs);
    const auto eventsAfter = h.engine->telemetry.slot[0].openEvents.load() + h.engine->telemetry.slot[0].resistiveEvents.load();

    juce::AudioBuffer<float> all (2, before.getNumSamples() + after.getNumSamples());
    for (int ch = 0; ch < 2; ++ch)
    {
        all.copyFrom (ch, 0, before, ch, 0, before.getNumSamples());
        all.copyFrom (ch, before.getNumSamples(), after, ch, 0, after.getNumSamples());
    }
    IFX_CHECK_MSG (eventsAfter == eventsBefore, "policy switch caused " + juce::String ((int) (eventsAfter - eventsBefore)) + " contact events");
    IFX_CHECK_MSG (rmsRange (after, 0, (int) (kFs * 0.5), (int) (kFs * 0.5)) > 1.5 * rmsRange (before, 0, (int) (kFs * 0.5), (int) (kFs * 0.5)),
                   "Expressive should allow a louder pickup");
    IFX_CHECK_MSG (maxSampleStep (all) < 0.02, "largest sample step across the switch " + juce::String (maxSampleStep (all), 4));
}

// C7.8: random parameter jumps every block, all sources, both layouts: always finite and bounded.
IFX_TEST (Scenarios, randomParameterJumpsEveryBlockStayFiniteAndBounded)
{
    Rng r (4242);
    int bad = 0;
    for (int trial = 0; trial < 24; ++trial)
    {
        Harness h (trial % 2 ? 44100.0 : 96000.0, 1 + trial % 2, 32 + (int) r.range (0, 480));
        const SourceType a = kAllSources[trial % 4];
        const SourceType b = trial % 5 == 0 ? SourceType::None : kAllSources[(trial + 1 + trial / 4) % 4];
        h.setSlots (a, a == b ? SourceType::None : b);
        h.transport.hasHostTransport = trial % 3 == 0;
        h.transport.playing = trial % 3 == 0;
        const auto in = drums (h.channels, 24000, h.fs, (uint64_t) trial);
        const auto out = runInChunks (h, in, h.block, [&] (int, int)
        {
            for (int k = 0; k < 6; ++k)
            {
                const int i = std::min (kNumParams - 1, (int) (r.uniform() * kNumParams));
                const auto& inf = paramInfo (i);
                const double u = r.uniform();
                float v = u < 0.25 ? inf.min : (u < 0.5 ? inf.max : inf.min + (inf.max - inf.min) * (float) r.uniform());
                if (inf.kind != ParamKind::Float) v = std::round (v);
                h.params[i] = v;
            }
            h.params[P::bypass] = 0.0f;
        });
        if (! allFinite (out) || peakOf (out) > kSafetyClamp) ++bad;
    }
    IFX_CHECK_MSG (bad == 0, juce::String (bad) + " runs out of bounds");
}

//==============================================================================
// C7.9: with Stereo Difference at 0 and identical L/R input, both channels are identical for every
// source (one mechanical contact state, no independent channel faults); raising it adds only
// small electrical differences.
IFX_TEST (Scenarios, stereoDifferenceZeroKeepsChannelsIdentical)
{
    auto render = [] (SourceType t, float stereo)
    {
        Harness h (kFs, 2, 256);
        h.params[P::hum] = 0.6f;
        h.params[P::stereo] = stereo;
        h.params[P::inputMode] = 1.0f;
        h.params[P::jkInsert] = 0.75f;
        h.params[P::jkWiggle] = 0.8f;
        h.params[P::jkWear] = 0.8f;
        h.params[P::jkPop] = 0.5f;
        h.params[P::tcTouch] = 0.6f;
        h.params[P::cbShake] = 0.6f;
        h.params[P::cbMic] = 0.5f;
        h.params[P::cbShieldDmg] = 0.6f;
        h.params[P::ptDirt] = 0.8f;
        h.params[P::ptBias] = 0.5f;
        h.setSlots (t, SourceType::None);
        addNoteWiggles (h, (int) kFs);
        return h.run (drums (2, (int) kFs, kFs));  // identical left and right input
    };
    for (auto t : kAllSources)
    {
        const auto out = render (t, 0.0f);
        double d = 0.0;
        for (int i = 0; i < out.getNumSamples(); ++i)
            d = std::max (d, (double) std::abs (out.getSample (0, i) - out.getSample (1, i)));
        IFX_CHECK_MSG (d < 1.0e-7, juce::String (sourceName (t)) + ": L/R differ by " + juce::String (d, 8));
    }
    const auto wide = render (SourceType::PlugTouch, 1.0f);
    double sxy = 0.0, sxx = 0.0, syy = 0.0, d = 0.0;
    for (int i = 0; i < wide.getNumSamples(); ++i)
    {
        const double x = wide.getSample (0, i), y = wide.getSample (1, i);
        sxy += x * y; sxx += x * x; syy += y * y;
        d = std::max (d, std::abs (x - y));
    }
    IFX_CHECK (d > 1.0e-4);
    IFX_CHECK_MSG (sxy / std::sqrt (std::max (1.0e-30, sxx * syy)) > 0.9, "channels decorrelated by Stereo Difference");
}

//==============================================================================
// C7.3: the optional input-stage drive keeps the counterfactual buses honest: Studio off W == C,
// a healthy contact shows no fault difference or artifacts through the nonlinearity, and a fully
// open contact's fault difference is bounded by the stage's output level.
IFX_TEST (Scenarios, inputDriveKeepsCounterfactualBusesHonest)
{
    const auto in = sine (2, (int) kFs, 220.0, kFs, 0.5f);
    auto run = [&in] (SourceType t, float insert, Tap tap)
    {
        Harness h (kFs, 2, 256);
        h.params = quietParams();
        h.params[P::preOn] = 1.0f;
        h.params[P::preDrive] = 18.0f;
        h.params[P::preHeadroom] = 6.0f;
        h.params[P::jkInsert] = insert;
        h.params[P::jkWiggle] = insert < 0.5f ? 0.0f : 0.7f;
        h.params[P::jkWear] = 0.8f;
        h.params[P::ptDirt] = 0.0f;
        h.params[P::monitor] = (float) (int) tap;
        h.setSlots (t, SourceType::None);
        return h.run (in);
    };
    // Studio off: the wet bus is exactly the complete core, with the drive on
    IFX_CHECK (maxAbsDiff (run (SourceType::LooseJack, 0.8f, Tap::FullWet), run (SourceType::LooseJack, 0.8f, Tap::FullCore)) < 1.0e-7);
    // a clean pot: R and C0 are the same signal, saturated identically
    IFX_CHECK_MSG (peakOf (run (SourceType::DirtyPot, 1.0f, Tap::FaultDifference)) < 1.0e-7f, "clean pot shows a fault difference through the drive");
    IFX_CHECK (peakOf (run (SourceType::DirtyPot, 1.0f, Tap::Artifacts)) < 1.0e-7f);
    // withdrawn plug: D = sat(0) - sat(x), bounded by the stage output level
    const float bound = dbToGain (6.0f - 12.0f) / dbToGain (18.0f);
    const auto d = run (SourceType::LooseJack, 0.0f, Tap::FaultDifference);
    IFX_CHECK (allFinite (d));
    IFX_CHECK_MSG (peakOf (d) > 0.5f * bound, "withdrawn plug should lose the through signal: D peak " + juce::String (peakOf (d)));
    IFX_CHECK_MSG (peakOf (d) <= bound * 1.001f + 1.0e-6f, "D peak " + juce::String (peakOf (d)) + " above the stage level " + juce::String (bound));
}

//==============================================================================
// Research gap 5: source-impedance-aware injection. A buffered / line source holds a driven,
// exposed node down (~1 kOhm vs ~50 kOhm: about -34 dB of mains pickup for the touched tip and a
// broken shield); an open jack tip is disconnected from the source (no change); RF pickup is set
// by the cable capacitance at radio frequencies (no change).
IFX_TEST (Scenarios, sourceImpedanceScalesOnlyDrivenNodeMainsPickup)
{
    enum Case { TouchHum = 0, ShieldHum, OpenJackHum, TouchRf };
    for (int c = 0; c < 4; ++c)
    {
        double level[2] {};
        for (int mode = 0; mode < 2; ++mode)
        {
            Harness h (kFs, 2, 256);
            h.params = quietParams();
            h.params[P::inputMode] = (float) mode;
            h.params[P::hum] = c == TouchRf ? 0.0f : 1.0f;
            h.params[P::rf] = c == TouchRf ? 1.0f : 0.0f;
            h.params[P::monitor] = (float) (int) Tap::Artifacts;
            SourceType t = SourceType::PlugTouch;
            if (c == TouchHum || c == TouchRf)
            {
                h.params[P::tcTouch] = 1.0f;
                h.params[P::tcPressure] = 1.0f;
                h.params[P::tcExposure] = 1.0f;
                h.params[P::tcShield] = 0.0f;
            }
            else if (c == ShieldHum)
            {
                t = SourceType::CableHandling;
                h.params[P::cbShieldDmg] = 1.0f;
                h.params[P::cbDamage] = 0.0f;
                h.params[P::cbBend] = 0.5f;
                h.params[P::cbWeakPos] = 0.5f;
            }
            else
            {
                t = SourceType::LooseJack;
                h.params[P::jkInsert] = 0.0f;
                h.params[P::jkWiggle] = 0.0f;
                h.params[P::jkShield] = 0.0f;
                h.params[P::jkShort] = 0.0f;
            }
            h.setSlots (t, SourceType::None);
            const auto out = h.runSilence ((int) (kFs * 1.2));
            level[mode] = rmsOf (slice (out, (int) (kFs * 0.2), (int) kFs));
        }
        const double ratio = level[0] / std::max (1.0e-12, level[1]);   // Buffered / Passive
        const juce::String name = c == TouchHum ? "touched tip" : (c == ShieldHum ? "broken shield" : (c == OpenJackHum ? "open jack" : "touch RF"));
        IFX_CHECK_MSG (level[1] > 1.0e-4, name + ": no pickup with a passive source");
        // decision: the touched tip is a floating node (its impedance is Circuit Exposure), so only the
        // driven conductor behind a broken shield is held down by a buffered source
        if (c == ShieldHum)
            IFX_CHECK_MSG (ratio > 0.01 && ratio < 0.04, name + ": buffered / passive " + juce::String (ratio, 4) + " (expected ~0.02)");
        else
            IFX_CHECK_MSG (std::abs (ratio - 1.0) < 1.0e-3, name + ": should not depend on the source, ratio " + juce::String (ratio, 5));
    }
}

//==============================================================================
// R7.5 (realism spec 7): "simultaneous pops caused by one linked gesture should be correlated with
// plausible simultaneous contact transitions, not two identical layered sample impulses". One
// gesture on slot A, shared with slot B only through Linked Gesture: both slots' contact events
// and pops follow that gesture in time, but each keeps its own contacts, wear and random
// stream, so the two pop tracks are different waveforms.
IFX_TEST (Scenarios, linkedGesturePopsAreCorrelatedNotLayeredCopies)
{
    const int block = 64, n = (int) (kFs * 6.0);
    auto gestureStart = [] (int k) { return (int) ((0.1 + 1.0 * k) * kFs); };  // one Wiggle per second
    auto inWindow = [&] (int sample)
    {
        for (int k = 0; k < 6; ++k)
            if (sample >= gestureStart (k) && sample < gestureStart (k) + (int) (0.5 * kFs)) return true;
        return false;
    };
    struct Run { juce::AudioBuffer<float> pops; std::vector<int> evA, evB; };
    auto render = [&] (bool link, float jackPop, float potBias)
    {
        Run r;
        Harness h (kFs, 1, block);
        h.params = quietParams();
        h.params[P::linkGesture] = link ? 1.0f : 0.0f;
        h.params[P::gestureA] = 0.0f;              // Wiggle
        h.params[P::jkInsert] = 0.75f;             // plug part way in, over its worn zone
        h.params[P::jkWearPos] = 0.75f;
        h.params[P::jkWear] = 0.8f;
        h.params[P::jkWiggle] = 0.0f;              // moves only when the gesture moves it
        h.params[P::jkIntermit] = 0.0f;
        h.params[P::jkPressure] = 0.3f;
        h.params[P::jkPop] = jackPop;
        h.params[P::ptTaper] = 0.0f;               // linear: the jack's pops pass the pot at half level
        h.params[P::ptTurn] = 0.5f;                // knob resting on its worn arc
        h.params[P::ptWearPos] = 0.5f;
        h.params[P::ptWearWidth] = 0.2f;
        h.params[P::ptDirt] = 0.8f;
        h.params[P::ptBias] = potBias;
        h.params[P::monitor] = (float) (int) Tap::Artifacts;
        h.setSlots (SourceType::LooseJack, SourceType::DirtyPot);
        for (int k = 0; k < 6; ++k)
            h.pendingEvents.push_back ({ gestureStart (k), ControlEvent::GestureA, (int) GestureKind::Wiggle, 1.0f });
        r.pops.setSize (1, n);
        uint32_t lastA = 0, lastB = 0;
        for (int pos = 0; pos < n; pos += block)
        {
            const auto o = h.runSilence (block);
            r.pops.copyFrom (0, pos, o, 0, 0, block);
            const auto& t = h.engine->telemetry;
            const uint32_t a = t.slot[0].openEvents.load() + t.slot[0].resistiveEvents.load();
            const uint32_t b = t.slot[1].openEvents.load() + t.slot[1].resistiveEvents.load();
            for (uint32_t e = lastA; e < a; ++e) r.evA.push_back (pos);
            for (uint32_t e = lastB; e < b; ++e) r.evB.push_back (pos);
            lastA = a;
            lastB = b;
        }
        return r;
    };
    auto shareInWindows = [&] (const std::vector<int>& ev)
    {
        int in = 0;
        for (int s : ev) if (inWindow (s)) ++in;
        return ev.empty() ? 0.0 : (double) in / (double) ev.size();
    };
    auto envelope = [] (const juce::AudioBuffer<float>& b)
    {
        std::vector<double> e ((size_t) b.getNumSamples());
        const double a = 1.0 - std::exp (-1.0 / (0.02 * kFs));
        double z = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i) { z += (std::abs ((double) b.getSample (0, i)) - z) * a; e[(size_t) i] = z; }
        return e;
    };
    auto pearson = [] (const std::vector<double>& x, const std::vector<double>& y)
    {
        double mx = 0, my = 0;
        for (size_t i = 0; i < x.size(); ++i) { mx += x[i]; my += y[i]; }
        mx /= (double) x.size(); my /= (double) y.size();
        double sxy = 0, sxx = 0, syy = 0;
        for (size_t i = 0; i < x.size(); ++i) { sxy += (x[i] - mx) * (y[i] - my); sxx += (x[i] - mx) * (x[i] - mx); syy += (y[i] - my) * (y[i] - my); }
        return sxy / std::sqrt (std::max (1.0e-30, sxx * syy));
    };

    // without the link, the gesture on slot A never reaches slot B: its pot stays still and silent
    const auto unlinked = render (false, 0.0f, 1.0f);
    IFX_CHECK_MSG (unlinked.evB.empty(), juce::String ((int) unlinked.evB.size()) + " pot events without a link");
    IFX_CHECK (peakOf (unlinked.pops) < 1.0e-7f);

    // linked: pops from each slot isolated by giving only that slot a bias (contacts are unchanged)
    const auto jackPops = render (true, 1.0f, 0.0f), potPops = render (true, 0.0f, 1.0f);
    IFX_CHECK_MSG (! jackPops.evA.empty() && ! jackPops.evB.empty(),
                   "events: jack " + juce::String ((int) jackPops.evA.size()) + ", pot " + juce::String ((int) jackPops.evB.size()));
    IFX_CHECK (jackPops.evA == potPops.evA && jackPops.evB == potPops.evB);   // bias never changes contact state
    IFX_CHECK_MSG (shareInWindows (jackPops.evA) >= 0.9 && shareInWindows (jackPops.evB) >= 0.9,
                   "events outside the gesture: jack " + juce::String (shareInWindows (jackPops.evA), 2)
                       + ", pot " + juce::String (shareInWindows (jackPops.evB), 2));
    IFX_CHECK (peakOf (jackPops.pops) > 1.0e-4f && peakOf (potPops.pops) > 1.0e-4f);

    // correlated in time (both follow the one gesture) ...
    const double envCorr = pearson (envelope (jackPops.pops), envelope (potPops.pops));
    IFX_CHECK_MSG (envCorr > 0.3, "pop envelopes correlate only " + juce::String (envCorr, 3));
    // ... but not the same impulses layered twice: different waveforms, different event timing
    std::vector<double> xa, xb;
    for (int i = 0; i < n; ++i) { xa.push_back (jackPops.pops.getSample (0, i)); xb.push_back (potPops.pops.getSample (0, i)); }
    const double waveCorr = pearson (xa, xb);
    IFX_CHECK_MSG (std::abs (waveCorr) < 0.9, "pop waveforms are near copies: r = " + juce::String (waveCorr, 3));
    int common = 0;
    for (int s : jackPops.evA)
        if (std::find (jackPops.evB.begin(), jackPops.evB.end(), s) != jackPops.evB.end()) ++common;
    const double overlap = (double) common / (double) std::max<size_t> (1, jackPops.evA.size() + jackPops.evB.size() - (size_t) common);
    IFX_CHECK_MSG (overlap < 0.9, "event timing is identical in both slots (overlap " + juce::String (overlap, 2) + ")");
}
