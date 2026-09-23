#include "LaneEditor.h"
#include <numeric>

namespace ifx
{

StepGrid::StepGrid (InterferenceProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setTitle ("Gesture lane step grid");
    setDescription ("Arrows move, Space toggles, plus / minus change intensity, A accent, R ratchet, G gesture, P probability, C choke.");
    setTooltip ("Click: toggle a step. Drag up/down: intensity. Right-click: gesture, accent, ratchet, probability, choke, target, duration.\n"
                "Keys: arrows, Space, +/-, A, R, G, P, C, Ctrl+C/V, Shift+arrows shift, Ctrl+Shift+arrows rotate, Del, Ctrl+Z/Y.");
    refreshFromProcessor();
}

void StepGrid::refreshFromProcessor()
{
    if (! (pattern == proc.getPattern()))
    {
        pattern = proc.getPattern();
        repaint();
    }
}

void StepGrid::edit (const std::function<void (Pattern&)>& change)
{
    undoStack.push_back (pattern);
    if (undoStack.size() > 64) undoStack.pop_front();
    redoStack.clear();
    change (pattern);
    proc.setPattern (pattern);
    repaint();
}

void StepGrid::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back (pattern);
    pattern = undoStack.back();
    undoStack.pop_back();
    proc.setPattern (pattern);
    repaint();
}

void StepGrid::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (pattern);
    pattern = redoStack.back();
    redoStack.pop_back();
    proc.setPattern (pattern);
    repaint();
}

void StepGrid::setPlayheads (int a, int b)
{
    if (a != playhead[0] || b != playhead[1])
    {
        playhead[0] = a;
        playhead[1] = b;
        repaint();
    }
}

int StepGrid::visibleSteps() const
{
    return std::max ({ 8, pattern.lanes[0].length, pattern.lanes[1].length });
}

juce::Rectangle<float> StepGrid::cellBounds (int lane, int step) const
{
    const float labelW = 26.0f;
    const float w = ((float) getWidth() - labelW) / (float) visibleSteps();
    const float h = (float) getHeight() / 2.0f;
    return { labelW + w * (float) step, h * (float) lane, w, h };
}

bool StepGrid::hitCell (juce::Point<float> p, int& lane, int& step) const
{
    const float labelW = 26.0f;
    if (p.x < labelW) return false;
    lane = juce::jlimit (0, 1, (int) (p.y / ((float) getHeight() / 2.0f)));
    step = (int) ((p.x - labelW) / (((float) getWidth() - labelW) / (float) visibleSteps()));
    return step >= 0 && step < pattern.lanes[lane].length;
}

juce::String StepGrid::gestureLabel (int lane, int g) const
{
    if (g >= 3) return "preset";
    return gestureName (patternGesture (proc.slotType (lane), g));
}

void StepGrid::paint (juce::Graphics& g)
{
    const int n = visibleSteps();
    for (int lane = 0; lane < 2; ++lane)
    {
        const auto& L = pattern.lanes[lane];
        auto label = cellBounds (lane, 0).withX (0.0f).withWidth (24.0f);
        g.setColour (L.mute ? Theme::textMuted : Theme::text);
        g.setFont (Theme::headerFont (12.0f));
        g.drawText (lane == 0 ? "A" : "B", label, juce::Justification::centred);
        for (int s = 0; s < n; ++s)
        {
            auto c = cellBounds (lane, s).reduced (1.5f, 3.0f);
            const bool inLane = s < L.length;
            const auto& st = L.steps[s];
            const bool beat = (s % 4) == 0;
            g.setColour (! inLane ? Theme::background : (beat ? Theme::panel.brighter (0.06f) : Theme::panel));
            g.fillRoundedRectangle (c, 2.0f);
            if (! inLane) continue;
            if (st.active)
            {
                const float ih = c.getHeight() * (0.2f + 0.8f * st.intensity);
                auto bar = c.withTop (c.getBottom() - ih);
                g.setColour ((L.mute ? Theme::textMuted : (st.accent ? Theme::warning : Theme::accent)).withAlpha (0.35f + 0.65f * st.probability));
                g.fillRoundedRectangle (bar, 2.0f);
                if (st.ratchet > 1)
                {
                    g.setColour (Theme::background);
                    for (int r = 1; r < st.ratchet; ++r)
                        g.fillRect (c.getX() + c.getWidth() * (float) r / (float) st.ratchet, bar.getY(), 1.0f, bar.getHeight());
                }
                if (st.choke)
                {
                    g.setColour (Theme::accent2);
                    g.fillRect (c.getX(), c.getY(), c.getWidth(), 2.0f);
                }
                if (c.getWidth() > 14.0f)
                {
                    g.setColour (Theme::background);
                    g.setFont (Theme::monoFont (9.0f));
                    g.drawText (st.gesture < 3 ? juce::String (st.gesture + 1) : "*", c.withHeight (12.0f).translated (0.0f, 2.0f), juce::Justification::centred);
                }
            }
            if (s == playhead[lane])
            {
                g.setColour (Theme::text.withAlpha (0.85f));
                g.drawRoundedRectangle (c.expanded (1.0f), 2.0f, 1.2f);
            }
            if (hasKeyboardFocus (false) && lane == cursorLane && s == cursorStep)
            {
                g.setColour (Theme::accent2);
                g.drawRoundedRectangle (c.expanded (1.5f), 2.0f, 1.5f);
            }
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    int lane, step;
    if (! hitCell (e.position, lane, step)) return;
    cursorLane = lane;
    cursorStep = step;
    if (e.mods.isPopupMenu())
    {
        showStepMenu (lane, step);
        return;
    }
    dragLane = lane;
    dragStep = step;
    dragMoved = false;
    dragStartIntensity = pattern.lanes[lane].steps[step].intensity;
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragLane < 0) return;
    if (std::abs (e.getDistanceFromDragStartY()) < 3 && ! dragMoved) return;
    if (! dragMoved)
    {
        undoStack.push_back (pattern);
        redoStack.clear();
        dragMoved = true;
    }
    auto& st = pattern.lanes[dragLane].steps[dragStep];
    st.active = 1;
    st.intensity = juce::jlimit (0.05f, 1.0f, dragStartIntensity - (float) e.getDistanceFromDragStartY() / 80.0f);
    proc.setPattern (pattern);
    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent& e)
{
    if (dragLane >= 0 && ! dragMoved && ! e.mods.isPopupMenu())
    {
        const int l = dragLane, s = dragStep;
        edit ([l, s] (Pattern& p) { p.lanes[l].steps[s].active ^= 1; });
    }
    dragLane = dragStep = -1;
}

void StepGrid::showStepMenu (int lane, int step)
{
    const auto st = pattern.lanes[lane].steps[step];
    juce::PopupMenu m, gm, rm, pm, dm, tm, om, sm;
    m.addSectionHeader (juce::String ("Lane ") + (lane == 0 ? "A" : "B") + " step " + juce::String (step + 1));
    m.addItem (1, st.active ? "Turn off" : "Turn on");
    for (int g = 0; g < 4; ++g)
        gm.addItem (10 + g, gestureLabel (lane, g), true, st.gesture == g);
    m.addSubMenu ("Gesture", gm);
    m.addItem (2, "Accent", true, st.accent != 0);
    for (int r = 1; r <= 4; ++r)
        rm.addItem (20 + r, juce::String (r) + (r == 1 ? " (single)" : "x"), true, st.ratchet == r);
    m.addSubMenu ("Ratchet", rm);
    for (int k = 0; k <= 4; ++k)
        pm.addItem (30 + k, juce::String (k * 25) + " %", true, std::abs (st.probability - k * 0.25f) < 0.01f);
    m.addSubMenu ("Probability", pm);
    for (float d : { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f })
        dm.addItem (40 + (int) (d * 4), juce::String (d) + " step" + (d == 1.0f ? "" : "s"), true, std::abs (st.duration - d) < 0.01f);
    m.addSubMenu ("Duration", dm);
    tm.addItem (60, "Gesture default", true, st.target < 0.0f);
    tm.addItem (61, "Aim at the worn region");
    for (int k = 0; k <= 4; ++k)
        tm.addItem (62 + k, juce::String (k * 25) + " % travel", true, std::abs (st.target - k * 0.25f) < 0.01f);
    m.addSubMenu ("Position target", tm);
    for (int k = -2; k <= 2; ++k)
        om.addItem (80 + k, (k == 0 ? juce::String ("on grid") : juce::String (k * 10) + " % of a step"), true, std::abs (st.offset - k * 0.1f) < 0.01f);
    m.addSubMenu ("Microtiming", om);
    for (int k = 0; k <= 4; ++k)
        sm.addItem (90 + k, juce::String (k * 50) + " %", true, std::abs (st.send * 2.0f - k * 0.5f) < 0.01f);
    m.addSubMenu ("Studio send", sm);
    m.addItem (3, "Choke Studio tail on this step", true, st.choke != 0);
    m.addSeparator();
    m.addItem (4, "Copy step");
    m.addItem (5, "Paste step", haveClipboard);
    m.addItem (6, "Clear step");

    juce::Component::SafePointer<StepGrid> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (cellBounds (lane, step).toNearestInt())),
                     [safe, lane, step] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        auto wearTarget = [&self, lane]
        {
            const auto t = self.proc.slotType (lane);
            const char* id = t == SourceType::LooseJack ? "jk_wearpos" : t == SourceType::DirtyPot ? "pt_wearpos" : t == SourceType::CableHandling ? "cb_weakpos" : nullptr;
            return id != nullptr ? self.proc.apvts.getRawParameterValue (id)->load() : -1.0f;
        };
        if (r == 4) { self.clipboard = self.pattern.lanes[lane].steps[step]; self.haveClipboard = true; return; }
        const float wt = wearTarget();
        self.edit ([&self, r, lane, step, wt] (Pattern& p)
        {
            auto& s = p.lanes[lane].steps[step];
            if (r == 1) s.active ^= 1;
            else if (r == 2) s.accent ^= 1;
            else if (r == 3) s.choke ^= 1;
            else if (r == 5) s = self.clipboard;
            else if (r == 6) s = LaneStep();
            else if (r >= 10 && r < 14) { s.gesture = (uint8_t) (r - 10); s.active = 1; }
            else if (r >= 21 && r <= 24) s.ratchet = (uint8_t) (r - 20);
            else if (r >= 30 && r <= 34) s.probability = (float) (r - 30) * 0.25f;
            else if (r >= 41 && r <= 56) s.duration = (float) (r - 40) / 4.0f;
            else if (r == 60) s.target = -1.0f;
            else if (r == 61) s.target = wt;
            else if (r >= 62 && r <= 66) s.target = (float) (r - 62) * 0.25f;
            else if (r >= 78 && r <= 82) s.offset = (float) (r - 80) * 0.1f;
            else if (r >= 90 && r <= 94) s.send = (float) (r - 90) * 0.25f;
        });
    });
}

bool StepGrid::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int code = k.getKeyCode();
    auto& lane = pattern.lanes[cursorLane];
    const int L = cursorLane, S = cursorStep;
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        const int dir = code == juce::KeyPress::leftKey ? -1 : 1;
        if (mods.isShiftDown() && mods.isCommandDown())
            edit ([L, dir] (Pattern& p) { p.rotate (L, dir); });
        else if (mods.isShiftDown())
            edit ([L, dir] (Pattern& p) { p.shift (L, dir); });
        else
            cursorStep = (cursorStep + dir + lane.length) % lane.length;
        repaint();
        return true;
    }
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        cursorLane = code == juce::KeyPress::upKey ? 0 : 1;
        cursorStep = std::min (cursorStep, pattern.lanes[cursorLane].length - 1);
        repaint();
        return true;
    }
    if (code == juce::KeyPress::spaceKey || code == juce::KeyPress::returnKey)
    {
        edit ([L, S] (Pattern& p) { p.lanes[L].steps[S].active ^= 1; });
        return true;
    }
    if (k.getTextCharacter() == '+' || k.getTextCharacter() == '=' || k.getTextCharacter() == '-')
    {
        const float d = k.getTextCharacter() == '-' ? -0.1f : 0.1f;
        edit ([L, S, d] (Pattern& p) { auto& s = p.lanes[L].steps[S]; s.active = 1; s.intensity = juce::jlimit (0.05f, 1.0f, s.intensity + d); });
        return true;
    }
    if (mods.isCommandDown() && (code == 'Z' || code == 'z')) { mods.isShiftDown() ? redo() : undo(); return true; }
    if (mods.isCommandDown() && (code == 'Y' || code == 'y')) { redo(); return true; }
    if (mods.isCommandDown() && (code == 'C' || code == 'c')) { clipboard = lane.steps[S]; haveClipboard = true; return true; }
    if (mods.isCommandDown() && (code == 'V' || code == 'v') && haveClipboard)
    {
        const auto c = clipboard;
        edit ([L, S, c] (Pattern& p) { p.lanes[L].steps[S] = c; });
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        if (mods.isCommandDown())
            edit ([L] (Pattern& p) { const int n = p.lanes[L].length; for (auto& s : p.lanes[L].steps) s = LaneStep(); p.lanes[L].length = n; });
        else
            edit ([L, S] (Pattern& p) { p.lanes[L].steps[S] = LaneStep(); });
        return true;
    }
    const auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (ch == 'a') { edit ([L, S] (Pattern& p) { p.lanes[L].steps[S].accent ^= 1; }); return true; }
    if (ch == 'c') { edit ([L, S] (Pattern& p) { p.lanes[L].steps[S].choke ^= 1; }); return true; }
    if (ch == 'r') { edit ([L, S] (Pattern& p) { auto& s = p.lanes[L].steps[S]; s.ratchet = (uint8_t) (s.ratchet % 4 + 1); }); return true; }
    if (ch == 'g') { edit ([L, S] (Pattern& p) { auto& s = p.lanes[L].steps[S]; s.gesture = (uint8_t) ((s.gesture + 1) % 4); }); return true; }
    if (ch == 'p') { edit ([L, S] (Pattern& p) { auto& s = p.lanes[L].steps[S]; s.probability = s.probability <= 0.26f ? 1.0f : s.probability - 0.25f; }); return true; }
    return false;
}

//==============================================================================
LaneEditor::LaneEditor (InterferenceProcessor& p)
    : proc (p), grid (p),
      laneOn (p, P::laneOn, "Lane On"), run (p, P::runInternal, "Run"),
      division (p, P::laneDiv, "Step"),
      swing (p, P::swing, ParamKnob::Style::SmallKnob), humanize (p, P::humanize, ParamKnob::Style::SmallKnob),
      density (p, P::density, ParamKnob::Style::SmallKnob), evolve (p, P::evolve, ParamKnob::Style::SmallKnob),
      level (p, P::patLevel, ParamKnob::Style::SmallKnob, "Level"), send (p, P::studioSend, ParamKnob::Style::SmallKnob, "Send"),
      bpm (p, P::bpm, ParamKnob::Style::SmallKnob, "BPM"), chaos (p, P::chaos, ParamKnob::Style::SmallKnob),
      fill (p, P::fill, "Fill")
{
    addAndMakeVisible (grid);
    for (auto* c : std::initializer_list<juce::Component*> { &laneOn, &run, &division, &swing, &humanize, &density, &evolve, &level, &send, &bpm, &chaos, &fill, &undoB, &redoB })
        addAndMakeVisible (c);
    undoB.onClick = [this] { grid.undo(); };
    redoB.onClick = [this] { grid.redo(); };
    undoB.setTooltip ("Undo the last lane edit (Ctrl+Z).");
    redoB.setTooltip ("Redo (Ctrl+Y).");
    for (int l = 0; l < 2; ++l)
    {
        const juce::String name = l == 0 ? "A" : "B";
        lenMinus[l].setButtonText ("-");
        lenPlus[l].setButtonText ("+");
        mute[l].setButtonText ("M");
        solo[l].setButtonText ("S");
        clearLane[l].setButtonText ("Clr");
        lenMinus[l].setTooltip ("Shorter lane " + name + " (1-64 steps). Lanes can differ: e.g. A: 16 against B: 12.");
        lenPlus[l].setTooltip ("Longer lane " + name + ".");
        mute[l].setTooltip ("Mute lane " + name);
        solo[l].setTooltip ("Solo lane " + name);
        clearLane[l].setTooltip ("Clear every step of lane " + name + " (undoable).");
        mute[l].setClickingTogglesState (true);
        solo[l].setClickingTogglesState (true);
        lenMinus[l].onClick = [this, l] { grid.edit ([l] (Pattern& p) { p.lanes[l].length = std::max (1, p.lanes[l].length - 1); }); };
        lenPlus[l].onClick = [this, l] { grid.edit ([l] (Pattern& p) { p.lanes[l].length = std::min (kMaxSteps, p.lanes[l].length + 1); }); };
        mute[l].onClick = [this, l] { const bool m = mute[l].getToggleState(); grid.edit ([l, m] (Pattern& p) { p.lanes[l].mute = m; }); };
        solo[l].onClick = [this, l] { const bool s = solo[l].getToggleState(); grid.edit ([l, s] (Pattern& p) { p.lanes[l].solo = s; }); };
        clearLane[l].onClick = [this, l] { grid.edit ([l] (Pattern& p) { const int n = p.lanes[l].length; p.lanes[l] = Lane(); p.lanes[l].length = n; }); };
        for (auto* b : { &lenMinus[l], &lenPlus[l], &mute[l], &solo[l], &clearLane[l] })
            addAndMakeVisible (b);
    }
}

void LaneEditor::refresh()
{
    grid.refreshFromProcessor();
    const auto& t = proc.telemetry();
    grid.setPlayheads (t.laneStep[0].load(), t.laneStep[1].load());
    const auto& pat = proc.getPattern();
    for (int l = 0; l < 2; ++l)
    {
        mute[l].setToggleState (pat.lanes[l].mute != 0, juce::dontSendNotification);
        solo[l].setToggleState (pat.lanes[l].solo != 0, juce::dontSendNotification);
    }
    juce::String c;
    if (t.hostTransport.load() && ! t.internalClock.load())
        c = t.clockRunning.load() ? "Host clock " + juce::String (t.bpm.load(), 1) + " BPM" : "Host stopped (press Run for the internal clock)";
    else
        c = t.clockRunning.load() ? "Internal clock " + juce::String (t.bpm.load(), 1) + " BPM (no host transport)" : "No host transport: internal clock stopped";
    c << "   A: " << pat.lanes[0].length << "   B: " << pat.lanes[1].length;
    const int la = pat.lanes[0].length, lb = pat.lanes[1].length;
    const int cycle = la / std::gcd (la, lb) * lb;
    c << "   cycle " << cycle << " steps";
    if (c != clockText)
    {
        clockText = c;
        repaint();
    }
}

void LaneEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    Theme::drawSectionHeader (g, "Gesture Lane / Machine Pattern", r.removeFromTop (20).withTrimmedRight (340));
    g.setFont (Theme::monoFont (10.5f));
    g.setColour (proc.telemetry().internalClock.load() ? Theme::warning : Theme::textMuted);
    g.drawText (clockText, getLocalBounds().removeFromTop (20).withTrimmedLeft (240), juce::Justification::centredRight);
}

void LaneEditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    auto controls = r.removeFromBottom (70);
    auto side = r.removeFromLeft (150);
    for (int l = 0; l < 2; ++l)
    {
        auto row = side.removeFromTop (side.getHeight() / (2 - l)).withSizeKeepingCentre (side.getWidth(), 24);
        const int w = row.getWidth() / 5;
        lenMinus[l].setBounds (row.removeFromLeft (w).reduced (1));
        lenPlus[l].setBounds (row.removeFromLeft (w).reduced (1));
        mute[l].setBounds (row.removeFromLeft (w).reduced (1));
        solo[l].setBounds (row.removeFromLeft (w).reduced (1));
        clearLane[l].setBounds (row.reduced (1));
    }
    grid.setBounds (r.reduced (2));
    auto left = controls.removeFromLeft (90);
    laneOn.setBounds (left.removeFromTop (30).reduced (2));
    run.setBounds (left.removeFromTop (30).reduced (2));
    auto l2 = controls.removeFromLeft (92);
    division.setBounds (l2.removeFromTop (44).reduced (2));
    fill.setBounds (l2.reduced (2));
    auto ur = controls.removeFromRight (64);
    undoB.setBounds (ur.removeFromTop (30).reduced (2));
    redoB.setBounds (ur.removeFromTop (30).reduced (2));
    const int kw = controls.getWidth() / 8;
    for (auto* k : { &swing, &humanize, &density, &evolve, &level, &send, &bpm, &chaos })
        k->setBounds (controls.removeFromLeft (kw));
}

} // namespace ifx
