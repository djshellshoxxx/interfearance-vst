#pragma once

#include "Widgets.h"

namespace ifx
{

/** Studio chain: source (Artifacts / Breaks / Both), seven fixed blocks with independent
    on/off + wet, the local feedback send and Raw Hardware. Everything here is labelled
    Studio processing, after the modeled hardware. */
class StudioPanel final : public juce::Component
{
public:
    explicit StudioPanel (InterferenceProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh();

private:
    void select (int stage);
    InterferenceProcessor& proc;
    ParamToggle studioOn, rawHw, keepExc;
    ParamChoice source;
    ParamKnob level;
    static constexpr int kStages = 8; // 7 blocks + Feedback
    std::unique_ptr<ParamToggle> stageOn[kStages];
    juce::TextButton stageEdit[kStages];
    juce::OwnedArray<juce::Component> controls[kStages];
    int selected = 0;
    juce::String note;
};

/** Named buses R, D, A, C, P, W, x, y with live meters, sidechain status, latency / tail and
    the capture tap. Reflects the engine's actual route choices. */
class RoutingView final : public juce::Component
{
public:
    explicit RoutingView (InterferenceProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh();

private:
    InterferenceProcessor& proc;
    LevelMeter meters[8];
    ParamChoice monitor;
    juce::ComboBox captureTap;
    juce::String status;
};

/** Create mode: internal excitation, one-shot playback, offline render and live record. */
class CreatePanel final : public juce::Component
{
public:
    explicit CreatePanel (InterferenceProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    std::function<void()> onRender, onRecord;
    void refresh();

private:
    InterferenceProcessor& proc;
    ParamChoice excType;
    ParamKnob excLevel;
    ParamToggle keepExc, laneOn;
    juce::TextButton playOnce { "Play one-shot pattern" }, render { "Render WAV..." }, record { "Record" };
    juce::String recText;
};

/** Output strip: Dry/Wet, output trim, monitor tap, ceiling, bypass, meters, record status. */
class OutputStrip final : public juce::Component
{
public:
    explicit OutputStrip (InterferenceProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh();

private:
    InterferenceProcessor& proc;
    ParamKnob mix, out, dryTrim, ceiling;
    ParamToggle ceilOn, bypass;
    ParamChoice monitor, mixLaw;
    LevelMeter inMeter { "in" }, outMeter { "out" };
    juce::String ioText;
};

} // namespace ifx
