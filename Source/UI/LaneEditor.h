#pragma once

#include "Widgets.h"
#include <deque>

namespace ifx
{

/** The step grid of the Gesture Lane / Machine Pattern (two lanes, 1-64 steps each).
    Mouse: click toggles a step, drag up/down sets intensity, right-click edits a step.
    Keyboard: arrows move, Space/Enter toggle, +/- intensity, A accent, R ratchet,
    G gesture, P probability, C choke, Ctrl+C / Ctrl+V copy / paste a step,
    Shift+Left/Right shift the lane, Ctrl+Shift+Left/Right rotate, Delete clear step,
    Ctrl+Delete clear lane, Ctrl+Z / Ctrl+Y undo / redo. */
class StepGrid final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (InterferenceProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

    void setPlayheads (int a, int b);
    void refreshFromProcessor();
    void undo();
    void redo();
    void edit (const std::function<void (Pattern&)>& change);

    int cursorLane = 0, cursorStep = 0;

private:
    juce::Rectangle<float> cellBounds (int lane, int step) const;
    bool hitCell (juce::Point<float> p, int& lane, int& step) const;
    void showStepMenu (int lane, int step);
    int visibleSteps() const;
    juce::String gestureLabel (int lane, int g) const;

    InterferenceProcessor& proc;
    Pattern pattern;
    std::deque<Pattern> undoStack, redoStack;
    int playhead[2] { -1, -1 };
    LaneStep clipboard;
    bool haveClipboard = false;
    int dragLane = -1, dragStep = -1;
    float dragStartIntensity = 0.0f;
    bool dragMoved = false;
};

/** Gesture Lane panel: grid + lane lengths, mute / solo, division, swing, humanize,
    density, evolve, Fill, level, send, internal clock. */
class LaneEditor final : public juce::Component
{
public:
    explicit LaneEditor (InterferenceProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh();

private:
    InterferenceProcessor& proc;
    StepGrid grid;
    ParamToggle laneOn, run;
    ParamChoice division;
    ParamKnob swing, humanize, density, evolve, level, send, bpm, chaos;
    MomentaryParamButton fill;
    juce::TextButton lenMinus[2], lenPlus[2], mute[2], solo[2], clearLane[2];
    juce::TextButton undoB { "Undo" }, redoB { "Redo" };
    juce::String clockText;
};

} // namespace ifx
