#include "Theme.h"

namespace ifx
{

namespace
{
    juce::String pickFamily (std::initializer_list<const char*> wanted, const juce::String& fallback)
    {
        static const juce::StringArray installed = juce::Font::findAllTypefaceNames();
        for (auto* w : wanted)
            if (installed.contains (w, true))
                return w;
        return fallback;
    }

    const juce::String& sansFamily()
    {
        static const juce::String f = pickFamily ({ "Inter", "Space Grotesk", "Segoe UI", "Helvetica Neue", "Arial" }, juce::Font::getDefaultSansSerifFontName());
        return f;
    }

    const juce::String& monoFamily()
    {
        static const juce::String f = pickFamily ({ "JetBrains Mono", "IBM Plex Mono", "Cascadia Mono", "Consolas", "Menlo" }, juce::Font::getDefaultMonospacedFontName());
        return f;
    }

    juce::Font make (const juce::String& family, float size, bool bold)
    {
        return juce::Font (juce::FontOptions (family, size, bold ? juce::Font::bold : juce::Font::plain));
    }
}

namespace Theme
{
    juce::Font labelFont (float size)  { return make (sansFamily(), size, false); }
    juce::Font headerFont (float size) { return make (sansFamily(), size, true); }
    juce::Font valueFont (float size)  { return make (monoFamily(), size, false); }
    juce::Font monoFont (float size)   { return make (monoFamily(), size, false); }
    juce::Font bodyFont (float size)   { return make (sansFamily(), size, false); }

    void drawLabel (juce::Graphics& g, const juce::String& label, juce::Rectangle<int> area, juce::Justification j, juce::Colour c, float size)
    {
        auto f = labelFont (size);
        f.setExtraKerningFactor (0.08f);
        g.setFont (f);
        g.setColour (c);
        g.drawFittedText (label.toUpperCase(), area, j, 1, 0.8f);
    }

    void drawSectionHeader (juce::Graphics& g, const juce::String& title, juce::Rectangle<int> area)
    {
        g.setColour (accent);
        g.fillRect (juce::Rectangle<float> ((float) area.getX(), (float) area.getCentreY() - 6.0f, 2.0f, 12.0f));
        auto f = headerFont (11.0f);
        f.setExtraKerningFactor (0.08f);
        g.setFont (f);
        g.setColour (Theme::text);
        g.drawText (title.toUpperCase(), area.withTrimmedLeft (8), juce::Justification::centredLeft, true);
    }

    void drawShadow (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f / 8.0f * (float) (5 - i)));
            g.fillRoundedRectangle (r.translated (0.0f, 2.0f).expanded ((float) i * 2.0f - 2.0f), radius + (float) i);
        }
    }
}

//==============================================================================
InterferenceLookAndFeel::InterferenceLookAndFeel()
{
    using namespace Theme;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, panel);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, textMuted);
    setColour (juce::TextButton::textColourOnId, accent);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, edge);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textMuted);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.2f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textMuted);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0101319));
    setColour (juce::TooltipWindow::textColourId, textMuted);
    setColour (juce::TooltipWindow::outlineColourId, edge);
    setColour (juce::TextEditor::backgroundColourId, background);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, edge);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ScrollBar::thumbColourId, edge);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, edge);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ListBox::backgroundColourId, background);
    setColour (juce::ListBox::textColourId, text);
    setColour (juce::DocumentWindow::textColourId, text);
    setColour (juce::FileBrowserComponent::currentPathBoxBackgroundColourId, panel);
    setColour (juce::FileBrowserComponent::filenameBoxBackgroundColourId, panel);
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, accent.withAlpha (0.3f));
    setDefaultSansSerifTypeface (nullptr);
}

void InterferenceLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    using namespace Theme;
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float arcR = size * 0.5f - 2.0f;           // value arc outside the body
    const float bodyR = arcR - 3.0f - 4.0f;          // 4 px gap + 3 px arc
    const bool hover = s.isMouseOverOrDragging();
    const bool enabled = s.isEnabled();
    const float angle = start + pos * (end - start);

    // unfilled + filled value arc (270 deg sweep)
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (edge.withAlpha (0.6f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? start + (float) s.valueToProportionOfLength (0.0) * (end - start) : start;
    juce::Path fill;
    fill.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
    g.setColour ((enabled ? accent : textMuted).withMultipliedBrightness (hover ? 1.08f : 1.0f));
    g.strokePath (fill, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    juce::ColourGradient grad (knobTop.withMultipliedBrightness (hover ? 1.08f : 1.0f), centre.x, body.getY(),
                               knobBottom, centre.x, body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (edge);
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    // indicator: 2 px accent line centre -> rim, rounded cap
    const juce::Point<float> tip (centre.x + std::sin (angle) * (bodyR - 3.0f), centre.y - std::cos (angle) * (bodyR - 3.0f));
    g.setColour (enabled ? accent : textMuted);
    g.drawLine ({ centre, tip }, 2.0f);
    g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (tip));

    // centre dot: accent when away from default, muted at default
    const bool atDefault = std::abs (s.getValue() - s.getDoubleClickReturnValue()) < 1e-6;
    g.setColour (atDefault ? textMuted : accent);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (centre));
}

void InterferenceLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                                juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace Theme;
    const bool vertical = style == juce::Slider::LinearVertical;
    const auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    juce::Rectangle<float> trackR = vertical ? juce::Rectangle<float> (r.getCentreX() - 2.0f, r.getY(), 4.0f, r.getHeight())
                                             : juce::Rectangle<float> (r.getX(), r.getCentreY() - 2.0f, r.getWidth(), 4.0f);
    g.setColour (edge);
    g.fillRoundedRectangle (trackR, 2.0f);
    // theme.md: 1 px muted ticks every 1/8 of the travel, outside the track
    g.setColour (textMuted.withAlpha (0.6f));
    for (int t = 0; t <= 8; ++t)
    {
        const float f = (float) t / 8.0f;
        if (vertical)
        {
            const float ty = r.getBottom() - f * r.getHeight();
            g.fillRect (trackR.getRight() + 3.0f, ty - 0.5f, 3.0f, 1.0f);
        }
        else
        {
            const float tx = r.getX() + f * r.getWidth();
            g.fillRect (tx - 0.5f, trackR.getBottom() + 3.0f, 1.0f, 3.0f);
        }
    }
    juce::Rectangle<float> fillR = vertical ? trackR.withTop (pos) : trackR.withRight (pos);
    g.setColour (s.isEnabled() ? accent : textMuted);
    g.fillRoundedRectangle (fillR, 2.0f);
    // thumb 16x24 (rotated for horizontal)
    const auto thumb = vertical ? juce::Rectangle<float> (24.0f, 16.0f).withCentre ({ r.getCentreX(), pos })
                                : juce::Rectangle<float> (16.0f, std::min (24.0f, r.getHeight())).withCentre ({ pos, r.getCentreY() });
    juce::ColourGradient grad (knobTop.withMultipliedBrightness (s.isMouseOverOrDragging() ? 1.08f : 1.0f), thumb.getCentreX(), thumb.getY(),
                               knobBottom, thumb.getCentreX(), thumb.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (s.isEnabled() ? accent : edge);
    g.drawRoundedRectangle (thumb.reduced (0.5f), 3.0f, 1.0f);
}

void InterferenceLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace Theme;
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto base = panel;
    if (on) base = accent.withAlpha (0.15f).overlaidWith (panel.withAlpha (0.0f));
    if (over) base = base.brighter (0.08f);
    if (down) base = accent.withAlpha (0.35f);
    g.setColour (on ? panel : panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent : edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    if (b.hasKeyboardFocus (true))
    {
        g.setColour (accent2.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (1.5f), 3.0f, 1.0f);
    }
}

void InterferenceLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    using namespace Theme;
    auto f = labelFont (11.0f);
    f.setExtraKerningFactor (0.06f);
    g.setFont (f);
    g.setColour (! b.isEnabled() ? edge : b.getToggleState() ? accent : (b.isMouseOver() ? text : textMuted));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.7f);
}

void InterferenceLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    using namespace Theme;
    const auto r = b.getLocalBounds();
    const auto box = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ (float) r.getX() + 9.0f, (float) r.getCentreY() });
    g.setColour (b.getToggleState() ? accent.withAlpha (0.25f) : panel);
    g.fillRoundedRectangle (box, 2.0f);
    g.setColour (b.getToggleState() ? accent : (over ? textMuted : edge));
    g.drawRoundedRectangle (box, 2.0f, 1.0f);
    if (b.getToggleState())
        g.fillRoundedRectangle (box.reduced (4.0f), 1.0f);
    g.setFont (bodyFont (12.0f));
    g.setColour (b.isEnabled() ? text : textMuted);
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (22), juce::Justification::centredLeft, 2);
}

void InterferenceLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c)
{
    using namespace Theme;
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (c.isMouseOver() ? panel.brighter (0.08f) : panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (c.hasKeyboardFocus (true) ? accent2 : edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (textMuted);
    g.fillPath (arrow);
}

juce::Font InterferenceLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Theme::bodyFont (12.5f); }
juce::Font InterferenceLookAndFeel::getPopupMenuFont() { return Theme::bodyFont (13.0f); }
juce::Font InterferenceLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return Theme::labelFont (11.0f); }

void InterferenceLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::edge);
    g.drawRect (0, 0, w, h);
}

juce::Rectangle<int> InterferenceLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto f = Theme::bodyFont (12.0f);
    juce::AttributedString s;
    s.append (tipText, f, Theme::textMuted);
    juce::TextLayout tl;
    tl.createLayout (s, 320.0f);
    const int w = (int) tl.getWidth() + 24, h = (int) tl.getHeight() + 14;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 12, w, h)
        .constrainedWithin (parentArea);
}

void InterferenceLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
    g.setColour (juce::Colour (0xf0101319));
    g.fillRoundedRectangle (r, (float) h * 0.5f > 14.0f ? 8.0f : (float) h * 0.5f);
    g.setColour (Theme::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), (float) h * 0.5f > 14.0f ? 8.0f : (float) h * 0.5f, 1.0f);
    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (text, Theme::bodyFont (12.0f), Theme::textMuted);
    juce::TextLayout tl;
    tl.createLayout (s, (float) w - 24.0f);
    tl.draw (g, r.reduced (12.0f, 7.0f));
}

void InterferenceLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    if (! l.isBeingEdited())
    {
        g.setColour (l.findColour (juce::Label::textColourId).withMultipliedAlpha (l.isEnabled() ? 1.0f : 0.5f));
        g.setFont (getLabelFont (l));
        g.drawFittedText (l.getText(), l.getLocalBounds().reduced (2, 0), l.getJustificationType(), 2, 0.8f);
    }
}

juce::Label* InterferenceLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (Theme::valueFont (12.0f));
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

void InterferenceLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool)
{
    const auto thumb = vertical ? juce::Rectangle<int> (x + w / 2 - 2, thumbStart, 4, thumbSize) : juce::Rectangle<int> (thumbStart, y + h / 2 - 2, thumbSize, 4);
    g.setColour (over ? Theme::textMuted : Theme::edge);
    g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
}

} // namespace ifx
