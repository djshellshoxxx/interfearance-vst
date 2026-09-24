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
            if (v.quiet & QuietLowImpedance)
                why.add ("A buffered / line source holds the exposed conductor down: a broken shield picks up about 34 dB less hum. "
                         "Use Details > Source circuit: Passive Pickup (a high-impedance guitar pickup) for strong pickup.");
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
      level (p, s == 0 ? P::slotALevel : P::slotBLevel, ParamKnob::Style::SmallKnob, "Disturb"),
      trigger (p, s == 0 ? P::trigA : P::trigB, "Play")
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
    trigger.setTooltip (juce::String ("Play this slot's gesture now (hold to sustain gestures that last until release).\n"
                                      "Right-click: MIDI learn, reset or type a value for ") + paramInfo (s == 0 ? P::trigA : P::trigB).name + ".");
    trigger.setTitle (juce::String ("Play slot ") + (s == 0 ? "A" : "B"));
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
        auto options = juce::MessageBoxOptions()
                           .withIconType (juce::MessageBoxIconType::InfoIcon)
                           .withTitle (juce::String ("Why is slot ") + (slot == 0 ? "A" : "B") + " quiet?")
                           .withMessage (explainQuiet (proc, slot, view))
                           .withAssociatedComponent (this);
        // a stable, healthy jack gets the guided action (realism spec 3)
        const bool guide = proc.slotType (slot) == SourceType::LooseJack && (view.quiet & QuietHealthy) != 0;
        // scoped: the dialog closes with the card instead of outliving the editor
        if (! guide)
        {
            messageBox = juce::AlertWindow::showScopedAsync (options.withButton ("OK"), {});
            return;
        }
        juce::Component::SafePointer<SlotCard> safe (this);
        messageBox = juce::AlertWindow::showScopedAsync (options.withButton ("Move toward worn area").withButton ("OK"),
                                                         [safe] (int result)
                                                         {
                                                             if (safe != nullptr && result == 1 && safe->onGuideToWear)
                                                                 safe->onGuideToWear (safe->slot);
                                                         });
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
    details.setTooltip ("Advanced controls: source circuit, input stage, trims, stereo, gesture chaos, range policy and linking, "
                        "plus diagnostic meters and contact state. The drawer scrolls when the window is small. "
                        "(Mains, Buzz and Interference are on the Environment tab; the order toggle sits between the slot cards.)");
    details.onClick = [this] { resized(); if (onLayoutChanged) onLayoutChanged(); };
    addAndMakeVisible (details);
    diag.setFont (Theme::monoFont (10.5f));
    diag.setColour (juce::Label::textColourId, Theme::textMuted);
    diag.setJustificationType (juce::Justification::topLeft);
    for (auto* c : std::initializer_list<juce::Component*> { &diag, &throughMeter, &elecMeter, &mechMeter })
        drawerContent.addAndMakeVisible (c);
    drawer.setViewedComponent (&drawerContent, false); // owned here, not by the viewport
    drawer.setScrollBarsShown (true, false);
    drawer.setScrollBarThickness (8);
    drawer.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, Theme::textMuted.withAlpha (0.45f));
    drawer.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, Theme::edge.withAlpha (0.6f));
    drawer.setTitle ("Details");
    addChildComponent (drawer);
    moveToWear.setTooltip ("Guided action: slowly moves the plug (Move Plug) to the worn zone set by Wear Position, "
                           "where movement can make the contact fail. A healthy seated plug is stable by design.");
    moveToWear.onClick = [this] { moveTowardWornArea(); };
    addChildComponent (moveToWear);
    rebuild();
}

ContextPanel::~ContextPanel()
{
    stopTimer();
    endGuide();
}

void ContextPanel::moveTowardWornArea()
{
    if (type != SourceType::LooseJack)
        return;
    auto* p = proc.param (P::jkInsert);
    guideFrom = rawValue (proc, P::jkInsert);
    guideTo = juce::jlimit (0.0f, 1.0f, rawValue (proc, P::jkWearPos));
    guidePos = 0.0f;
    guideLast = -1.0f;
    if (! guiding)
    {
        p->beginChangeGesture();
        guiding = true;
    }
    startTimerHz (30);
}

void ContextPanel::timerCallback()
{
    auto* p = proc.param (P::jkInsert);
    // the user (knob, illustration) or host automation moved Move Plug: hand over, never fight it
    if (guideLast >= 0.0f && std::abs (p->getValue() - guideLast) > 1.0e-4f)
    {
        stopTimer();
        endGuide();
        return;
    }
    guidePos = std::min (1.0f, guidePos + 1.0f / 18.0f); // ~0.6 s at 30 Hz
    const float s = guidePos * guidePos * (3.0f - 2.0f * guidePos);
    guideLast = p->convertTo0to1 (guideFrom + (guideTo - guideFrom) * s);
    p->setValueNotifyingHost (guideLast);
    guideLast = p->getValue(); // what the parameter actually stores (range snapping)
    if (guidePos >= 1.0f)
    {
        stopTimer();
        endGuide();
    }
}

void ContextPanel::endGuide()
{
    guideLast = -1.0f;
    if (guiding)
    {
        proc.param (P::jkInsert)->endChangeGesture();
        guiding = false;
    }
}

void ContextPanel::setDetailsOpen (bool open)
{
    if (details.getToggleState() == open)
        return;
    details.setToggleState (open, juce::dontSendNotification);
    resized();
    if (onLayoutChanged)
        onLayoutChanged();
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
        drawerContent.addAndMakeVisible (detailControls.add (knob (pi)));
    // global circuit / calibration controls in the drawer (Mains, Buzz and Interference live on the
    // Environment tab and Swap Order between the slot cards: not repeated here)
    for (int pi : { (int) P::cableLoad, (int) P::inTrim, (int) P::preDrive, (int) P::preHeadroom, (int) P::stereo, (int) P::chaos })
        drawerContent.addAndMakeVisible (detailControls.add (knob (pi)));
    drawerContent.addAndMakeVisible (detailControls.add (new ParamChoice (proc, P::inputMode, "Source circuit")));
    drawerContent.addAndMakeVisible (detailControls.add (new ParamChoice (proc, P::policy, "Range policy")));
    drawerContent.addAndMakeVisible (detailControls.add (new ParamToggle (proc, P::preOn, "Input drive")));
    drawerContent.addAndMakeVisible (detailControls.add (new ParamToggle (proc, P::linkGesture, "Link gesture")));
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
    const bool stableJack = type == SourceType::LooseJack && (v.quiet & QuietHealthy) != 0;
    auto limit = [&] (ParamKnob& k)
    {
        juce::String lim;
        const int pi = k.getParamIndex();
        if ((pi == P::jkIntermit || pi == P::jkWiggle) && stableJack) lim = "Contact stable here";
        if (pi == P::ptScratch && (v.quiet & QuietStationary)) lim = "Pot is stationary";
        if (pi == P::cbShake && rawValue (proc, P::cbMic) <= 0.0f && rawValue (proc, P::cbDamage) < 0.05f
            && rawValue (proc, P::cbShieldDmg) < 0.05f) lim = "Healthy cable, mic off";
        if (pi == P::cbMic && rawValue (proc, P::cbMic) > 0.0f && (v.quiet & QuietStationary)) lim = "Cable is still";
        // a buffered / line source holds the broken-shield node down (Source Circuit: Passive Pickup for strong pickup)
        if (pi == P::cbShieldDmg && type == SourceType::CableHandling && (v.quiet & QuietLowImpedance)) lim = "Line source: ~34 dB less hum";
        k.setLimited (lim);
    };
    for (auto* c : primary)
        if (auto* k = dynamic_cast<ParamKnob*> (c)) limit (*k);
    for (auto* c : detailControls)
        if (auto* k = dynamic_cast<ParamKnob*> (c)) limit (*k);
    // the guided action is highlighted exactly when the engine reports a stable contact
    moveToWear.setToggleState (stableJack, juce::dontSendNotification);

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
    // the guided action belongs to the compact view; the open drawer needs the room
    moveToWear.setVisible (type == SourceType::LooseJack && ! open);
    if (moveToWear.isVisible())
        moveToWear.setBounds (r.removeFromTop (32).reduced (2).withWidth (std::min (220, r.getWidth() - 4)));
    drawer.setVisible (open);
    if (open)
    {
        r.removeFromTop (6);
        drawer.setBounds (r);
        const int w = std::max (72, r.getWidth() - drawer.getScrollBarThickness() - 2);
        drawerContent.setSize (w, layoutDrawer (w));
        freeArea = {};
    }
    else
        freeArea = r.withTrimmedTop (8);
}

int ContextPanel::layoutDrawer (int width)
{
    auto r = juce::Rectangle<int> (0, 0, width, 100000);
    std::vector<juce::Component*> knobs, choices, toggles;
    for (auto* c : detailControls)
    {
        if (dynamic_cast<ParamKnob*> (c) != nullptr)        knobs.push_back (c);
        else if (dynamic_cast<ParamChoice*> (c) != nullptr) choices.push_back (c);
        else                                                toggles.push_back (c);
    }
    // cells of `perRow` equal columns, `h` tall; toggles keep the 26 px button height
    auto placeRows = [&r, width] (const std::vector<juce::Component*>& cs, int perRow, int h, bool button)
    {
        const int cw = width / std::max (1, perRow);
        juce::Rectangle<int> row;
        for (size_t k = 0; k < cs.size(); ++k)
        {
            if (k % (size_t) perRow == 0)
                row = r.removeFromTop (h);
            const auto cell = row.removeFromLeft (cw).reduced (2);
            cs[k]->setBounds (button ? cell.withSizeKeepingCentre (cell.getWidth(), 26) : cell);
        }
    };
    placeRows (knobs, std::max (1, width / 72), 64, false);
    r.removeFromTop (4);
    placeRows (choices, 2, 42, false);
    placeRows (toggles, 2, 30, true);
    r.removeFromTop (6);
    auto meters = r.removeFromTop (60);
    throughMeter.setBounds (meters.removeFromLeft (70).reduced (4, 0));
    elecMeter.setBounds (meters.removeFromLeft (70).reduced (4, 0));
    mechMeter.setBounds (meters.removeFromLeft (70).reduced (4, 0));
    r.removeFromTop (4);
    diag.setBounds (r.removeFromTop (46));
    return r.getY() + 4;
}

} // namespace ifx
