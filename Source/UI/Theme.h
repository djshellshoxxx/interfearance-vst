#pragma once

#include <JuceHeader.h>

namespace ifx
{

/** theme.md visual identity. Neutrals, layout and control shapes are shared across the
    user's plugins; Interference keeps the default burnt-orange primary accent. */
namespace Theme
{
    inline const juce::Colour background   { 0xff0e1116 };
    inline const juce::Colour panel        { 0xff171b22 };
    inline const juce::Colour edge         { 0xff2a303a };
    inline const juce::Colour accent       { 0xffe8532a };
    inline const juce::Colour accent2      { 0xff4fb6c4 };
    inline const juce::Colour text         { 0xffe6e8ec };
    inline const juce::Colour textMuted    { 0xff8a929e };
    inline const juce::Colour success      { 0xff7bc96f };
    inline const juce::Colour warning      { 0xfff2c14e };
    inline const juce::Colour clip         { 0xffe5484d };
    inline const juce::Colour shadow       { 0x8c000000 };
    inline const juce::Colour knobTop      { 0xff232833 };
    inline const juce::Colour knobBottom   { 0xff14181f };
    inline const juce::Colour streamGreen  { 0xff7bc96f };

    // spacing (8 px grid)
    constexpr int grid = 8;
    constexpr int headerHeight = 32;
    constexpr int windowRadius = 6;
    constexpr int panelRadius = 4;
    constexpr int controlRadius = 2;

    juce::Font labelFont (float size = 11.0f);     // uppercase labels, weight 500
    juce::Font headerFont (float size = 12.0f);    // section headers, weight 600
    juce::Font valueFont (float size = 13.0f);     // value readouts (mono)
    juce::Font monoFont (float size = 11.0f);
    juce::Font bodyFont (float size = 13.0f);

    /** Draws an uppercase label with +0.08 em tracking. */
    void drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                    juce::Justification j = juce::Justification::centred, juce::Colour c = textMuted, float size = 11.0f);

    /** Section header: uppercase text with the 2x12 px accent bar on the left. */
    void drawSectionHeader (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area);

    /** Soft drop shadow (rgba(0,0,0,.55), 8 px blur, y + 2). */
    void drawShadow (juce::Graphics& g, juce::Rectangle<float> r, float radius);
}

class InterferenceLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    InterferenceLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
};

} // namespace ifx
