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
    controls[4].addArray (juce::Array<juce::Component*> { K (P::rsFreq), K (P::rsRatio), K (P::rsDecay), K (P::rsFeedback), K (P::rsMix), T (P::rsMidi, "Follow MIDI") });
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
    for (int s = 0; s < kStages - 1; ++s)
        stageMeter[s].setLevels (t.stageIn[s].load(), t.stageOut[s].load());
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
    excType.setBounds (row.removeFromLeft (170).reduced (2).withHeight (44));
    excLevel.setBounds (row.removeFromLeft (70));
    auto t = row.removeFromLeft (150);
    keepExc.setBounds (t.removeFromTop (30).reduced (2));
    laneOn.setBounds (t.removeFromTop (30).reduced (2));
    row.removeFromLeft (8);
    playOnce.setBounds (row.removeFromLeft (170).withSizeKeepingCentre (170, 28));
    render.setBounds (row.removeFromLeft (130).withSizeKeepingCentre (122, 28));
    record.setBounds (row.removeFromLeft (130).withSizeKeepingCentre (122, 28));
}

//==============================================================================
EnvironmentPanel::EnvironmentPanel (InterferenceProcessor& p)
    : proc (p),
      mains (p, P::mains, "Mains"), buzzType (p, P::buzzType, "Buzz Source"), dimKind (p, P::dimKind, "Dimmer Kind"),
      phone (p, P::gsm, "2G Phone"), gear (p, P::glGear, "Other Gear"), outlets (p, P::glOutlets, "Outlets"),
      shield (p, P::glShield, "Cable Shield"), iface (p, P::glIface, "Interface"),
      hum (p, P::hum, ParamKnob::Style::SmallKnob), buzz (p, P::buzz, ParamKnob::Style::SmallKnob),
      dimmer (p, P::dimBright, ParamKnob::Style::SmallKnob), rf (p, P::rf, ParamKnob::Style::SmallKnob),
      phoneDist (p, P::gsmDist, ParamKnob::Style::SmallKnob, "Distance"), run (p, P::glRun, ParamKnob::Style::SmallKnob)
{
    for (auto* c : std::initializer_list<juce::Component*> { &mains, &hum, &buzz, &buzzType, &dimmer, &dimKind, &rf, &phone, &phoneDist,
                                                             &gear, &outlets, &run, &shield, &iface })
        addAndMakeVisible (c);
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
        t = "Ground loop: off";
    else if (db <= -119.0f)
        t = "Ground loop: -inf dBFS";
    else
        t = "Ground loop: " + juce::String (db, 0) + " dBFS";
    if (t != groundText)
    {
        groundText = t;
        repaint (groundArea.expanded (0, 20));
    }
}

void EnvironmentPanel::paint (juce::Graphics& g)
{
    Theme::drawSectionHeader (g, "Environment (shared by both slots)", getLocalBounds().removeFromTop (20));
    Theme::drawLabel (g, "Mains field, buzz and radio", fieldHeader, juce::Justification::centredLeft, Theme::textMuted, 9.5f);
    Theme::drawLabel (g, "2G phone", phoneHeader, juce::Justification::centredLeft, Theme::textMuted, 9.5f);
    Theme::drawLabel (g, "Ground loop through the cable shield", loopHeader, juce::Justification::centredLeft, Theme::textMuted, 9.5f);
    g.setFont (Theme::monoFont (11.5f));
    g.setColour ((int) rawValue (proc, P::glGear) == 0 ? Theme::textMuted : Theme::text);
    g.drawFittedText (groundText, groundArea, juce::Justification::centredLeft, 1);
    g.setFont (Theme::bodyFont (10.5f));
    g.setColour (Theme::textMuted);
    g.drawFittedText ("hum after the interface (measured)", groundArea.translated (0, 16), juce::Justification::centredLeft, 1);
}

void EnvironmentPanel::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    const int cw = r.getWidth() / 9;
    auto place = [] (juce::Component& c, juce::Rectangle<int> cell)
    {
        if (dynamic_cast<ParamChoice*> (&c) != nullptr)
            c.setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 4, 42));
        else
            c.setBounds (cell.reduced (2, 0));
    };

    // row 1: mains field, buzz, dimmer, RF | 2G phone
    auto head1 = r.removeFromTop (14);
    fieldHeader = head1.withWidth (cw * 7).reduced (2, 0);
    phoneHeader = head1.withTrimmedLeft (cw * 7).reduced (2, 0);
    auto row1 = r.removeFromTop (juce::jlimit (56, 70, (r.getHeight() - 18) / 2));
    for (auto* c : std::initializer_list<juce::Component*> { &mains, &hum, &buzz, &buzzType, &dimmer, &dimKind, &rf, &phone, &phoneDist })
        place (*c, row1.removeFromLeft (cw));

    // row 2: ground loop + measured readout
    r.removeFromTop (4);
    loopHeader = r.removeFromTop (14).reduced (2, 0);
    auto row2 = r.removeFromTop (std::min (70, r.getHeight()));
    for (auto* c : std::initializer_list<juce::Component*> { &gear, &outlets, &run, &shield, &iface })
        place (*c, row2.removeFromLeft (cw));
    groundArea = row2.reduced (8, 0).withHeight (18).withY (row2.getCentreY() - 16);
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
