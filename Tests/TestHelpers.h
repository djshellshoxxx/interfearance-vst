#pragma once

#include "TestFramework.h"
#include "DSP/Engine.h"
#include <memory>
#include <vector>

namespace ifxtest
{

/** A parameter snapshot with every environmental / excitation source switched off:
    the realism spec's "silence" configuration. */
inline ifx::ParamSnapshot quietParams()
{
    ifx::ParamSnapshot p;
    p[ifx::P::hum] = 0.0f;
    p[ifx::P::buzz] = 0.0f;
    p[ifx::P::rf] = 0.0f;
    p[ifx::P::excType] = 0.0f;
    p[ifx::P::jkPop] = 0.0f;
    p[ifx::P::tcPop] = 0.0f;
    p[ifx::P::tcTouch] = 0.0f;
    p[ifx::P::cbMic] = 0.0f;
    p[ifx::P::ptBias] = 0.0f;
    return p;
}

/** Runs an Engine over generated input in blocks of a chosen size. */
struct Harness
{
    std::unique_ptr<ifx::Engine> engine = std::make_unique<ifx::Engine>();
    double fs = 48000.0;
    int channels = 2;
    int block = 256;
    ifx::ParamSnapshot params;
    ifx::TransportInfo transport;
    std::vector<ifx::ControlEvent> pendingEvents; // offsets are absolute sample indices
    ifx::Tap captureTap = ifx::Tap::FullOutput;
    juce::AudioBuffer<float> capture;

    Harness (double sampleRate = 48000.0, int numChannels = 2, int blockSize = 256)
        : fs (sampleRate), channels (numChannels), block (blockSize)
    {
        engine->prepare (fs, blockSize, numChannels);
        engine->setSeed (12345);
    }

    void setSlots (ifx::SourceType a, ifx::SourceType b)
    {
        engine->setSlotTypes (a, b);
        engine->applyPendingNow();
    }

    void publish (const ifx::Pattern& p)
    {
        engine->patternExchange().publish (p);
        engine->applyPendingNow();
    }

    /** Process `input` (numChannels x N) and return the output. Transport advances if playing. */
    juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>* sidechain = nullptr,
                                  bool wantCapture = false)
    {
        const int n = input.getNumSamples();
        juce::AudioBuffer<float> out (channels, n);
        out.clear();
        if (wantCapture) { capture.setSize (channels, n); capture.clear(); }
        int pos = 0;
        std::vector<ifx::ControlEvent> ev;
        while (pos < n)
        {
            const int len = std::min (block, n - pos);
            const float* in[2] = { input.getReadPointer (0, pos), input.getReadPointer (std::min (1, input.getNumChannels() - 1), pos) };
            float* o[2] = { out.getWritePointer (0, pos), out.getWritePointer (std::min (1, channels - 1), pos) };
            const float* sc[2] = { nullptr, nullptr };
            int scCh = 0;
            if (sidechain != nullptr)
            {
                sc[0] = sidechain->getReadPointer (0, pos);
                sc[1] = sidechain->getReadPointer (std::min (1, sidechain->getNumChannels() - 1), pos);
                scCh = sidechain->getNumChannels();
            }
            ev.clear();
            for (auto& e : pendingEvents)
                if (e.offset >= absolute + pos && e.offset < absolute + pos + len)
                {
                    auto c = e;
                    c.offset -= absolute + pos;
                    ev.push_back (c);
                }
            float* cap[2] = { nullptr, nullptr };
            if (wantCapture)
            {
                cap[0] = capture.getWritePointer (0, pos);
                cap[1] = capture.getWritePointer (std::min (1, channels - 1), pos);
            }
            engine->process (in, o, len, params, transport, ev.data(), (int) ev.size(), sidechain ? sc : nullptr, scCh,
                             wantCapture ? cap : nullptr, captureTap);
            if (transport.playing)
            {
                transport.ppq += len * transport.bpm / 60.0 / fs;
                transport.hostSeconds += len / fs;
            }
            pos += len;
        }
        absolute += n;
        return out;
    }

    juce::AudioBuffer<float> runSilence (int n)
    {
        juce::AudioBuffer<float> in (channels, n);
        in.clear();
        return run (in);
    }

    int64_t absolute = 0;
};

inline juce::AudioBuffer<float> sine (int channels, int n, double freq, double fs, float amp = 0.5f)
{
    juce::AudioBuffer<float> b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / fs));
    return b;
}

inline juce::AudioBuffer<float> noise (int channels, int n, uint64_t seed, float amp = 0.3f)
{
    ifx::Rng r (seed);
    juce::AudioBuffer<float> b (channels, n);
    for (int i = 0; i < n; ++i)
    {
        const float v = (float) r.bipolar() * amp;
        for (int ch = 0; ch < channels; ++ch)
            b.setSample (ch, i, ch == 0 ? v : v * 0.8f + 0.2f * (float) r.bipolar() * amp);
    }
    return b;
}

/** Simple drum-like test material: decaying noise bursts on each beat + a low tone. */
inline juce::AudioBuffer<float> drums (int channels, int n, double fs, uint64_t seed = 7)
{
    ifx::Rng r (seed);
    juce::AudioBuffer<float> b (channels, n);
    const int beat = (int) (fs * 0.5);
    for (int i = 0; i < n; ++i)
    {
        const int t = i % beat;
        const float env = std::exp (-(float) t / (float) (fs * 0.04));
        const float body = 0.4f * std::exp (-(float) t / (float) (fs * 0.12)) * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * t / fs);
        const float v = env * (float) r.bipolar() * 0.5f + body;
        for (int ch = 0; ch < channels; ++ch)
            b.setSample (ch, i, v);
    }
    return b;
}

inline double maxAbsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double m = 0.0;
    for (int ch = 0; ch < std::min (a.getNumChannels(), b.getNumChannels()); ++ch)
        for (int i = 0; i < std::min (a.getNumSamples(), b.getNumSamples()); ++i)
            m = std::max (m, (double) std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
    return m;
}

inline juce::AudioBuffer<float> slice (const juce::AudioBuffer<float>& b, int start, int len)
{
    juce::AudioBuffer<float> s (b.getNumChannels(), len);
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        s.copyFrom (ch, 0, b, ch, start, len);
    return s;
}

inline const ifx::SourceType kAllSources[4] = { ifx::SourceType::LooseJack, ifx::SourceType::PlugTouch,
                                               ifx::SourceType::CableHandling, ifx::SourceType::DirtyPot };

} // namespace ifxtest
