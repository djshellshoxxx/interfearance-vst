#include "SlotPanel.h"

namespace ifx
{

namespace
{
    float rawValue (InterferenceProcessor& p, int param) { return p.apvts.getRawParameterValue (paramInfo (param).id)->load(); }
}

juce::String explainQuiet (InterferenceProcessor& proc, int slot, const SlotView& v)
{
    juce::StringArray why;
    const auto t = proc.slotType (slot);
    const auto& tel = proc.telemetry();
    const bool excOn = rawValue (proc, P::excType) > 0.5f;
    const bool humOn = rawValue (proc, P::hum) > 0.0f || rawValue (proc, P::buzz) > 0.0f || rawValue (proc, P::rf) > 0.0f;
    const bool inputSilent = tel.inPeak.load() < 1.0e-5f;

    if (t == SourceType::None)
        return "This slot is empty: the signal passes unchanged. Load a source to hear its faults.";
    if (! v.enabled)
        why.add ("The slot is bypassed (exact identity; its settings are kept).");
    if (rawValue (proc, P::bypass) > 0.5f)
        why.add ("The plugin is bypassed: you hear the dry input.");
    if (rawValue (proc, P::mix) <= 0.001f)
        why.add ("Dry/Wet is at 0 %: only the dry input is heard.");

    switch (t)
    {
        case SourceType::LooseJack:
            if (v.quiet & QuietHealthy) why.add ("Contact stable here: the plug is fully seated on a healthy spot. Move it toward the worn area (Move Plug / Wear Position) or raise Wear.");
            if ((v.quiet & QuietStationary) && ! (v.quiet & QuietHealthy)) why.add ("The plug is not moving: faults need Wiggle, a gesture or automation of Move Plug.");
            if (v.shorted) why.add ("The tip is shorted to the sleeve: a grounded contact goes quiet instead of humming.");
            if (v.openness > 0.6f && inputSilent && ! excOn) why.add ("No input reaches this open contact: there is no audio to interrupt. Enable Excitation (Create) or play audio.");
            if (v.quiet & QuietShielded) why.add ("Circuit is shielded: an open jack picks up almost no hum (Shield is high).");
            break;
        case SourceType::PlugTouch:
            if (v.quiet & QuietNotTouched) why.add ("The tip is not touched: press on the illustration or raise Touch.");
            if (v.quiet & QuietShielded) why.add ("Circuit is shielded / terminated: raise Circuit Exposure or lower Shield.");
            if (! humOn) why.add ("There is no hum in the environment (Hum, Buzz and Interference are off): touch alone only makes brief contact clicks.");
            break;
        case SourceType::CableHandling:
            if (rawValue (proc, P::cbMic) <= 0.0f) why.add ("Cable mic is off: a sealed cable makes no handling sound. Raise Mic for rub and tap noise.");
            if (rawValue (proc, P::cbDamage) < 0.05f && rawValue (proc, P::cbShieldDmg) < 0.05f) why.add ("The cable is healthy: raise Damage or Shield Damage for electrical faults.");
            if (v.velocity < 0.01f) why.add ("The cable is still: faults follow movement (Shake, Bend or a gesture).");
            break;
        case SourceType::DirtyPot:
            if (v.quiet & QuietStationary) why.add ("Pot is stationary: a still knob does not scratch. Turn it or automate Turn Knob.");
            if (v.quiet & QuietHealthy) why.add ("The track is clean: raise Dirt.");
            if (v.wearHere < 0.05f && ! (v.quiet & QuietHealthy)) why.add ("The wiper is away from the worn arc: move Wear Position or turn through it.");
            if (inputSilent && rawValue (proc, P::ptBias) <= 0.0f && ! excOn) why.add ("Silent input and no circuit bias: scratch modulates the signal, so there is nothing to hear. Play audio, enable Excitation or add Circuit Bias.");
            break;
        default: break;
    }
    if (inputSilent && ! excOn && why.isEmpty())
        why.add ("Excitation is off and the input is silent: only injected artifacts (hum, pops, handling) can be heard.");
    if (why.isEmpty())
        why.add ("Nothing is limiting this source right now: its events depend on movement, so try a gesture.");
    return "- " + why.joinIntoString ("\n- ");
}

//==============================================================================
SlotCard::SlotCard (InterferenceProcessor& p, int s)
    : proc (p), slot (s),
      enable (p, s == 0 ? P::slotAOn : P::slotBOn, "On"),
      gesture (p, s == 0 ? P::gestureA : P::gestureB),
      level (p, s == 0 ? P::slotALevel : P::slotBLevel, ParamKnob::Style::SmallKnob, "Disturb")
{
    sourceBox.addItem ("Empty", 1);
    for (int i = 1; i <= kNumSourceTypes; ++i)
        sourceBox.addItem (sourceName ((SourceType) i), i + 1);
    sourceBox.setTitle (juce::String ("Slot ") + (s == 0 ? "A" : "B") + " source");
    sourceBox.setTooltip ("Source loaded in this slot. Choosing the source of the other slot moves it here (the two swap).");
    sourceBox.onChange = [this]
    {
        if (! updating && onChooseSource)
            onChooseSource (slot, (SourceType) (sourceBox.getSelectedId() - 1));
    };
    addAndMakeVisible (sourceBox);
    addAndMakeVisible (enable);
    addAndMakeVisible (gesture);
    addAndMakeVisible (level);
    addAndMakeVisible (trigger);
    addAndMakeVisible (why);
    addAndMakeVisible (meter);
    trigger.setTooltip ("Play this slot's gesture now (hold to sustain gestures that last until release).");
    trigger.onStateChange = [this]
    {
        const bool down = trigger.isDown();
        if (down && ! trigDown)
        {
            const int g = (int) rawValue (proc, slot == 0 ? P::gestureA : P::gestureB);
            proc.pushControlEvent ({ 0, slot == 0 ? ControlEvent::GestureA : ControlEvent::GestureB, g, 0.8f });
            flash (trigger);
        }
        else if (! down && trigDown)
            proc.pushControlEvent ({ 0, slot == 0 ? ControlEvent::ReleaseA : ControlEvent::ReleaseB, 0, 0.0f });
        trigDown = down;
    };
    why.setTooltip ("Explain what currently limits this source's sound.");
    why.onClick = [this]
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::InfoIcon)
                                          .withTitle (juce::String ("Why is slot ") + (slot == 0 ? "A" : "B") + " quiet?")
                                          .withMessage (explainQuiet (proc, slot, view))
                                          .withButton ("OK")
                                          .withAssociatedComponent (this),
                                      nullptr);
    };
    setWantsKeyboardFocus (false);
}

void SlotCard::refresh (const SlotView& v, bool sel)
{
    view = v;
    meter.setLevel (std::max (v.activity, v.openness * 0.5f));
    const int id = (int) proc.slotType (slot) + 1;
    if (sourceBox.getSelectedId() != id)
    {
        updating = true;
        sourceBox.setSelectedId (id, juce::dontSendNotification);
        updating = false;
    }
    const bool empty = proc.slotType (slot) == SourceType::None;
    trigger.setEnabled (! empty);
    gesture.setEnabled (! empty);
    if (sel != selected) { selected = sel; }
    repaint();
}

void SlotCard::mouseDown (const juce::MouseEvent&)
{
    if (onSelect) onSelect (slot);
}

void SlotCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (selected ? Theme::accent : Theme::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, selected ? 1.5f : 1.0f);
    auto top = getLocalBounds().reduced (8, 6);
    auto badge = top.removeFromLeft (22).removeFromTop (22);
    g.setColour (view.enabled && proc.slotType (slot) != SourceType::None ? Theme::accent : Theme::edge);
    g.fillRoundedRectangle (badge.toFloat(), 3.0f);
    g.setColour (Theme::background);
    g.setFont (Theme::headerFont (13.0f));
    g.drawText (slot == 0 ? "A" : "B", badge, juce::Justification::centred);
    // one-sentence physical cause
    g.setFont (Theme::bodyFont (11.5f));
    g.setColour (Theme::textMuted);
    g.drawFittedText (sourceCause (proc.slotType (slot)), causeArea, juce::Justification::topLeft, 2, 0.9f);
}

void SlotCard::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto row1 = r.removeFromTop (22);
    row1.removeFromLeft (30);
    enable.setBounds (row1.removeFromRight (40));
    row1.removeFromRight (6);
    sourceBox.setBounds (row1);
    r.removeFromTop (4);
    causeArea = r.removeFromTop (28);
    r.removeFromTop (2);
    meter.setBounds (r.removeFromTop (14));
    r.removeFromTop (4);
    auto row3 = r.removeFromTop (std::min (48, r.getHeight()));
    level.setBounds (row3.removeFromRight (52));
    row3.removeFromRight (4);
    auto btns = row3.removeFromRight (78);
    trigger.setBounds (btns.removeFromTop (btns.getHeight() / 2).reduced (0, 1));
    why.setBounds (btns.reduced (0, 1));
    row3.removeFromRight (6);
    gesture.setBounds (row3.withSizeKeepingCentre (row3.getWidth(), 26));
}

//==============================================================================
SourcePalette::SourcePalette (InterferenceProcessor& p) : proc (p)
{
    for (int i = 1; i <= kNumSourceTypes; ++i)
    {
        auto* b = buttons.add (new juce::TextButton (sourceName ((SourceType) i)));
        b->setTooltip (juce::String (sourceCause ((SourceType) i)) + "\nClick to load. Any one or two different sources can be active.");
        b->onClick = [this, i] { pick ((SourceType) i); };
        addAndMakeVisible (b);
    }
}

void SourcePalette::pick (SourceType t)
{
    const auto a = proc.slotType (0), b = proc.slotType (1);
    if (a == t || b == t)
        return; // already loaded: its card shows where
    if (a == SourceType::None) { proc.setSlotSource (0, t); return; }
    if (b == SourceType::None) { proc.setSlotSource (1, t); return; }

    // two-source limit reached: explicit replace choice
    juce::PopupMenu m;
    m.addSectionHeader ("Two-source limit: choose which slot to replace");
    m.addItem (1, juce::String ("Replace A (") + sourceName (a) + ") with " + sourceName (t));
    m.addItem (2, juce::String ("Replace B (") + sourceName (b) + ") with " + sourceName (t));
    m.addSeparator();
    m.addItem (3, "Cancel");
    limitNote = "Two sources max: pick a slot to replace";
    repaint();
    juce::Component::SafePointer<SourcePalette> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (buttons[(int) t - 1]),
                     [safe, t] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1) safe->proc.setSlotSource (0, t);
        if (r == 2) safe->proc.setSlotSource (1, t);
        safe->limitNote.clear();
        safe->repaint();
    });
}

void SourcePalette::refresh()
{
    for (int i = 0; i < buttons.size(); ++i)
    {
        const auto t = (SourceType) (i + 1);
        const bool inA = proc.slotType (0) == t, inB = proc.slotType (1) == t;
        buttons[i]->setToggleState (inA || inB, juce::dontSendNotification);
        buttons[i]->setButtonText (juce::String (sourceName (t)) + (inA ? "  [A]" : inB ? "  [B]" : ""));
    }
    const bool full = proc.slotType (0) != SourceType::None && proc.slotType (1) != SourceType::None;
    const juce::String note = full ? "2 of 2 slots used" : "";
    if (limitNote.isEmpty() || ! full)
    {
        if (limitNote != note) { limitNote = note; repaint(); }
    }
}

void SourcePalette::paint (juce::Graphics& g)
{
    Theme::drawLabel (g, limitNote, getLocalBounds().removeFromRight (180), juce::Justification::centredRight,
                      limitNote.startsWith ("Two") ? Theme::warning : Theme::textMuted, 10.0f);
}

void SourcePalette::resized()
{
    auto r = getLocalBounds();
    r.removeFromRight (184);
    const int w = r.getWidth() / std::max (1, buttons.size());
    for (auto* b : buttons)
        b->setBounds (r.removeFromLeft (w).reduced (2, 2));
}

//==============================================================================
ContextPanel::ContextPanel (InterferenceProcessor& p) : proc (p)
{
    details.setClickingTogglesState (true);
    details.setTooltip ("Circuit mode, bias, stereo, advanced calibration and diagnostics.");
    details.onClick = [this] { resized(); if (onLayoutChanged) onLayoutChanged(); };
    addAndMakeVisible (details);
    diag.setFont (Theme::monoFont (10.5f));
    diag.setColour (juce::Label::textColourId, Theme::textMuted);
    diag.setJustificationType (juce::Justification::topLeft);
    addChildComponent (diag);
    addChildComponent (throughMeter);
    addChildComponent (elecMeter);
    addChildComponent (mechMeter);
    rebuild();
}

void ContextPanel::setSlot (int s, SourceType t)
{
    if (s == slot && t == type && ! primary.isEmpty())
        return;
    slot = s;
    type = t;
    rebuild();
}

ParamKnob* ContextPanel::knob (int param)
{
    return new ParamKnob (proc, param);
}

void ContextPanel::rebuild()
{
    primary.clear();
    detailControls.clear();
    std::vector<int> prim, det;
    switch (type)
    {
        case SourceType::LooseJack:     prim = { P::jkInsert, P::jkWiggle, P::jkPressure, P::jkWear, P::hum, P::jkPop };
                                        det = { P::jkWearPos, P::jkIntermit, P::jkRecovery, P::jkShort, P::jkShield }; break;
        case SourceType::PlugTouch:     prim = { P::tcTouch, P::tcPressure, P::tcBody, P::hum, P::tcExposure, P::tcPop };
                                        det = { P::tcShield }; break;
        case SourceType::CableHandling: prim = { P::cbBend, P::cbShake, P::cbDamage, P::cbMic, P::hum, P::cbShieldDmg };
                                        det = { P::cbWeakPos, P::cbIntermit }; break;
        case SourceType::DirtyPot:      prim = { P::ptTurn, P::ptDirt, P::ptScratch, P::ptWearPos, P::ptWearWidth };
                                        det = { P::ptBias, P::ptVibration }; break;
        default:                        prim = { slot == 0 ? P::slotALevel : P::slotBLevel, P::hum }; break;
    }
    for (int pi : prim)
        addAndMakeVisible (primary.add (knob (pi)));
    if (type == SourceType::DirtyPot)
    {
        addAndMakeVisible (primary.add (new ParamChoice (proc, P::ptTaper, "Taper")));
        addAndMakeVisible (primary.add (new ParamChoice (proc, P::ptCircuit, "Circuit")));
    }
    for (int pi : det)
        addChildComponent (detailControls.add (knob (pi)));
    // global circuit / calibration controls in the drawer
    for (int pi : { (int) P::cableLoad, (int) P::inTrim, (int) P::preDrive, (int) P::preHeadroom, (int) P::stereo, (int) P::buzz, (int) P::rf, (int) P::chaos })
        addChildComponent (detailControls.add (knob (pi)));
    addChildComponent (detailControls.add (new ParamChoice (proc, P::inputMode, "Source circuit")));
    addChildComponent (detailControls.add (new ParamChoice (proc, P::mains, "Mains")));
    addChildComponent (detailControls.add (new ParamChoice (proc, P::policy, "Range policy")));
    addChildComponent (detailControls.add (new ParamToggle (proc, P::preOn, "Input drive")));
    addChildComponent (detailControls.add (new ParamToggle (proc, P::linkGesture, "Link gesture")));
    addChildComponent (detailControls.add (new ParamToggle (proc, P::swapOrder, "Swap order")));
    resized();
    repaint();
}

void ContextPanel::refresh (const SlotView& v)
{
    // dormant controls keep their values and explain why they do nothing now
    const bool passive = rawValue (proc, P::inputMode) > 0.5f;
    const bool preOn = rawValue (proc, P::preOn) > 0.5f;
    const float hum = rawValue (proc, P::hum);
    auto dormant = [this] (int param, bool d, const juce::String& why)
    {
        for (auto* c : primary) if (auto* k = dynamic_cast<ParamKnob*> (c)) if (k->getParamIndex() == param) k->setDormant (d, why);
        for (auto* c : detailControls) if (auto* k = dynamic_cast<ParamKnob*> (c)) if (k->getParamIndex() == param) k->setDormant (d, why);
    };
    dormant (P::cableLoad, ! passive, "Cable Load only matters with a Passive Pickup source circuit");
    dormant (P::preDrive, ! preOn, "Input drive stage is off");
    dormant (P::preHeadroom, ! preOn, "Input drive stage is off");
    dormant (P::jkShield, false, {});
    dormant (P::tcBody, hum <= 0.0f && rawValue (proc, P::buzz) <= 0.0f, "No hum in the environment to colour");
    dormant (P::ptVibration, rawValue (proc, P::ptDirt) < 0.7f, "Only a severely dirty pot (Dirt > 70 %) reacts to vibration");
    dormant (P::jkPop, false, {});

    // "Limited by ..." indicators when a request exceeds what the circuit can produce
    for (auto* c : primary)
        if (auto* k = dynamic_cast<ParamKnob*> (c))
        {
            juce::String lim;
            if (k->getParamIndex() == P::jkIntermit && (v.quiet & QuietHealthy)) lim = "Contact stable here";
            if (k->getParamIndex() == P::ptScratch && (v.quiet & QuietStationary)) lim = "Pot is stationary";
            if (k->getParamIndex() == P::cbShake && rawValue (proc, P::cbMic) <= 0.0f && rawValue (proc, P::cbDamage) < 0.05f) lim = "Healthy cable, mic off";
            k->setLimited (lim);
        }

    if (details.getToggleState())
    {
        const auto& t = proc.telemetry();
        throughMeter.setLevel (t.rPeak.load());
        elecMeter.setLevel (v.activity);
        mechMeter.setLevel (v.mech);
        diag.setText (juce::String ("state ") + (v.state == 0 ? "good" : v.state == 1 ? "resistive" : v.state == 2 ? "open" : "recover")
                      + "  open ev " + juce::String ((int) v.openEvents) + "  res ev " + juce::String ((int) v.resistiveEvents)
                      + "\npos " + juce::String (v.position, 3) + "  vel " + juce::String (v.velocity, 3) + "  wear " + juce::String (v.wearHere, 2)
                      + "\nseed " + juce::String::toHexString ((juce::int64) t.effectiveSeed.load()).toUpperCase(), juce::dontSendNotification);
    }
}

void ContextPanel::paint (juce::Graphics& g)
{
    Theme::drawSectionHeader (g, juce::String ("Slot ") + (slot == 0 ? "A" : "B") + ": " + sourceName (type), getLocalBounds().removeFromTop (20));
}

void ContextPanel::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (20);
    details.setBounds (head.removeFromRight (78).reduced (0, 1));
    r.removeFromTop (6);
    const int cols = 4;
    const int kw = r.getWidth() / cols;
    const int kh = 76;
    int i = 0;
    auto row = r.removeFromTop (kh);
    for (auto* c : primary)
    {
        if (i > 0 && i % cols == 0)
            row = r.removeFromTop (kh);
        auto cell = row.removeFromLeft (kw).reduced (2);
        if (dynamic_cast<ParamChoice*> (c) != nullptr)
            cell = cell.withSizeKeepingCentre (cell.getWidth(), 42);
        c->setBounds (cell);
        ++i;
    }
    const bool open = details.getToggleState();
    for (auto* c : detailControls) c->setVisible (open);
    throughMeter.setVisible (open);
    elecMeter.setVisible (open);
    mechMeter.setVisible (open);
    diag.setVisible (open);
    if (open)
    {
        r.removeFromTop (6);
        const int dk = 64;
        const int dcols = std::max (1, r.getWidth() / 72);
        const int dw = r.getWidth() / dcols;
        int j = 0;
        juce::Rectangle<int> drow;
        std::vector<juce::Component*> others;
        for (auto* c : detailControls)
        {
            if (dynamic_cast<ParamKnob*> (c) == nullptr) { others.push_back (c); continue; }
            if (j % dcols == 0) drow = r.removeFromTop (dk);
            c->setBounds (drow.removeFromLeft (dw).reduced (2));
            ++j;
        }
        auto orow = r.removeFromTop (42);
        const int ow = orow.getWidth() / 3;
        int k = 0;
        for (auto* c : others)
        {
            if (k == 3) { orow = r.removeFromTop (30); }
            const auto cell = orow.removeFromLeft (ow).reduced (2);
            c->setBounds (dynamic_cast<ParamChoice*> (c) != nullptr ? cell : cell.withSizeKeepingCentre (cell.getWidth(), 26));
            ++k;
        }
        r.removeFromTop (4);
        auto meters = r.removeFromTop (std::min (60, r.getHeight()));
        throughMeter.setBounds (meters.removeFromLeft (70).reduced (4, 0));
        elecMeter.setBounds (meters.removeFromLeft (70).reduced (4, 0));
        mechMeter.setBounds (meters.removeFromLeft (70).reduced (4, 0));
        diag.setBounds (meters);
        freeArea = {};
    }
    else
        freeArea = r.withTrimmedTop (8);
}

} // namespace ifx
