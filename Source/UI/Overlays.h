#pragma once

#include "Widgets.h"
#include "HelpContent.h"
#include <thread>

namespace ifx
{

/** Base for full-window overlays: dimmed backdrop, panel, Close button, Esc closes. */
class Overlay : public juce::Component
{
public:
    explicit Overlay (const juce::String& title);
    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;
    void mouseDown (const juce::MouseEvent& e) override;
    std::function<void()> onClose;
    void close();

protected:
    virtual void layoutContent (juce::Rectangle<int> area) = 0;
    juce::Rectangle<int> panelBounds() const;
    juce::String title;
    juce::TextButton closeButton { "Close" };
};

class HelpOverlay final : public Overlay
{
public:
    HelpOverlay();
    std::function<void()> onDebug;
    void showSection (int index);

private:
    void layoutContent (juce::Rectangle<int> area) override;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::TextEditor text;
    juce::TextButton debug { "Debug..." };
    juce::HyperlinkButton home, mail;
};

class OptionsOverlay final : public Overlay, private juce::Timer
{
public:
    explicit OptionsOverlay (InterferenceProcessor& p);
    ~OptionsOverlay() override;

private:
    void layoutContent (juce::Rectangle<int> area) override;
    void timerCallback() override;
    void rebuildMappings();
    InterferenceProcessor& proc;
    juce::ToggleButton tooltips { "Show tooltips" };
    juce::TextButton audioSettings { "Audio / MIDI devices..." }, openPresets { "Open preset folder in Explorer" },
                     openSettings { "Open settings folder" }, clearAll { "Clear all MIDI mappings" };
    juce::Label deviceNote, mapTitle;
    juce::TextEditor mappings;
    int lastMapCount = -1;
};

/** Offline render dialog: options, background render with progress / cancel, silent-result
    warning, file chooser (no overwrite without confirmation), success popup. */
class RenderOverlay final : public Overlay, private juce::Timer
{
public:
    explicit RenderOverlay (InterferenceProcessor& p);
    ~RenderOverlay() override;

private:
    void layoutContent (juce::Rectangle<int> area) override;
    void paint (juce::Graphics& g) override;
    void timerCallback() override;
    void startRender();
    void finished();
    void chooseFileAndWrite();
    RenderSettings gather() const;

    InterferenceProcessor& proc;
    juce::ComboBox lengthMode, bars, rate, depth, channels, tap;
    juce::Slider seconds, bpm, tail, headroom;
    juce::ToggleButton normalize { "Normalize peak to -headroom dB" }, sidecar { "Write a sidecar .txt (seed, tempo, sources)" };
    juce::TextButton go { "Render..." }, cancel { "Cancel render" };
    juce::String status;
    std::atomic<bool> cancelFlag { false }, done { false };
    std::atomic<float> progress { 0.0f };
    std::unique_ptr<std::thread> worker;
    RenderResult result;
    RenderSettings settings;
    PluginState snapshot;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::OwnedArray<juce::Label> captions;
};

} // namespace ifx
