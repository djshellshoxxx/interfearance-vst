#pragma once

#include "Widgets.h"

namespace ifx
{

/** Small per-stage meter: block peaks of one Studio stage's input (thin muted bar) and
    output (level-coloured bar), with the same fall rate as LevelMeter. */
class StageMeter final : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setLevels (float inPeak, float outPeak) noexcept; // linear, from a UI timer
    void paint (juce::Graphics& g) override;

private:
    float inDb = -100.0f, outDb = -100.0f;
};

/** Studio chain: source (Artifacts / Breaks / Both), seven fixed blocks with independent
    on/off + wet and an input/output meter each, the local feedback send and Raw Hardware.
    Everything here is labelled Studio processing, after the modeled hardware. */
class StudioPanel final : public juce::Component
{
public:
    explicit StudioPanel (InterferenceProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh();

private:
    void select (int stage);
    /** Greys out controls that do nothing in the current setting, with the reason (values kept). */
    void updateDormancy();
    InterferenceProcessor& proc;
    ParamToggle studioOn, rawHw, keepExc;
    ParamChoice source;
    ParamKnob level;
    static constexpr int kStages = 8; // 7 blocks + Feedback
    std::unique_ptr<ParamToggle> stageOn[kStages];
    juce::TextButton stageEdit[kStages];
    StageMeter stageMeter[kStages - 1]; // the seven blocks (Feedback has no stage meter)
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
    juce::String status, orderLine, feedbackLine;
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

/** Environment tab: the rig's electrical surroundings, shared by both slots. The mains field
    (picked up by exposed paths), the Buzz Band from a supply or a light dimmer, RF interference,
    a ground loop through the cable shield with the receiving interface, and a nearby 2G phone.
    Controls that do nothing in the current setting are greyed with a reason; the ground-loop
    readout is the engine's measured hum after the interface. */
class EnvironmentPanel final : public juce::Component
{
public:
    explicit EnvironmentPanel (InterferenceProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh();

private:
    InterferenceProcessor& proc;
    ParamChoice mains, buzzType, dimKind, rfType, phone, gear, outlets, shield, iface;
    ParamKnob hum, buzz, dimmer, rf, phoneDist, run;
    juce::Label groundReadout; // measured loop hum after the interface (a Label: screen readers see it)
    juce::Rectangle<int> fieldHeader, phoneHeader, loopHeader;
    int phoneDividerX = 0, bandBottom = 0; // 1 px separators between the groups (theme.md: lines, not boxes)
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
