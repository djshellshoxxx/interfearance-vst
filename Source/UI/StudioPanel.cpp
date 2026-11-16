#include "StudioPanel.h"

namespace ifx
{

namespace
{
    float rawValue (InterferenceProcessor& p, int param) { return p.apvts.getRawParameterValue (paramInfo (param).id)->load(); }

    const char* stageNames[] = { "EQ", "Contour", "Drive", "Crush", "Resonator", "Rhythm Gate", "Echo", "Feedback" };
    const int stageToggles[] = { P::eqOn, P::ctOn, P::drOn, P::crOn, P::rsOn, P::gtOn, P::ecOn, -1 };

    ParamKnob* findKnob (juce::OwnedArray<juce::Component>& cs, int param)
    {
        for (auto* c : cs)
            if (auto* k = dynamic_cast<ParamKnob*> (c))
                if (k->getParamIndex() == param)
                    return k;
        return nullptr;
    }
}

//==============================================================================
void StageMeter::setLevels (float inPeak, float outPeak) noexcept
{
    auto fall = [] (float current, float linear)
    {
        const float db = linear > 1.0e-6f ? juce::Decibels::gainToDecibels (linear) : -100.0f;
        return db > current ? db : std::max (db, current - 30.0f / 30.0f); // 30 dB/s at the 30 Hz UI rate
    };
    const float i = fall (inDb, inPeak), o = fall (outDb, outPeak);
    if (std::abs (i - inDb) > 0.05f || std::abs (o - outDb) > 0.05f)
    {
        inDb = i;
        outDb = o;
        repaint();
    }
}

void StageMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::edge.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 1.5f);
    auto pos = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); };
    const auto inBar = r.removeFromTop (r.getHeight() * 0.4f);
    g.setColour (Theme::textMuted.withAlpha (0.7f));
    g.fillRect (inBar.withWidth (inBar.getWidth() * pos (inDb)));
    const juce::Colour oc = outDb > 0.0f ? Theme::clip : (outDb > -6.0f ? Theme::warning : (outDb > -30.0f ? Theme::accent : Theme::accent2));
    g.setColour (oc);
    g.fillRect (r.withWidth (r.getWidth() * pos (outDb)));
}

//==============================================================================
StudioPanel::StudioPanel (InterferenceProcessor& p)
    : proc (p), studioOn (p, P::studioOn, "Studio On"), rawHw (p, P::rawHw, "Raw Hardware"), keepExc (p, P::keepExc, "Keep Excitation"),
      source (p, P::studioSrc, "Studio source"), level (p, P::studioLevel, ParamKnob::Style::SmallKnob, "Level")
{
    for (auto* c : std::initializer_list<juce::Component*> { &studioOn, &rawHw, &keepExc, &source, &level })
        addAndMakeVisible (c);

    auto K = [&p] (int param) { return new ParamKnob (p, param, ParamKnob::Style::SmallKnob); };
    auto C = [&p] (int param, const char* cap) { return new ParamChoice (p, param, cap); };
    auto T = [&p] (int param, const char* text) { return new ParamToggle (p, param, text); };
    controls[0].addArray (juce::Array<juce::Component*> { K (P::eqHP), K (P::eqLP), K (P::eqTilt), K (P::eqPkFreq), K (P::eqPkGain), K (P::eqMix) });
    controls[1].addArray (juce::Array<juce::Component*> { K (P::ctAttack), K (P::ctSustain), K (P::ctMix) });
    controls[2].addArray (juce::Array<juce::Component*> { C (P::drCurve, "Curve"), K (P::drPre), K (P::drPost), K (P::drTone), K (P::drMix), T (P::drAuto, "Auto Match") });
    controls[3].addArray (juce::Array<juce::Component*> { K (P::crBits), K (P::crRate), K (P::crDither), K (P::crMix) });
    controls[4].addArray (juce::Array<juce::Component*> { K (P::rsFreq), K (P::rsRatio), K (P::rsDecay), K (P::rsFeedback), K (P::rsMix), T (P::rsMidi, "Follow MIDI"),
                                                          T (P::rsSnap, "Snap to Scale"), C (P::rsKey, "Key"), C (P::rsScale, "Scale") });
    controls[5].addArray (juce::Array<juce::Component*> { C (P::gtMode, "Mode"), C (P::gtKey, "Key"), C (P::gtDiv, "Division"), K (P::gtAttack), K (P::gtRelease), K (P::gtDepth),
                                                          T (P::gtChokeFb, "Choke kills FB"), T (P::gtChokeEcho, "Choke clears Echo") });
    controls[6].addArray (juce::Array<juce::Component*> { T (P::ecSync, "Sync"), C (P::ecDiv, "Division"), K (P::ecTime), K (P::ecFeedback), K (P::ecTone), K (P::ecMix),
                                                          T (P::killTail, "Kill Tail") });
    controls[7].addArray (juce::Array<juce::Component*> { K (P::fbAmount), K (P::fbTime), K (P::fbHP), K (P::fbLP), T (P::killFb, "Kill Feedback") });

    for (int s = 0; s < kStages; ++s)
    {
        if (stageToggles[s] >= 0)
        {
            stageOn[s] = std::make_unique<ParamToggle> (p, stageToggles[s], "");
            stageOn[s]->setButtonText ("on");
            addAndMakeVisible (*stageOn[s]);
        }
        stageEdit[s].setButtonText (stageNames[s]);
        stageEdit[s].setTooltip (juce::String ("Show the ") + stageNames[s] + " controls. Studio stages are fixed in this order after the hardware.");
        stageEdit[s].onClick = [this, s] { select (s); };
        stageEdit[s].setRadioGroupId (0x57D);
        stageEdit[s].setClickingTogglesState (true);
        addAndMakeVisible (stageEdit[s]);
        if (s < kStages - 1)
        {
            stageMeter[s].setTooltip (juce::String (stageNames[s]) + " meter: upper bar = stage input peak, lower bar = stage output peak (Studio branch).");
            stageMeter[s].setTitle (juce::String (stageNames[s]) + " meter");
            addAndMakeVisible (stageMeter[s]);
        }
        for (auto* c : controls[s])
            addChildComponent (c);
    }
    select (0);
}

void StudioPanel::select (int stage)
{
    selected = stage;
    stageEdit[stage].setToggleState (true, juce::dontSendNotification);
    for (int s = 0; s < kStages; ++s)
        for (auto* c : controls[s])
            c->setVisible (s == stage);
    resized();
}

void StudioPanel::refresh()
{
    juce::String n;
    const bool on = rawValue (proc, P::studioOn) > 0.5f;
    const bool raw = rawValue (proc, P::rawHw) > 0.5f;
    const int src = (int) rawValue (proc, P::studioSrc);
    const auto& t = proc.telemetry();
    const float avail = src == 0 ? t.aPeak.load() : src == 1 ? t.dPeak.load() : std::max (t.aPeak.load(), t.dPeak.load());
    if (raw) n = "Raw Hardware: Studio, feedback and excitation bypassed (settings kept)";
    else if (! on) n = "Studio off: output is the modeled hardware only";
    else if (avail < 1.0e-5f) n = "No artifact to process: the selected Studio source is silent";
    else n = "Studio processing active (labelled Studio, not hardware)";
    const bool fbLimited = t.fbLimited.load();
    if (fbLimited)
        n << "   |   Feedback limited to " << juce::String (t.fbEffective.load() * 100.0f, 0) << " % for loop stability";
    if (rawValue (proc, P::gtOn) > 0.5f && (int) rawValue (proc, P::gtMode) == 1 && (int) rawValue (proc, P::gtKey) == 0 && ! t.clockRunning.load())
        n << "   |   Gate: no clock, stays open (start the host or press Run)";
    if (n != note) { note = n; repaint(); }

    if (auto* fb = findKnob (controls[7], P::fbAmount))
    {
        fb->setLimited (fbLimited ? juce::String ("Limited by loop stability") : juce::String());
        fb->setEffective (fbLimited ? t.fbEffective.load() : std::numeric_limits<float>::quiet_NaN());
    }
    updateDormancy();

    // take-and-reset: the engine accumulates each stage's peak since the last UI tick
    auto& tel = const_cast<EngineTelemetry&> (t);
    for (int s = 0; s < kStages - 1; ++s)
        stageMeter[s].setLevels (tel.stageIn[s].exchange (0.0f), tel.stageOut[s].exchange (0.0f));
}

void StudioPanel::updateDormancy()
{
    const bool raw = rawValue (proc, P::rawHw) > 0.5f;
    const bool on = rawValue (proc, P::studioOn) > 0.5f;
    const juce::String chainWhy = raw ? juce::String ("Raw Hardware bypasses the Studio chain (settings kept)")
                                : ! on ? juce::String ("Studio is off (settings kept)") : juce::String();

    auto setDormant = [] (juce::Component* c, const juce::String& why)
    {
        const bool d = why.isNotEmpty();
        if (auto* k = dynamic_cast<ParamKnob*> (c))         k->setDormant (d, why);
        else if (auto* ch = dynamic_cast<ParamChoice*> (c)) ch->setDormant (d, why);
        else if (auto* t = dynamic_cast<ParamToggle*> (c))  t->setDormant (d, why);
    };
    auto paramOf = [] (juce::Component* c)
    {
        if (auto* k = dynamic_cast<ParamKnob*> (c))   return k->getParamIndex();
        if (auto* ch = dynamic_cast<ParamChoice*> (c)) return ch->getParamIndex();
        if (auto* t = dynamic_cast<ParamToggle*> (c))  return t->getParamIndex();
        return -1;
    };

    // one setting makes another irrelevant (checked only while the stage itself runs)
    const bool sync = rawValue (proc, P::ecSync) > 0.5f;
    const bool followMidi = rawValue (proc, P::rsMidi) > 0.5f;
    const int gateMode = (int) rawValue (proc, P::gtMode);   // Through | Pulse | Choke
    const int gateKey = (int) rawValue (proc, P::gtKey);     // Steps | MIDI | Sidechain | Envelope
    const bool feedback = rawValue (proc, P::fbAmount) > 0.0f;
    auto specific = [&] (int pi) -> juce::String
    {
        if (pi == P::ecTime && sync)                     return "Sync is on: Division sets the echo time";
        if (pi == P::ecDiv && ! sync)                    return "Sync is off: Echo Time sets the echo time";
        if (pi == P::rsFreq && followMidi)               return "Follow MIDI is on: the last played note sets the frequency";
        if (pi == P::gtKey && gateMode != 1)             return "Key only matters in Pulse mode";
        if (pi == P::gtDiv && (gateMode != 1 || gateKey != 0)) return gateMode != 1 ? "Division only matters in Pulse mode"
                                                                                 : "Key is not Steps: the key signal, not the grid, opens the gate";
        if ((pi == P::gtAttack || pi == P::gtRelease || pi == P::gtDepth) && gateMode == 0)
            return "Mode is Through: the gate stays open";
        if ((pi == P::fbTime || pi == P::fbHP || pi == P::fbLP) && ! feedback)
            return "Feedback is 0 %: no loop to shape";
        return {};
    };

    for (int s = 0; s < kStages; ++s)
    {
        juce::String stageWhy = chainWhy;
        if (stageWhy.isEmpty() && stageToggles[s] >= 0 && rawValue (proc, stageToggles[s]) < 0.5f)
            stageWhy = juce::String (stageNames[s]) + " is off (settings kept)";
        for (auto* c : controls[s])
        {
            const int pi = paramOf (c);
            if (pi == P::killFb || pi == P::killTail)
            {
                setDormant (c, chainWhy); // momentary kills stay usable while their stage is off
                continue;
            }
            setDormant (c, stageWhy.isNotEmpty() ? stageWhy : specific (pi));
        }
    }
    setDormant (&source, chainWhy);
    setDormant (&level, chainWhy);
    setDormant (&keepExc, raw ? juce::String() : juce::String ("Keep Excitation only matters while Raw Hardware is on"));
}

void StudioPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    Theme::drawSectionHeader (g, "Studio chain (post-hardware)", r.removeFromTop (20).withTrimmedRight (420));
    g.setFont (Theme::bodyFont (11.5f));
    g.setColour (note.startsWith ("No artifact") ? Theme::warning : Theme::textMuted);
    g.drawText (note, getLocalBounds().removeFromBottom (16), juce::Justification::centredLeft);
    // flow arrows between blocks
    g.setColour (Theme::edge);
    for (int s = 0; s < kStages - 2; ++s)
    {
        const auto a = stageEdit[s].getBounds(), b = stageEdit[s + 1].getBounds();
        g.drawArrow (juce::Line<float> ((float) a.getRight(), (float) a.getCentreY(), (float) b.getX(), (float) b.getCentreY()), 1.0f, 5.0f, 5.0f);
    }
}

void StudioPanel::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (20);
    head.removeFromLeft (220);
    rawHw.setBounds (head.removeFromRight (110).reduced (2, 0));
    keepExc.setBounds (head.removeFromRight (120).reduced (2, 0));
    studioOn.setBounds (head.removeFromRight (90).reduced (2, 0));
    r.removeFromTop (6);
    r.removeFromBottom (18);
    auto left = r.removeFromLeft (130);
    source.setBounds (left.removeFromTop (44).reduced (2));
    level.setBounds (left.removeFromTop (std::min (70, left.getHeight())));

    auto chain = r.removeFromTop (56);
    const int w = chain.getWidth() / kStages;
    for (int s = 0; s < kStages; ++s)
    {
        auto cell = chain.removeFromLeft (w).reduced (6, 0);
        stageEdit[s].setBounds (cell.removeFromTop (24));
        cell.removeFromTop (2);
        auto toggleRow = cell.removeFromTop (18);
        if (stageOn[s] != nullptr)
            stageOn[s]->setBounds (toggleRow.withSizeKeepingCentre (std::min (40, toggleRow.getWidth()), 18));
        if (s < kStages - 1)
            stageMeter[s].setBounds (cell.withTrimmedTop (3).withHeight (8));
    }
    r.removeFromTop (4);
    auto& cs = controls[selected];
    if (cs.isEmpty()) return;
    const int cw = std::min (96, r.getWidth() / std::max (1, cs.size()));
    for (auto* c : cs)
    {
        auto cell = r.removeFromLeft (cw).reduced (3, 0);
        if (dynamic_cast<ParamKnob*> (c) != nullptr) c->setBounds (cell.withHeight (std::min (72, cell.getHeight())));
        else if (dynamic_cast<ParamChoice*> (c) != nullptr) c->setBounds (cell.withHeight (42));
        else c->setBounds (cell.withHeight (26).translated (0, 16));
    }
}

//==============================================================================
RoutingView::RoutingView (InterferenceProcessor& p)
    : proc (p), monitor (p, P::monitor, "Monitor (what leaves the plugin)")
{
    const char* names[] = { "x dry", "R ref", "D breaks", "A artif.", "C core", "P studio", "W wet", "y out" };
    for (int i = 0; i < 8; ++i)
    {
        meters[i].setCaption (names[i]);
        addAndMakeVisible (meters[i]);
    }
    addAndMakeVisible (monitor);
    captureTap.addItemList ({ "Full Output y", "Full Core C", "Injected Artifacts A", "Fault Difference D", "Studio Only P", "Full Wet W", "Dry Input x" }, 1);
    captureTap.setSelectedId (proc.recordTap.load() + 1, juce::dontSendNotification);
    captureTap.onChange = [this] { proc.recordTap.store (captureTap.getSelectedId() - 1); };
    captureTap.setTooltip ("Capture Tap: which bus Record writes. Tapping never changes the audible output or the random sequence.");
    captureTap.setTitle ("Capture tap");
    addAndMakeVisible (captureTap);
}

void RoutingView::refresh()
{
    const auto& t = proc.telemetry();
    const float v[8] = { t.inPeak.load(), t.rPeak.load(), t.dPeak.load(), t.aPeak.load(), t.cPeak.load(), t.pPeak.load(), t.wPeak.load(), t.outPeak.load() };
    for (int i = 0; i < 8; ++i)
        meters[i].setLevel (v[i]);
    juce::String s;
    s << "Latency: 0 samples (all buses aligned)   Tail: " << juce::String (t.tailSeconds.load(), 2) << " s   ";
    s << "Sidechain: " << (t.sidechainPresent.load() ? "present (control only)" : "absent (React / key disabled)") << "   ";
    s << (t.numChannels.load() == 1 ? "mono" : "stereo") << " @ " << juce::String (t.sampleRate.load() / 1000.0, 1) << " kHz";
    const bool swapped = rawValue (proc, P::swapOrder) > 0.5f;
    const juce::String order = swapped ? "p  -> slot B -> slot A (swapped)    -> C" : "p  -> slot A -> slot B (user order)  -> C";
    juce::String fb = "     feedback: Resonator out -> Drive in, ";
    fb << juce::String (rawValue (proc, P::fbAmount) * 100.0f, 0) << " %";
    if (t.fbLimited.load())
        fb << " (limited to " << juce::String (t.fbEffective.load() * 100.0f, 0) << " %)";
    if (s != status || order != orderLine || fb != feedbackLine)
    {
        status = s;
        orderLine = order;
        feedbackLine = fb;
        repaint();
    }
    if (captureTap.getSelectedId() != proc.recordTap.load() + 1)
        captureTap.setSelectedId (proc.recordTap.load() + 1, juce::dontSendNotification);
}

void RoutingView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    Theme::drawSectionHeader (g, "Routing: named buses", r.removeFromTop (20));
    g.setFont (Theme::monoFont (10.5f));
    g.setColour (Theme::textMuted);
    g.drawText (status, getLocalBounds().removeFromBottom (16), juce::Justification::centredLeft);
    // signal-flow sketch: x -> [slots] -> R/C0/C -> D = C0 - R, A = C - C0 -> Studio(F) -> P -> W = R + U + P -> mix -> y
    auto flow = getLocalBounds().withTrimmedTop (24).withTrimmedBottom (20).removeFromRight (getWidth() / 2).reduced (8);
    g.setFont (Theme::monoFont (11.0f));
    g.setColour (Theme::text.withAlpha (0.85f));
    juce::StringArray lines;
    lines.add ("x  -> trim/circuit (+ excitation e) -> p");
    lines.add (orderLine.isNotEmpty() ? orderLine : juce::String ("p  -> slot A -> slot B (user order)  -> C"));
    lines.add ("      same contacts, no injection    -> C0");
    lines.add ("      healthy reference              -> R");
    lines.add ("D = C0 - R (breaks)   A = C - C0 (artifacts)");
    lines.add ("F = A | D | A+D -> EQ>Contour>Drive>Crush");
    lines.add ("     >Resonator>Gate>Echo -> P");
    lines.add (feedbackLine.isNotEmpty() ? feedbackLine : juce::String ("     feedback: Resonator out -> Drive in"));
    lines.add ("W = R + U + P   (Studio off / Raw HW: W = C)");
    lines.add ("y = dry*x + wet*W -> output trim -> out");
    const int lh = std::max (12, flow.getHeight() / lines.size());
    for (const auto& l : lines)
        g.drawText (l, flow.removeFromTop (lh), juce::Justification::centredLeft);
}

void RoutingView::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    r.removeFromBottom (18);
    auto left = r.removeFromLeft (getWidth() / 2);
    auto ctl = left.removeFromTop (44);
    monitor.setBounds (ctl.removeFromLeft (ctl.getWidth() / 2).reduced (2));
    captureTap.setBounds (ctl.reduced (2).withTrimmedTop (14));
    const int w = left.getWidth() / 8;
    for (auto& m : meters)
        m.setBounds (left.removeFromLeft (w).reduced (4, 2));
}

//==============================================================================
CreatePanel::CreatePanel (InterferenceProcessor& p)
    : proc (p), excType (p, P::excType, "Excitation (studio source)"), excLevel (p, P::excLevel, ParamKnob::Style::SmallKnob, "Exc Level"),
      keepExc (p, P::keepExc, "Keep with Raw HW"), laneOn (p, P::laneOn, "Lane On")
{
    for (auto* c : std::initializer_list<juce::Component*> { &excType, &excLevel, &keepExc, &laneOn, &playOnce, &render, &record })
        addAndMakeVisible (c);
    playOnce.setTooltip ("Play one cycle of the Gesture Lane now (same as MIDI note 62).");
    playOnce.onClick = [this]
    {
        proc.pushControlEvent ({ 0, ControlEvent::NoteOn, 62, 1.0f });
        proc.pushControlEvent ({ 0, ControlEvent::NoteOff, 61, 0.0f }); // keeps the held-note count balanced
        flash (playOnce);
    };
    render.setTooltip ("Offline render of the internal excitation / gesture path to a WAV (cloned engine, fixed tempo).");
    render.onClick = [this] { if (onRender) onRender(); };
    record.setTooltip ("Record the chosen Capture Tap (Routing) to a WAV while you play.");
    record.onClick = [this] { if (onRecord) onRecord(); };
}

void CreatePanel::refresh()
{
    const bool rec = proc.recorder().isRecording();
    record.setButtonText (rec ? "Stop recording" : "Record...");
    record.setToggleState (rec, juce::dontSendNotification);
    juce::String t;
    if (rec)
        t = "Recording " + juce::String ((double) proc.recorder().samplesWritten() / std::max (1.0, proc.currentSampleRate()), 1) + " s to " + proc.recorder().getFile().getFileName()
          + (proc.recorder().samplesDropped() > 0 ? "  (dropped " + juce::String ((int) proc.recorder().samplesDropped()) + " samples: disk too slow)" : juce::String());
    if (t != recText) { recText = t; repaint(); }

    // realism spec 1: the stored level is kept; the range policy's cap is shown, not hidden
    const auto& tel = proc.telemetry();
    if (tel.excitationLimited.load())
    {
        const bool expressive = (int) rawValue (proc, P::policy) == 1;
        excLevel.setLimited (juce::String ("Limited by ") + (expressive ? "Expressive" : "Natural") + " cap");
        excLevel.setEffective (tel.excitationEffectiveDb.load());
    }
    else
    {
        excLevel.setLimited ({});
        excLevel.setEffective (std::numeric_limits<float>::quiet_NaN());
    }
}

void CreatePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    Theme::drawSectionHeader (g, "Create: excitation, one-shots, render", r.removeFromTop (20));
    g.setFont (Theme::bodyFont (12.0f));
    g.setColour (Theme::textMuted);
    auto text = getLocalBounds().withTrimmedTop (84).reduced (4, 0);
    g.drawFittedText ("Excitation is an internal studio source that feeds the same modeled circuit to reveal faults on a silent track. "
                      "It is not a claim that a disconnected cable makes these sounds unaided. Monitor 'Injected Artifacts' (Routing) "
                      "to hear only the fault response. Host bounce / record remains the simplest route for incoming-track material.",
                      text.removeFromTop (48), juce::Justification::topLeft, 3);
    g.setColour (Theme::accent);
    g.drawText (recText, text.removeFromTop (18), juce::Justification::centredLeft);
}

void CreatePanel::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    auto row = r.removeFromTop (62);
    // natural widths sum to 828 px; narrower panels (706 px at the 1000 px minimum window) scale
    // every cell down so Record never leaves the panel (button text shrinks to 70 % before eliding)
    const float k = juce::jmin (1.0f, (float) row.getWidth() / 828.0f);
    auto cell = [&row, k] (int natural) { return row.removeFromLeft (juce::roundToInt ((float) natural * k)); };
    excType.setBounds (cell (170).reduced (2).withHeight (44));
    excLevel.setBounds (cell (70));
    auto t = cell (150);
    keepExc.setBounds (t.removeFromTop (30).reduced (2));
    laneOn.setBounds (t.removeFromTop (30).reduced (2));
    cell (8);
    playOnce.setBounds (cell (170).reduced (2, 0).withSizeKeepingCentre (juce::roundToInt (166.0f * k) - 4, 28));
    render.setBounds (cell (130).withSizeKeepingCentre (juce::roundToInt (122.0f * k), 28));
    record.setBounds (row.withWidth (std::min (row.getWidth(), juce::roundToInt (130.0f * k))).withSizeKeepingCentre (
                          std::min (row.getWidth(), juce::roundToInt (122.0f * k)), 28));
}

//==============================================================================
EnvironmentPanel::EnvironmentPanel (InterferenceProcessor& p)
    : proc (p),
      mains (p, P::mains, "Mains"), buzzType (p, P::buzzType, "Buzz Source"), dimKind (p, P::dimKind, "Dimmer Kind"),
      rfType (p, P::rfType, "Transmitter"), phone (p, P::gsm, "2G Phone"), gear (p, P::glGear, "Other Gear"), outlets (p, P::glOutlets, "Outlets"),
      shield (p, P::glShield, "Cable Shield"), iface (p, P::glIface, "Interface"),
      hum (p, P::hum, ParamKnob::Style::SmallKnob), buzz (p, P::buzz, ParamKnob::Style::SmallKnob),
      dimmer (p, P::dimBright, ParamKnob::Style::SmallKnob), rf (p, P::rf, ParamKnob::Style::SmallKnob),
      phoneDist (p, P::gsmDist, ParamKnob::Style::SmallKnob, "Distance"), run (p, P::glRun, ParamKnob::Style::Horizontal)
{
    for (auto* c : std::initializer_list<juce::Component*> { &mains, &hum, &buzz, &buzzType, &dimmer, &dimKind, &rf, &rfType, &phone, &phoneDist,
                                                             &gear, &outlets, &run, &shield, &iface })
        addAndMakeVisible (c);
    groundReadout.setFont (Theme::monoFont (11.0f));
    groundReadout.setJustificationType (juce::Justification::centredRight);
    groundReadout.setColour (juce::Label::textColourId, Theme::textMuted);
    groundReadout.setTitle ("Ground loop hum");
    groundReadout.setTooltip ("The engine's measured ground-loop hum after the interface (peak of the last audio block).");
    addAndMakeVisible (groundReadout);
}

void EnvironmentPanel::refresh()
{
    // dormant controls keep their values and say why they do nothing now (realism spec 9)
    const bool buzzOn = rawValue (proc, P::buzz) > 0.0f;
    const bool dimmerSource = (int) rawValue (proc, P::buzzType) == 1;
    const juce::String dimWhy = ! buzzOn ? "Buzz Band is 0: no buzz in the room" : "Buzz Source is Supply: no dimmer";
    dimmer.setDormant (! buzzOn || ! dimmerSource, dimWhy);
    dimKind.setDormant (! buzzOn || ! dimmerSource, dimWhy);
    buzzType.setDormant (! buzzOn, "Buzz Band is 0: no buzz in the room");
    rfType.setDormant (rawValue (proc, P::rf) <= 0.0f, "Interference is 0: no transmitter nearby");

    const int g = (int) rawValue (proc, P::glGear);
    const juce::String loopWhy = "Other Gear is Off: no ground loop";
    run.setDormant (g == 0, loopWhy);
    shield.setDormant (g == 0, loopWhy);
    iface.setDormant (g == 0, loopWhy);
    outlets.setDormant (g != 4, g == 0 ? loopWhy : juce::String ("Outlets only matter for Grounded gear (2-prong gear loops through its leakage current)"));

    phoneDist.setDormant ((int) rawValue (proc, P::gsm) == 0, "The phone is Away");

    const float db = proc.telemetry().groundLoopDb.load();
    juce::String t;
    if (g == 0)
        t = "loop off";
    else if (db <= -119.0f)
        t = "after the interface: -inf dBFS";
    else
        t = "after the interface: " + juce::String (db, 0) + " dBFS";
    groundReadout.setText (t, juce::dontSendNotification); // repaints only when the text changes
    groundReadout.setColour (juce::Label::textColourId, g == 0 ? Theme::textMuted : Theme::text);
}

void EnvironmentPanel::paint (juce::Graphics& g)
{
    Theme::drawSectionHeader (g, "Environment (shared by both slots)", getLocalBounds().removeFromTop (20));
    Theme::drawLabel (g, "Mains field, buzz and radio", fieldHeader, juce::Justification::centredLeft, Theme::textMuted, 10.0f);
    Theme::drawLabel (g, "2G phone", phoneHeader, juce::Justification::centredLeft, Theme::textMuted, 10.0f);
    Theme::drawLabel (g, "Ground loop through the cable shield", loopHeader, juce::Justification::centredLeft, Theme::textMuted, 10.0f);
    // groups are separated by 1 px lines, never boxes inside boxes (theme.md)
    g.setColour (Theme::edge);
    if (phoneDividerX > 0 && bandBottom > fieldHeader.getY())
        g.fillRect (phoneDividerX, fieldHeader.getY(), 1, bandBottom - fieldHeader.getY());
    if (bandBottom > 0)
        g.fillRect (0, bandBottom + 3, getWidth(), 1);
}

void EnvironmentPanel::resized()
{
    // Two bands that fit the 208 px the bottom area gives at the minimum window height:
    //   field band: five topic columns, each a level knob above its source / kind choice
    //   loop band:  one row of choices plus a horizontal Cable Run slider, readout in its header
    // Every choice is at least ~120 px wide at the minimum window width, so its text stays readable.
    constexpr int headerH = 14, choiceH = 42, loopRowH = 42, gap = 8;
    auto r = getLocalBounds();
    r.removeFromTop (22); // section header
    const int w = r.getWidth();
    const int knobH = juce::jlimit (52, 76, r.getHeight() - (headerH + choiceH + gap + headerH + loopRowH));

    const int colW = w / 5;
    auto head1 = r.removeFromTop (headerH);
    fieldHeader = head1.withWidth (colW * 4).reduced (2, 0);
    phoneHeader = head1.withTrimmedLeft (colW * 4).reduced (8, 0);
    phoneDividerX = head1.getX() + colW * 4;
    auto knobRow = r.removeFromTop (knobH);
    auto choiceRow = r.removeFromTop (choiceH);
    bandBottom = choiceRow.getBottom();
    const std::pair<juce::Component*, juce::Component*> columns[] = {
        { &hum, &mains }, { &buzz, &buzzType }, { &dimmer, &dimKind }, { &rf, &rfType }, { &phoneDist, &phone } };
    for (const auto& col : columns)
    {
        col.first->setBounds (knobRow.removeFromLeft (colW).reduced (4, 0));
        col.second->setBounds (choiceRow.removeFromLeft (colW).reduced (4, 0));
    }

    r.removeFromTop (gap);
    auto head2 = r.removeFromTop (headerH);
    groundReadout.setBounds (head2.removeFromRight (std::min (240, w / 2)).withTrimmedRight (4));
    loopHeader = head2.reduced (2, 0);
    auto loopRow = r.removeFromTop (loopRowH);
    // Interface gets a wider cell for "Balanced + Input Transformer"
    const int share = w * 10 / 53;
    for (auto* c : std::initializer_list<juce::Component*> { &gear, &outlets, &run, &shield })
        c->setBounds (loopRow.removeFromLeft (share).reduced (4, 0));
    iface.setBounds (loopRow.reduced (4, 0));
}

//==============================================================================
OutputStrip::OutputStrip (InterferenceProcessor& p)
    : proc (p), mix (p, P::mix, ParamKnob::Style::Knob, "Dry/Wet"), out (p, P::outTrim, ParamKnob::Style::SmallKnob, "Output"),
      dryTrim (p, P::dryTrim, ParamKnob::Style::SmallKnob, "Dry Trim"), ceiling (p, P::ceiling, ParamKnob::Style::SmallKnob, "Ceiling"),
      ceilOn (p, P::ceilOn, "Ceiling"), bypass (p, P::bypass, "Bypass"),
      monitor (p, P::monitor, "Monitor"), mixLaw (p, P::mixLaw, "Mix law")
{
    for (auto* c : std::initializer_list<juce::Component*> { &mix, &out, &dryTrim, &ceiling, &ceilOn, &bypass, &monitor, &mixLaw, &inMeter, &outMeter })
        addAndMakeVisible (c);
}

void OutputStrip::refresh()
{
    const auto& t = proc.telemetry();
    inMeter.setLevel (t.inPeak.load());
    outMeter.setLevel (t.outPeak.load());
    ceiling.setDormant (rawValue (proc, P::ceilOn) < 0.5f, "Ceiling stage is off");
    const juce::String s = juce::String (t.numChannels.load() == 1 ? "mono" : "stereo") + " in/out";
    if (s != ioText) { ioText = s; repaint(); }
}

void OutputStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    Theme::drawSectionHeader (g, "Output", r.removeFromTop (20));
    Theme::drawLabel (g, ioText, getLocalBounds().removeFromTop (20), juce::Justification::centredRight, Theme::textMuted, 9.0f);
}

void OutputStrip::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    auto meters = r.removeFromRight (56);
    inMeter.setBounds (meters.removeFromLeft (28).reduced (3, 0));
    outMeter.setBounds (meters.reduced (3, 0));
    // compact layout (choices side by side, shorter knobs) when the strip is short, e.g. at minimum editor size
    const bool compact = r.getHeight() < 84 + 44 + 44 + 66 + 30;
    auto top = r.removeFromTop (compact ? 78 : 84);
    mix.setBounds (top.removeFromLeft (top.getWidth() / 2));
    out.setBounds (top);
    if (compact)
    {
        auto choices = r.removeFromTop (44);
        monitor.setBounds (choices.removeFromLeft (choices.getWidth() / 2).reduced (2));
        mixLaw.setBounds (choices.reduced (2));
    }
    else
    {
        monitor.setBounds (r.removeFromTop (44).reduced (2));
        mixLaw.setBounds (r.removeFromTop (44).reduced (2));
    }
    auto row = r.removeFromTop (compact ? 60 : 66);
    dryTrim.setBounds (row.removeFromLeft (row.getWidth() / 2));
    ceiling.setBounds (row);
    auto b = r.removeFromTop (30);
    ceilOn.setBounds (b.removeFromLeft (b.getWidth() / 2).reduced (2));
    bypass.setBounds (b.reduced (2));
}

} // namespace ifx
