#pragma once

#include "Theme.h"
#include "Plugin/PluginProcessor.h"
#include <deque>
#include <limits>

namespace ifx
{

/** Right-click menu shared by every parameter control (include.md): MIDI learn / clear,
    reset to default, type an exact value, plus the host automation name. */
void showParamMenu (InterferenceProcessor& proc, int paramIndex, juce::Component& target);

/** Asynchronous "type a value" box for a parameter. */
void promptParamValue (InterferenceProcessor& proc, int paramIndex, juce::Component& target);

/** Tooltip text for a parameter: its audible description + MIDI mapping if any. */
juce::String paramTooltip (InterferenceProcessor& proc, int paramIndex);

/** Slider with theme.md drag feel: vertical drag, Shift = coarse, Ctrl/Cmd = ultra-fine,
    double-click = default, right-click = parameter menu. */
class FineSlider : public juce::Slider
{
public:
    FineSlider() = default;
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
private:
    bool rightDown = false;
};

/** Knob (or horizontal slider) bound to one parameter by attachment: no duplicated state. */
class ParamKnob : public juce::Component, private juce::Timer
{
public:
    enum class Style { Knob, SmallKnob, Horizontal };
    ParamKnob (InterferenceProcessor& p, int paramIndex, Style style = Style::Knob, const juce::String& labelOverride = {});
    ~ParamKnob() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    /** Grey the control out with a short reason (realism spec 9): value stays saved. */
    void setDormant (bool dormant, const juce::String& reason = {});
    /** Show a short "Limited by ..." reason under the value readout and in the tooltip
        (empty = not limited). The stored value is never changed. */
    void setLimited (const juce::String& why);
    /** Mark the effective (clamped) value on the arc, in real units; NaN clears it. */
    void setEffective (float realValue);

    int getParamIndex() const noexcept { return index; }
    FineSlider slider;

private:
    void timerCallback() override;
    InterferenceProcessor& proc;
    int index;
    Style style;
    juce::String label;
    juce::String dormantReason, limitedReason;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool showValue = false;
    float effectiveValue = std::numeric_limits<float>::quiet_NaN();
};

/** Toggle button bound to a Bool parameter. */
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (InterferenceProcessor& p, int paramIndex, const juce::String& text = {});
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    int getParamIndex() const noexcept { return index; }
    /** Grey the toggle out with a short reason (value stays saved), like ParamKnob::setDormant. */
    void setDormant (bool dormant, const juce::String& reason = {});
private:
    InterferenceProcessor& proc;
    int index;
    bool menuClick = false;
    juce::String dormantReason;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

/** A plain TextButton (it keeps its own onClick / onStateChange behaviour) whose right-click
    opens the menu of a related parameter (MIDI learn, reset, type a value) instead of pressing
    it, e.g. the mode buttons (Mode) and the slot Play buttons (Trigger A / B). */
class ParamMenuButton : public juce::TextButton
{
public:
    ParamMenuButton (InterferenceProcessor& p, int paramIndex, const juce::String& text = {});
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    int getParamIndex() const noexcept { return index; }
private:
    InterferenceProcessor& proc;
    int index;
    bool menuClick = false;
};

/** Momentary button bound to a Bool parameter (true while held), e.g. Fill / Kill. */
class MomentaryParamButton : public juce::TextButton
{
public:
    MomentaryParamButton (InterferenceProcessor& p, int paramIndex, const juce::String& text);
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;
    bool keyStateChanged (bool isKeyDown) override;
private:
    void set (bool on);
    InterferenceProcessor& proc;
    int index;
    bool held = false;
};

/** Combo box bound to a Choice parameter, with an optional caption drawn above. */
class ParamChoice : public juce::Component
{
public:
    ParamChoice (InterferenceProcessor& p, int paramIndex, const juce::String& caption = {});
    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    /** Grey the choice out with a short reason (value stays saved), like ParamKnob::setDormant. */
    void setDormant (bool dormant, const juce::String& reason = {});
    juce::ComboBox box;
    int getParamIndex() const noexcept { return index; }
private:
    InterferenceProcessor& proc;
    int index;
    juce::String caption, dormantReason;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

/** theme.md meter: smooth segmented gradient, 1.5 s peak hold then -20 dB/s, mono numeric peak. */
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (const juce::String& name = {}, bool vertical = true);
    void setLevel (float linearPeak) noexcept;   // call from a UI timer
    void paint (juce::Graphics& g) override;
    void setCaption (const juce::String& c) { caption = c; repaint(); }
private:
    static float toPos (float db) noexcept { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); }
    juce::String caption;
    bool vertical;
    float levelDb = -100.0f, peakDb = -100.0f;
    double peakTime = 0.0;
};

/** The top-left "LED": gray at -inf, brighter towards 0 dB, red above 0 dBFS. */
class OutputLed : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setLevelDb (float db) noexcept;
    void paint (juce::Graphics& g) override;
private:
    float db = -120.0f;
};

/** Rolling stream of real internal events (theme.md "matrix" text): light green, faded top and
    bottom two lines, scrolls only while there is new data. */
class DataStream : public juce::Component, private juce::Timer
{
public:
    DataStream();
    void addLines (const juce::StringArray& lines);
    void paint (juce::Graphics& g) override;
    int numVisibleLines = 10;
private:
    void timerCallback() override;
    std::deque<juce::String> lines;
    std::deque<juce::String> pending;
    float scroll = 0.0f;
};

/** A small momentary accent flash helper for buttons (100 ms). */
void flash (juce::Component& c);

} // namespace ifx
