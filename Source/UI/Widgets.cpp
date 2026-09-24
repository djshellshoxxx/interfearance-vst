#include "Widgets.h"

namespace ifx
{

//==============================================================================
juce::String paramTooltip (InterferenceProcessor& proc, int paramIndex)
{
    const auto& info = paramInfo (paramIndex);
    juce::String t = juce::String (info.name) + ": " + info.tooltip;
    const auto m = proc.describeMidiMapping (paramIndex);
    if (m.isNotEmpty())
        t << "\nMIDI: " << m;
    t << "\nRight-click: MIDI learn, reset, type a value.";
    return t;
}

void promptParamValue (InterferenceProcessor& proc, int paramIndex, juce::Component& target)
{
    auto* p = proc.param (paramIndex);
    auto* aw = new juce::AlertWindow ("Set value", juce::String (paramInfo (paramIndex).name) + " (" + p->getCurrentValueAsText() + ")",
                                      juce::MessageBoxIconType::NoIcon, &target);
    aw->addTextEditor ("value", p->getCurrentValueAsText());
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<juce::Component> safe (&target);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw, p, safe] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            const auto text = aw->getTextEditorContents ("value");
            const float norm = juce::jlimit (0.0f, 1.0f, p->getValueForText (text));
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    }), true);
}

void showParamMenu (InterferenceProcessor& proc, int paramIndex, juce::Component& target)
{
    auto* p = proc.param (paramIndex);
    const auto mapping = proc.describeMidiMapping (paramIndex);
    juce::PopupMenu m;
    m.addSectionHeader (juce::String (paramInfo (paramIndex).name) + "  (" + p->getCurrentValueAsText() + ")");
    if (proc.midiLearnTarget() == paramIndex)
        m.addItem (1, "Cancel MIDI learn");
    else
        m.addItem (2, "MIDI learn (move a controller)");
    m.addItem (3, "Clear MIDI mapping" + (mapping.isNotEmpty() ? " (" + mapping + ")" : juce::String()), mapping.isNotEmpty());
    m.addSeparator();
    m.addItem (4, "Reset to default");
    m.addItem (5, "Set value...");
    m.addSeparator();
    m.addItem (6, "Host automation: \"" + juce::String (paramInfo (paramIndex).name) + "\"", false);
    juce::Component::SafePointer<juce::Component> safe (&target);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                     [&proc, paramIndex, p, safe] (int r)
    {
        switch (r)
        {
            case 1: proc.cancelMidiLearn(); break;
            case 2: proc.startMidiLearn (paramIndex); break;
            case 3: proc.clearMidiMapping (paramIndex); break;
            case 4:
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getDefaultValue());
                p->endChangeGesture();
                break;
            case 5: if (safe != nullptr) promptParamValue (proc, paramIndex, *safe); break;
            default: break;
        }
    });
}

//==============================================================================
void FineSlider::mouseDown (const juce::MouseEvent& e)
{
    rightDown = e.mods.isPopupMenu();
    if (rightDown)
    {
        if (onRightClick) onRightClick();
        return;
    }
    setMouseDragSensitivity (e.mods.isShiftDown() ? 90 : (e.mods.isCommandDown() ? 1400 : 280));
    juce::Slider::mouseDown (e);
}

void FineSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! rightDown)
        juce::Slider::mouseDrag (e);
}

void FineSlider::mouseUp (const juce::MouseEvent& e)
{
    if (! rightDown)
        juce::Slider::mouseUp (e);
    rightDown = false;
}

//==============================================================================
ParamKnob::ParamKnob (InterferenceProcessor& p, int paramIndex, Style s, const juce::String& labelOverride)
    : proc (p), index (paramIndex), style (s), label (labelOverride.isNotEmpty() ? labelOverride : juce::String (paramInfo (paramIndex).name))
{
    if (style == Style::Horizontal)
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
    else
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor); // theme.md: vertical-resize cursor on knobs
    }
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setScrollWheelEnabled (true);
    slider.setWantsKeyboardFocus (true);
    slider.setTitle (label);
    slider.setDescription (paramInfo (index).tooltip);
    slider.onRightClick = [this] { showParamMenu (proc, index, slider); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramInfo (index).id, slider);
    slider.setDoubleClickReturnValue (true, (double) paramInfo (index).def);
    slider.onDragStart = [this] { showValue = true; repaint(); };
    slider.onDragEnd = [this] { showValue = false; repaint(); };
    slider.onValueChange = [this] { repaint(); };
    slider.setTooltip (paramTooltip (proc, index));
    startTimerHz (4);
}

ParamKnob::~ParamKnob() { stopTimer(); }

void ParamKnob::timerCallback()
{
    // refresh tooltip (MIDI mapping may change) and hover readout
    auto t = paramTooltip (proc, index);
    if (dormantReason.isNotEmpty())
        t << "\n(" << dormantReason << ")";
    if (limitedReason.isNotEmpty())
    {
        t << "\nLimited: " << limitedReason;
        if (! std::isnan (effectiveValue))
        {
            auto* p = proc.param (index);
            t << " (effective " << p->getText (p->convertTo0to1 (effectiveValue), 32) << ")";
        }
    }
    if (slider.getTooltip() != t)
        slider.setTooltip (t);
    const bool hov = slider.isMouseOverOrDragging() || slider.hasKeyboardFocus (false);
    if (hov != showValue)
    {
        showValue = hov;
        repaint();
    }
}

void ParamKnob::setDormant (bool dormant, const juce::String& reason)
{
    const auto r = dormant ? reason : juce::String();
    if (slider.isEnabled() == ! dormant && dormantReason == r)
        return;
    dormantReason = r;
    slider.setEnabled (! dormant);
    repaint();
}

void ParamKnob::setLimited (const juce::String& why)
{
    if (why != limitedReason)
    {
        limitedReason = why;
        timerCallback(); // tooltip at once
        repaint();
    }
}

void ParamKnob::setEffective (float realValue)
{
    const bool same = std::isnan (realValue) ? std::isnan (effectiveValue)
                                             : (! std::isnan (effectiveValue) && std::abs (realValue - effectiveValue) < 1.0e-4f);
    if (! same)
    {
        effectiveValue = realValue;
        repaint();
    }
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    if (style == Style::Horizontal)
    {
        r.removeFromTop (14);
        slider.setBounds (r.removeFromTop (std::min (24, r.getHeight())));
    }
    else
    {
        r.removeFromTop (14);   // value readout
        r.removeFromBottom (16); // label
        const int size = std::min (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (size, size));
    }
}

void ParamKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    const bool active = showValue || slider.isMouseButtonDown();
    const auto valueText = proc.param (index)->getCurrentValueAsText();
    if (style == Style::Horizontal)
    {
        auto top = r.removeFromTop (14);
        Theme::drawLabel (g, label, top, juce::Justification::centredLeft, slider.isEnabled() ? Theme::textMuted : Theme::textMuted.withAlpha (0.55f));
        g.setFont (Theme::valueFont (11.0f));
        g.setColour (active ? Theme::text : Theme::textMuted);
        g.drawText (valueText, top, juce::Justification::centredRight);
    }
    else
    {
        // value readout above (hover / drag), label below; while the control is not touched a
        // "Limited by" reason uses the readout row
        if (active)
        {
            g.setFont (Theme::valueFont (12.0f));
            g.setColour (Theme::text);
            g.drawFittedText (valueText, r.removeFromTop (14), juce::Justification::centred, 1);
        }
        else if (limitedReason.isNotEmpty())
        {
            g.setFont (Theme::bodyFont (10.0f));
            g.setColour (Theme::warning);
            g.drawFittedText (limitedReason, r.removeFromTop (14).reduced (8, 0), juce::Justification::centred, 1, 0.6f);
        }
        else
            r.removeFromTop (14);
        Theme::drawLabel (g, label, getLocalBounds().removeFromBottom (16), juce::Justification::centred,
                          slider.isEnabled() ? Theme::textMuted : Theme::textMuted.withAlpha (0.55f), 10.0f);
    }
    if (limitedReason.isNotEmpty())
    {
        g.setColour (Theme::warning);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ (float) getWidth() - 5.0f, 5.0f }));
    }
    if (proc.midiLearnTarget() == index)
    {
        g.setColour (Theme::accent2);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 1.5f);
    }
}

void ParamKnob::paintOverChildren (juce::Graphics& g)
{
    // effective (clamped) value: a short warning-coloured tick across the value arc
    if (style == Style::Horizontal || std::isnan (effectiveValue))
        return;
    const auto rp = slider.getRotaryParameters();
    const auto b = slider.getBounds().toFloat();
    const float size = std::min (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float prop = juce::jlimit (0.0f, 1.0f, (float) slider.valueToProportionOfLength ((double) effectiveValue));
    const float ang = rp.startAngleRadians + prop * (rp.endAngleRadians - rp.startAngleRadians);
    const float r0 = size * 0.5f - 7.0f, r1 = size * 0.5f + 1.0f;
    g.setColour (Theme::warning);
    g.drawLine (c.x + std::sin (ang) * r0, c.y - std::cos (ang) * r0, c.x + std::sin (ang) * r1, c.y - std::cos (ang) * r1, 2.0f);
}

//==============================================================================
ParamToggle::ParamToggle (InterferenceProcessor& p, int paramIndex, const juce::String& text)
    : proc (p), index (paramIndex)
{
    setButtonText (text.isNotEmpty() ? text : juce::String (paramInfo (index).name));
    setClickingTogglesState (true);
    setWantsKeyboardFocus (true);
    setTitle (paramInfo (index).name);
    setDescription (paramInfo (index).tooltip);
    setTooltip (paramTooltip (proc, index));
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramInfo (index).id, *this);
}

void ParamToggle::mouseDown (const juce::MouseEvent& e)
{
    menuClick = e.mods.isPopupMenu();
    if (menuClick)
    {
        showParamMenu (proc, index, *this);
        return;
    }
    juce::TextButton::mouseDown (e);
}

void ParamToggle::mouseDrag (const juce::MouseEvent& e)
{
    if (! menuClick) // a right-drag must not press the button
        juce::TextButton::mouseDrag (e);
}

void ParamToggle::mouseUp (const juce::MouseEvent& e)
{
    if (menuClick)
    {
        menuClick = false;
        return;
    }
    juce::TextButton::mouseUp (e);
    const auto t = paramTooltip (proc, index);
    setTooltip (dormantReason.isNotEmpty() ? t + "\n(" + dormantReason + ")" : t);
}

void ParamToggle::setDormant (bool dormant, const juce::String& reason)
{
    const auto r = dormant ? reason : juce::String();
    if (isEnabled() == ! dormant && dormantReason == r)
        return;
    dormantReason = r;
    setEnabled (! dormant);
    const auto t = paramTooltip (proc, index);
    setTooltip (r.isNotEmpty() ? t + "\n(" + r + ")" : t);
}

//==============================================================================
ParamMenuButton::ParamMenuButton (InterferenceProcessor& p, int paramIndex, const juce::String& text)
    : juce::TextButton (text), proc (p), index (paramIndex)
{
    setWantsKeyboardFocus (true);
    setTitle (text.isNotEmpty() ? text : juce::String (paramInfo (index).name)); // callers may refine it
}

void ParamMenuButton::mouseDown (const juce::MouseEvent& e)
{
    menuClick = e.mods.isPopupMenu();
    if (menuClick)
    {
        showParamMenu (proc, index, *this);
        return;
    }
    juce::TextButton::mouseDown (e);
}

void ParamMenuButton::mouseDrag (const juce::MouseEvent& e)
{
    if (! menuClick)
        juce::TextButton::mouseDrag (e);
}

void ParamMenuButton::mouseUp (const juce::MouseEvent& e)
{
    if (menuClick)
    {
        menuClick = false;
        return;
    }
    juce::TextButton::mouseUp (e);
}

//==============================================================================
MomentaryParamButton::MomentaryParamButton (InterferenceProcessor& p, int paramIndex, const juce::String& text)
    : proc (p), index (paramIndex)
{
    setButtonText (text);
    setWantsKeyboardFocus (true);
    setTitle (text);
    setTooltip (paramTooltip (proc, index) + "\nHold to keep it active (Space works when focused).");
}

void MomentaryParamButton::set (bool on)
{
    if (on == held) return;
    held = on;
    auto* p = proc.param (index);
    p->beginChangeGesture();
    p->setValueNotifyingHost (on ? 1.0f : 0.0f);
    p->endChangeGesture();
    setToggleState (on, juce::dontSendNotification);
}

void MomentaryParamButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showParamMenu (proc, index, *this); return; }
    juce::TextButton::mouseDown (e);
    set (true);
}

void MomentaryParamButton::mouseUp (const juce::MouseEvent& e)
{
    juce::TextButton::mouseUp (e);
    set (false);
}

bool MomentaryParamButton::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey) { set (true); return true; }
    return juce::TextButton::keyPressed (k);
}

bool MomentaryParamButton::keyStateChanged (bool keyIsDown)
{
    if (! keyIsDown && held && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
        set (false);
    return juce::TextButton::keyStateChanged (keyIsDown);
}

//==============================================================================
ParamChoice::ParamChoice (InterferenceProcessor& p, int paramIndex, const juce::String& cap)
    : proc (p), index (paramIndex), caption (cap)
{
    const auto choices = juce::StringArray::fromTokens (paramInfo (index).choices, "|", "");
    box.addItemList (choices, 1);
    box.setWantsKeyboardFocus (true);
    box.setTitle (paramInfo (index).name);
    box.setTooltip (paramTooltip (proc, index));
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramInfo (index).id, box);
    box.addMouseListener (this, false);
}

void ParamChoice::resized()
{
    auto r = getLocalBounds();
    if (caption.isNotEmpty())
        r.removeFromTop (14);
    box.setBounds (r.withSizeKeepingCentre (r.getWidth(), std::min (26, r.getHeight())));
}

void ParamChoice::paint (juce::Graphics& g)
{
    if (caption.isNotEmpty())
        Theme::drawLabel (g, caption, getLocalBounds().removeFromTop (14), juce::Justification::centredLeft,
                          box.isEnabled() ? Theme::textMuted : Theme::textMuted.withAlpha (0.55f), 10.0f);
}

void ParamChoice::setDormant (bool dormant, const juce::String& reason)
{
    const auto r = dormant ? reason : juce::String();
    if (box.isEnabled() == ! dormant && dormantReason == r)
        return;
    dormantReason = r;
    box.setEnabled (! dormant);
    box.setTooltip (r.isNotEmpty() ? paramTooltip (proc, index) + "\n(" + r + ")" : paramTooltip (proc, index));
    repaint();
}

void ParamChoice::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        box.hidePopup();
        showParamMenu (proc, index, box);
    }
}

//==============================================================================
LevelMeter::LevelMeter (const juce::String& n, bool v) : caption (n), vertical (v)
{
    setOpaque (false);
}

void LevelMeter::setLevel (float linear) noexcept
{
    const float db = linear > 1.0e-6f ? juce::Decibels::gainToDecibels (linear) : -100.0f;
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = 1.0f / 30.0f;
    levelDb = db > levelDb ? db : std::max (db, levelDb - 30.0f * dt); // smooth fall
    if (db >= peakDb) { peakDb = db; peakTime = now; }
    else if (now - peakTime > 1.5) peakDb = std::max (db, peakDb - 20.0f * dt);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty())
    {
        Theme::drawLabel (g, caption, getLocalBounds().removeFromBottom (12), juce::Justification::centred, Theme::textMuted, 9.0f);
        r.removeFromBottom (13.0f);
    }
    auto readout = r.removeFromTop (12.0f);
    g.setFont (Theme::monoFont (9.5f));
    g.setColour (peakDb > 0.0f ? Theme::clip : Theme::textMuted);
    g.drawText (peakDb <= -99.0f ? "-inf" : juce::String (peakDb, 1), readout, juce::Justification::centredRight);

    g.setColour (Theme::edge.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 2.0f);
    // gradient: teal (low) -> orange (nominal) -> yellow (near clip) -> red (clip)
    juce::ColourGradient grad = vertical ? juce::ColourGradient (Theme::accent2, r.getX(), r.getBottom(), Theme::clip, r.getX(), r.getY(), false)
                                         : juce::ColourGradient (Theme::accent2, r.getX(), r.getY(), Theme::clip, r.getRight(), r.getY(), false);
    grad.addColour (toPos (-18.0f), Theme::accent);
    grad.addColour (toPos (-3.0f), Theme::warning);
    grad.addColour (toPos (0.0f) - 0.001, Theme::warning);
    const float pos = toPos (levelDb);
    g.setGradientFill (grad);
    if (vertical)
        g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * pos), 2.0f);
    else
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * pos), 2.0f);
    // peak hold line
    const float pk = toPos (peakDb);
    g.setColour (peakDb > 0.0f ? Theme::clip : Theme::text.withAlpha (0.8f));
    if (vertical)
        g.fillRect (r.getX(), r.getBottom() - r.getHeight() * pk, r.getWidth(), 1.0f);
    else
        g.fillRect (r.getX() + r.getWidth() * pk, r.getY(), 1.0f, r.getHeight());
    // subtle ticks every 6 dB
    g.setColour (Theme::background.withAlpha (0.35f));
    for (float d = -54.0f; d < 6.0f; d += 6.0f)
    {
        const float p = toPos (d);
        if (vertical) g.fillRect (r.getX(), r.getBottom() - r.getHeight() * p, r.getWidth(), 0.5f);
        else          g.fillRect (r.getX() + r.getWidth() * p, r.getY(), 0.5f, r.getHeight());
    }
}

//==============================================================================
void OutputLed::setLevelDb (float d) noexcept
{
    if (std::abs (d - db) > 0.25f || (d > 0.0f) != (db > 0.0f))
    {
        db = d;
        repaint();
    }
}

void OutputLed::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    juce::Colour c;
    if (db > 0.0f)
        c = Theme::clip;
    else
    {
        // -inf = dark gray, brighter towards 0 dB, white at 0 dB
        const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        c = juce::Colour (0xff3a3f47).interpolatedWith (juce::Colours::white, t * t);
    }
    if (db > -40.0f)
    {
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (r.expanded (3.0f));
    }
    g.setColour (c);
    g.fillEllipse (r);
    g.setColour (Theme::edge);
    g.drawEllipse (r, 1.0f);
}

//==============================================================================
DataStream::DataStream()
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void DataStream::addLines (const juce::StringArray& l)
{
    for (const auto& s : l)
        pending.push_back (s);
    while (pending.size() > 64)
        pending.pop_front();
}

void DataStream::timerCallback()
{
    if (! pending.empty())
    {
        scroll += 0.25f; // lines per frame while data flows
        while (scroll >= 1.0f && ! pending.empty())
        {
            scroll -= 1.0f;
            lines.push_back (pending.front());
            pending.pop_front();
            while ((int) lines.size() > numVisibleLines + 2)
                lines.pop_front();
        }
        repaint();
    }
    else if (scroll != 0.0f)
    {
        scroll = 0.0f; // stop scrolling when there is no new data
        repaint();
    }
}

void DataStream::paint (juce::Graphics& g)
{
    const int lh = std::max (9, getHeight() / std::max (1, numVisibleLines));
    g.setFont (Theme::monoFont ((float) lh * 0.8f));
    const int n = (int) lines.size();
    for (int i = 0; i < n; ++i)
    {
        const int row = numVisibleLines - (n - i);
        const float y = (float) (row * lh) - scroll * (float) lh;
        if (row < -1) continue;
        const int visRow = row;
        float alpha = 0.45f;
        if (visRow <= 0 || visRow >= numVisibleLines - 1) alpha = 0.08f;       // top/bottom lines fade away
        else if (visRow == 1 || visRow == numVisibleLines - 2) alpha = 0.22f;
        g.setColour (Theme::streamGreen.withAlpha (alpha));
        g.drawText (lines[(size_t) i], juce::Rectangle<float> (0.0f, y, (float) getWidth(), (float) lh), juce::Justification::centredLeft, true);
    }
}

//==============================================================================
void flash (juce::Component& c)
{
    struct Flash : public juce::Component, private juce::Timer
    {
        Flash (juce::Component& parent)
        {
            setInterceptsMouseClicks (false, false);
            parent.addAndMakeVisible (this);
            setBounds (parent.getLocalBounds());
            startTimer (100);
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (Theme::accent.withAlpha (0.35f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        }
        void timerCallback() override { delete this; }
    };
    new Flash (c);
}

} // namespace ifx
