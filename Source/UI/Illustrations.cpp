#include "Illustrations.h"

namespace ifx
{

SlotView SlotView::read (const SlotTelemetry& t)
{
    SlotView v;
    v.type = (SourceType) t.type.load();
    v.position = t.position.load();
    v.secondary = t.secondary.load();
    v.velocity = t.velocity.load();
    v.openness = t.openness.load();
    v.auxOpenness = t.auxOpenness.load();
    v.activity = t.activity.load();
    v.wearHere = t.wearHere.load();
    v.gesture = t.gesture.load();
    v.mech = t.mechanicalPeak.load();
    v.state = t.state.load();
    v.shorted = t.shorted.load();
    v.enabled = t.enabled.load();
    v.motionLimited = t.motionLimited.load();
    v.quiet = t.quiet.load();
    v.openEvents = t.openEvents.load();
    v.resistiveEvents = t.resistiveEvents.load();
    v.gestures = t.gestures.load();
    return v;
}

//==============================================================================
Illustration::Illustration (InterferenceProcessor& p, int slotIndex, SourceType t)
    : proc (p), slot (slotIndex), type (t)
{
    setWantsKeyboardFocus (true);
    setTitle (juce::String (sourceName (t)) + " illustration (slot " + (slot == 0 ? "A" : "B") + ")");
    setDescription ("Interactive illustration. Arrow keys adjust the main control, Space plays the slot gesture.");
    setRepaintsOnMouseActivity (true);

    juce::String tip;
    switch (t)
    {
        case SourceType::LooseJack:
            tip = "Loose Jack: drag the plug left/right to move it in or out (Move Plug); drag up/down to wiggle it. "
                  "The coloured scale under the jack is the seeded worn zone the engine uses. The spring and signal path "
                  "light up only in the contact state the engine reports.";
            break;
        case SourceType::PlugTouch:
            tip = "Exposed Plug Touch: press and hold the finger to touch the tip (contact grows while held), drag down to press "
                  "harder, release to let go. The rings follow the measured hum pickup, not a decoration.";
            break;
        case SourceType::CableHandling:
            tip = "Cable Handling: drag along the cable to set the bend point; fast drags shake it. The highlighted spot is the "
                  "seeded weak spot; the mic box shows whether the microphonic pickup is on.";
            break;
        case SourceType::DirtyPot:
            tip = "Dirty Pot: drag or use the mouse wheel to turn the knob (Shift: fine); double-click to show or hide the interior. "
                  "The dark arc is the seeded worn region the engine uses.";
            break;
        default:
            tip = "Empty slot: the signal passes unchanged. Choose a source in the card above.";
            break;
    }
    tip << "\nKeys: arrows adjust " << paramInfo (primaryParamFor (t, slotIndex)).name << ", Space plays the slot gesture."
        << "\nRight-click: MIDI learn, reset or type a value for " << paramInfo (primaryParamFor (t, slotIndex)).name << ".";
    setTooltip (tip);
}

int Illustration::primaryParamFor (SourceType t, int slotIndex) noexcept
{
    switch (t)
    {
        case SourceType::LooseJack:     return P::jkInsert;
        case SourceType::PlugTouch:     return P::tcTouch;
        case SourceType::CableHandling: return P::cbBend;
        case SourceType::DirtyPot:      return P::ptTurn;
        default:                        return slotIndex == 0 ? P::slotALevel : P::slotBLevel;
    }
}

bool Illustration::handlePopup (const juce::MouseEvent& e)
{
    // set on every mouse-down: a right-click whose mouse-up never arrived (the menu window took
    // the mouse) must not make the next left drag skip its edit or leave its host gesture open
    popupActive = e.mods.isPopupMenu();
    if (popupActive)
        showParamMenu (proc, primaryParam(), *this);
    return popupActive;
}

bool Illustration::finishPopup() noexcept
{
    const bool was = popupActive;
    popupActive = false;
    return was;
}

Illustration::~Illustration()
{
    if (editing >= 0)
        endEdit (editing);
}

void Illustration::update (const SlotView& v)
{
    view = v;
    anim += 1.0f / 30.0f;
    repaint();
}

float Illustration::value (int param) const
{
    return proc.apvts.getRawParameterValue (paramInfo (param).id)->load();
}

void Illustration::beginEdit (int param)
{
    if (editing >= 0 && editing != param)
        endEdit (editing);
    if (editing != param)
    {
        editing = param;
        proc.param (param)->beginChangeGesture();
    }
}

void Illustration::setValue (int param, float v)
{
    auto* p = proc.param (param);
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (paramInfo (param).min, paramInfo (param).max, v)));
}

void Illustration::endEdit (int param)
{
    if (editing == param)
    {
        proc.param (param)->endChangeGesture();
        editing = -1;
    }
}

void Illustration::sendGesture (GestureKind k, float intensity)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastGestureMs < 120.0)
        return; // GUI gesture rate is bounded here; the engine applies the physical limits
    lastGestureMs = now;
    proc.pushControlEvent ({ 0, slot == 0 ? ControlEvent::GestureA : ControlEvent::GestureB, (int) k, juce::jlimit (0.05f, 1.0f, intensity) });
}

void Illustration::releaseGesture()
{
    proc.pushControlEvent ({ 0, slot == 0 ? ControlEvent::ReleaseA : ControlEvent::ReleaseB, 0, 0.0f });
}

float Illustration::wearAt (float q) const
{
    ParamSnapshot p;
    for (int i : { (int) P::jkWear, (int) P::jkWearPos, (int) P::cbWeakPos, (int) P::ptDirt, (int) P::ptWearPos, (int) P::ptWearWidth })
        p[i] = value (i);
    const auto ws = wearSettingsFor (type, mapSeedFor (proc.getSeed(), type), p);
    return WearMap::compute (ws, q, std::max (0.02f, ws.width));
}

juce::Colour Illustration::heat (float w)
{
    w = juce::jlimit (0.0f, 1.0f, w);
    return Theme::edge.interpolatedWith (Theme::accent, std::min (1.0f, w * 1.6f)).withAlpha (0.35f + 0.65f * w);
}

void Illustration::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::background);
    g.fillRoundedRectangle (r, 4.0f);
    draw (g, r.reduced (8.0f, 22.0f).withTrimmedTop (0.0f));

    // state caption (text as well as colour: never colour alone)
    Theme::drawLabel (g, stateText(), getLocalBounds().removeFromBottom (18).reduced (8, 0), juce::Justification::centredLeft,
                      view.openness > 0.5f ? Theme::warning : Theme::textMuted, 10.0f);
    // industrial spec 2: a gesture that could not complete in time was coalesced (every source)
    if (view.motionLimited && type != SourceType::None)
        Theme::drawLabel (g, "motion limit reached", getLocalBounds().removeFromTop (18).reduced (8, 0), juce::Justification::centredLeft, Theme::warning, 9.0f);
    if (! view.enabled)
        Theme::drawLabel (g, "bypassed (settings kept)", getLocalBounds().removeFromBottom (18).reduced (8, 0), juce::Justification::centredRight, Theme::textMuted, 10.0f);
    if (hasKeyboardFocus (false))
    {
        g.setColour (Theme::accent2.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.0f);
    }
}

bool Illustration::keyPressed (const juce::KeyPress& k)
{
    const int pi = primaryParam();
    const auto& info = paramInfo (pi);
    const float step = (info.max - info.min) * (k.getModifiers().isShiftDown() ? 0.1f : 0.02f);
    if (k == juce::KeyPress::upKey || k == juce::KeyPress::rightKey || k == juce::KeyPress::downKey || k == juce::KeyPress::leftKey)
    {
        const bool up = k == juce::KeyPress::upKey || k == juce::KeyPress::rightKey;
        beginEdit (pi);
        setValue (pi, value (pi) + (up ? step : -step));
        endEdit (pi);
        return true;
    }
    if (k == juce::KeyPress::spaceKey)
    {
        const int g = (int) proc.apvts.getRawParameterValue (paramInfo (slot == 0 ? P::gestureA : P::gestureB).id)->load();
        lastGestureMs = 0.0;
        sendGesture ((GestureKind) g, 0.8f);
        return true;
    }
    return false;
}

//==============================================================================
namespace
{
    const char* contactStateName (int s)
    {
        switch (s)
        {
            case 1: return "resistive";
            case 2: return "open";
            case 3: return "recovering";
            default: return "good";
        }
    }

    juce::Colour signalColour (float openness, bool shorted)
    {
        if (shorted) return Theme::accent2;
        if (openness > 0.6f) return Theme::clip;
        if (openness > 0.15f) return Theme::warning;
        return Theme::success;
    }

    void spark (juce::Graphics& g, juce::Point<float> c, float size, float t)
    {
        juce::Path p;
        for (int i = 0; i < 6; ++i)
        {
            const float a = (float) i * 1.047f + t * 7.0f;
            p.startNewSubPath (c);
            p.lineTo (c.x + std::cos (a) * size, c.y + std::sin (a) * size);
        }
        g.setColour (Theme::warning.withAlpha (0.8f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    //==========================================================================
    /** Loose Jack: cutaway of a TS jack (sleeve contact, tip leaf spring) and a TS plug.
        Drag the plug horizontally = Insert; shake it vertically = Wiggle gesture. */
    class JackIllustration final : public Illustration
    {
    public:
        using Illustration::Illustration;
        int primaryParam() const override { return P::jkInsert; }

        juce::String stateText() const override
        {
            juce::String s;
            if (view.shorted) s = "ground: tip shorted to sleeve (quiet)";
            else if (view.position < 0.08f) s = "withdrawn: input open";
            else if (view.openness > 0.6f) s = "open contact";
            else if (view.openness > 0.12f || view.state == 1) s = "partial / resistive contact";
            else s = "engaged";
            return s + "  |  insert " + juce::String (juce::roundToInt (view.position * 100.0f)) + " %";
        }

        void draw (juce::Graphics& g, juce::Rectangle<float> a) override
        {
            const float cy = a.getCentreY();
            const float h = std::min (a.getHeight() * 0.42f, a.getWidth() * 0.16f);
            // jack housing (right half), cut away
            const auto jack = juce::Rectangle<float> (a.getX() + a.getWidth() * 0.52f, cy - h, a.getWidth() * 0.4f, h * 2.0f);
            g.setColour (Theme::panel.brighter (0.05f));
            g.fillRoundedRectangle (jack, 3.0f);
            g.setColour (Theme::edge);
            g.drawRoundedRectangle (jack, 3.0f, 1.0f);
            const float bore = h * 0.38f;
            const auto boreR = juce::Rectangle<float> (jack.getX(), cy - bore, jack.getWidth() * 0.86f, bore * 2.0f);
            g.setColour (Theme::background);
            g.fillRect (boreR);

            // insertion scale with the seeded worn zone (the DSP's wear map)
            const float travel = boreR.getWidth() * 0.8f;
            const float x0 = boreR.getX() - travel * 0.15f;
            for (int i = 0; i < 64; ++i)
            {
                const float q = (float) i / 63.0f;
                g.setColour (heat (wearAt (q)));
                g.fillRect (x0 + q * travel, jack.getBottom() + 6.0f, travel / 63.0f + 0.5f, 4.0f);
            }
            Theme::drawLabel (g, "worn zone", juce::Rectangle<int> ((int) x0, (int) jack.getBottom() + 11, (int) travel, 12), juce::Justification::centredLeft, Theme::textMuted, 9.0f);
            const float qx = x0 + view.position * travel;
            g.setColour (Theme::text);
            g.fillRect (qx - 0.5f, jack.getBottom() + 4.0f, 1.0f, 8.0f);

            // sleeve contact ring at the jack mouth
            const float sleeveOpen = view.auxOpenness;
            g.setColour (signalColour (sleeveOpen, false).withAlpha (0.9f));
            g.fillRect (boreR.getX() + 2.0f, cy - bore - 3.0f, 10.0f, 3.0f);
            g.fillRect (boreR.getX() + 2.0f, cy + bore, 10.0f, 3.0f);

            // tip leaf spring deep in the jack
            const float springX = boreR.getX() + boreR.getWidth() * 0.78f;
            const float lateral = (view.secondary - 0.5f) * bore * 0.9f;
            const float bend = view.openness * bore * 0.6f;
            juce::Path spring;
            spring.startNewSubPath (springX - boreR.getWidth() * 0.3f, cy - bore - 1.0f);
            spring.quadraticTo (springX - 6.0f, cy - bore * 0.2f - bend, springX + 4.0f, cy + lateral * 0.3f - bend);
            g.setColour (signalColour (view.openness, view.shorted));
            g.strokePath (spring, juce::PathStrokeType (2.5f));

            // plug: sleeve barrel, insulator, tip. Travel from 0 (out) to 1 (seated)
            const float seatX = springX + 4.0f;
            const float tipX = seatX - (1.0f - view.position) * travel * 1.05f;
            const float tipLen = bore * 1.6f, insLen = bore * 0.5f, barrel = boreR.getWidth() * 0.62f;
            const float py = cy + lateral;
            const auto tip = juce::Rectangle<float> (tipX - tipLen, py - bore * 0.55f, tipLen, bore * 1.1f);
            const auto ins = juce::Rectangle<float> (tip.getX() - insLen, py - bore * 0.62f, insLen, bore * 1.24f);
            const auto sleeve = juce::Rectangle<float> (ins.getX() - barrel, py - bore * 0.75f, barrel, bore * 1.5f);
            const auto handle = juce::Rectangle<float> (sleeve.getX() - a.getWidth() * 0.18f, py - bore * 1.25f, a.getWidth() * 0.18f, bore * 2.5f);
            juce::ColourGradient metal (juce::Colour (0xffb9bec7), 0.0f, py - bore, juce::Colour (0xff5d636e), 0.0f, py + bore, false);
            g.setGradientFill (metal);
            juce::Path tipP;
            tipP.addRoundedRectangle (tip.getX(), tip.getY(), tip.getWidth(), tip.getHeight(), bore * 0.5f, bore * 0.5f, false, true, false, true);
            g.fillPath (tipP);
            g.fillRect (sleeve);
            g.setColour (Theme::knobBottom);
            g.fillRect (ins);
            g.setColour (Theme::knobTop);
            g.fillRoundedRectangle (handle, 3.0f);
            g.setColour (Theme::edge);
            g.drawRoundedRectangle (handle, 3.0f, 1.0f);

            // highlighted signal path only in the engine's actual state
            if (view.position > 0.5f && view.openness < 0.15f && ! view.shorted)
            {
                g.setColour (Theme::success.withAlpha (0.6f));
                g.drawLine (handle.getX(), py, seatX, py, 1.5f);
            }
            if (view.openness > 0.6f && ! view.shorted && view.position > 0.3f)
                spark (g, { seatX - 2.0f, py }, 6.0f, anim);
            if (view.shorted)
            {
                g.setColour (Theme::accent2);
                g.drawLine (tip.getCentreX(), py, tip.getCentreX(), cy - bore - 2.0f, 2.0f);
            }
            handleArea = handle.getUnion (sleeve);
            scaleX0 = x0;
            scaleW = travel;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (handlePopup (e)) return;
            downInsert = value (P::jkInsert);
            downY = e.position.y;
            beginEdit (P::jkInsert);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (popupActive) return;
            setValue (P::jkInsert, downInsert + (float) e.getDistanceFromDragStartX() / std::max (40.0f, scaleW));
            const float dy = std::abs (e.position.y - downY);
            if (dy > 6.0f)
            {
                sendGesture (GestureKind::Wiggle, std::min (1.0f, dy / 40.0f));
                downY = e.position.y;
            }
        }
        void mouseUp (const juce::MouseEvent&) override { if (! finishPopup()) endEdit (P::jkInsert); }

    private:
        juce::Rectangle<float> handleArea;
        float scaleX0 = 0, scaleW = 100, downInsert = 1, downY = 0;
    };

    //==========================================================================
    /** Exposed Plug Touch: exposed tip, finger, high-impedance input and the mains field.
        Press and hold on the finger/tip to touch (contact grows while held), release returns. */
    class TouchIllustration final : public Illustration, private juce::Timer
    {
    public:
        TouchIllustration (InterferenceProcessor& p, int s, SourceType t) : Illustration (p, s, t) {}
        ~TouchIllustration() override { stopTimer(); }
        int primaryParam() const override { return P::tcTouch; }

        juce::String stateText() const override
        {
            juce::String s = view.position < 0.02f ? "untouched" : (view.openness > 0.5f ? "finger fluttering" : "finger contact");
            return s + "  |  coupling " + juce::String (juce::roundToInt (view.auxOpenness * 100.0f)) + " %";
        }

        void draw (juce::Graphics& g, juce::Rectangle<float> a) override
        {
            const float cy = a.getCentreY() + a.getHeight() * 0.12f;
            const float r = std::min (a.getHeight(), a.getWidth() * 0.4f) * 0.12f;
            // mains field: concentric arcs whose intensity is the engine's measured pickup (injected
            // level relative to the hum cap); they only move while hum is actually being picked up
            const float field = juce::jlimit (0.0f, 1.0f, view.activity * 12.5f);
            const float ringStep = a.getWidth() * 0.07f;
            const float ringOffset = field > 0.02f ? std::fmod (anim * 12.0f, ringStep) : 0.0f;
            const auto src = juce::Point<float> (a.getX() + a.getWidth() * 0.1f, a.getY() + a.getHeight() * 0.2f);
            for (int i = 0; i < 6; ++i)
            {
                const float rad = (float) (i + 1) * ringStep + ringOffset;
                g.setColour (Theme::accent2.withAlpha (0.05f + 0.35f * field * (1.0f - (float) i / 6.0f)));
                g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (src), 1.0f);
            }
            Theme::drawLabel (g, "mains field", juce::Rectangle<float> (src.x - 30.0f, src.y - 8.0f, 90.0f, 14.0f).toNearestInt(), juce::Justification::centredLeft, Theme::textMuted, 9.0f);

            // exposed plug tip + wire to the high-impedance input
            const auto tip = juce::Rectangle<float> (a.getX() + a.getWidth() * 0.3f, cy - r * 0.6f, a.getWidth() * 0.22f, r * 1.2f);
            juce::ColourGradient metal (juce::Colour (0xffc8ccd3), 0.0f, tip.getY(), juce::Colour (0xff5d636e), 0.0f, tip.getBottom(), false);
            g.setGradientFill (metal);
            g.fillRoundedRectangle (tip, r * 0.6f);
            const auto input = juce::Rectangle<float> (a.getRight() - a.getWidth() * 0.22f, cy - r * 1.4f, a.getWidth() * 0.2f, r * 2.8f);
            g.setColour (Theme::panel.brighter (0.06f));
            g.fillRoundedRectangle (input, 3.0f);
            g.setColour (Theme::edge);
            g.drawRoundedRectangle (input, 3.0f, 1.0f);
            Theme::drawLabel (g, "high-Z input", input.toNearestInt(), juce::Justification::centred, Theme::textMuted, 9.0f);
            g.setColour (Theme::accent2.withAlpha (0.3f + 0.7f * view.auxOpenness));
            g.drawLine (tip.getRight(), cy, input.getX(), cy, 1.5f + 2.0f * view.auxOpenness);
            // shield sleeve over part of the conductor
            const float shield = value (P::tcShield);
            g.setColour (Theme::textMuted.withAlpha (0.15f + 0.6f * shield));
            g.drawRoundedRectangle (tip.withWidth (tip.getWidth() * (0.2f + 0.7f * shield)).expanded (3.0f, 4.0f), 4.0f, 1.0f);

            // finger with contact patch = measured contact area
            const float area = view.position;
            const float press = area * r * 0.7f;
            const auto fingerC = juce::Point<float> (tip.getX() + tip.getWidth() * 0.55f, tip.getY() - r * 2.2f + press);
            const auto finger = juce::Rectangle<float> (r * 2.4f, r * 4.4f).withCentre (fingerC.translated (0.0f, -r * 1.2f));
            g.setColour (juce::Colour (0xffc99a80).withAlpha (0.9f));
            g.fillRoundedRectangle (finger, r * 1.2f);
            g.setColour (juce::Colour (0xff8a6552));
            g.drawRoundedRectangle (finger, r * 1.2f, 1.0f);
            if (area > 0.01f)
            {
                const float pw = r * (0.4f + 2.0f * area);
                g.setColour (Theme::accent.withAlpha (0.4f + 0.5f * (1.0f - view.openness)));
                g.fillEllipse (juce::Rectangle<float> (pw, r * 0.35f).withCentre ({ fingerC.x, tip.getY() }));
            }
            fingerArea = finger.getUnion (tip);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (handlePopup (e)) return;
            baseline = value (P::tcTouch);
            downY = e.position.y;
            held = 0.0f;
            beginEdit (P::tcTouch);
            startTimerHz (30);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (popupActive) return;
            extra = juce::jlimit (-1.0f, 1.0f, (e.position.y - downY) / 80.0f); // drag down = press harder
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (finishPopup()) return;
            stopTimer();
            setValue (P::tcTouch, baseline); // release returns to the baseline
            endEdit (P::tcTouch);
            extra = 0.0f;
        }
        void timerCallback() override
        {
            held = std::min (1.0f, held + 1.0f / 30.0f / 0.6f); // contact grows over ~0.6 s
            setValue (P::tcTouch, juce::jlimit (0.0f, 1.0f, baseline + 0.25f + 0.45f * held + 0.3f * extra));
        }

    private:
        juce::Rectangle<float> fingerArea;
        float baseline = 0, downY = 0, held = 0, extra = 0;
    };

    //==========================================================================
    /** Cable Handling: flexed cable with conductive core, shield braid, the seeded weak spot
        and the optional microphonic pickup. Drag along the cable = Bend point; fast drags shake. */
    class CableIllustration final : public Illustration
    {
    public:
        using Illustration::Illustration;
        int primaryParam() const override { return P::cbBend; }

        juce::String stateText() const override
        {
            juce::StringArray s;
            if (view.openness > 0.5f) s.add ("conductor open");
            else if (view.openness > 0.12f) s.add ("conductor resistive");
            if (view.auxOpenness > 0.3f) s.add ("shield broken: hum admitted");
            if (s.isEmpty()) s.add ("cable intact here");
            return s.joinIntoString (", ") + "  |  bend at " + juce::String (juce::roundToInt (view.position * 100.0f)) + " %";
        }

        void draw (juce::Graphics& g, juce::Rectangle<float> a) override
        {
            const float x0 = a.getX() + 10.0f, x1 = a.getRight() - 10.0f;
            const float cy = a.getCentreY() - a.getHeight() * 0.05f;
            const float bendX = x0 + view.position * (x1 - x0);
            const float flex = juce::jlimit (-1.0f, 1.0f, (view.secondary - 0.5f) * 2.0f);
            const float sag = a.getHeight() * (0.18f + 0.12f * std::abs (flex) + 0.05f * std::min (1.0f, view.velocity));
            auto curveAt = [&] (float q)
            {
                const float x = x0 + q * (x1 - x0);
                const float d = (x - bendX) / (x1 - x0);
                return juce::Point<float> (x, cy + sag * std::exp (-d * d * 18.0f));
            };
            // weak-spot map along the cable (the DSP's map)
            for (int i = 0; i < 80; ++i)
            {
                const float q = (float) i / 79.0f;
                g.setColour (heat (wearAt (q) * std::max (value (P::cbDamage), value (P::cbShieldDmg))));
                g.fillRect (x0 + q * (x1 - x0), a.getBottom() - 12.0f, (x1 - x0) / 79.0f + 0.5f, 4.0f);
            }
            Theme::drawLabel (g, "weak spot map", juce::Rectangle<float> (x0, a.getBottom() - 8.0f, 120.0f, 10.0f).toNearestInt(), juce::Justification::centredLeft, Theme::textMuted, 9.0f);

            juce::Path cable;
            for (int i = 0; i <= 60; ++i)
            {
                const auto p = curveAt ((float) i / 60.0f);
                if (i == 0) cable.startNewSubPath (p); else cable.lineTo (p);
            }
            const float thick = std::max (8.0f, a.getHeight() * 0.09f);
            // shield braid (outer), broken segment highlighted when the engine opens it
            g.setColour (juce::Colour (0xff3b414c));
            g.strokePath (cable, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path dashed;
            const float dashes[] = { 3.0f, 2.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, cable, dashes, 2);
            g.setColour (Theme::textMuted.withAlpha (0.5f));
            g.strokePath (cable, juce::PathStrokeType (thick - 2.0f));
            g.setColour (Theme::knobBottom);
            g.strokePath (cable, juce::PathStrokeType (thick - 4.0f));
            // conductive core
            g.setColour (signalColour (view.openness, false).withAlpha (0.85f));
            g.strokePath (cable, juce::PathStrokeType (2.0f));

            const float weakQ = value (P::cbWeakPos);
            const auto wp = curveAt (weakQ);
            if (view.auxOpenness > 0.2f)
            {
                g.setColour (Theme::warning.withAlpha (0.4f + 0.6f * view.auxOpenness));
                g.drawEllipse (juce::Rectangle<float> (thick + 6.0f, thick + 6.0f).withCentre (wp), 1.5f);
            }
            if (view.openness > 0.5f)
            {
                g.setColour (Theme::background);
                g.fillRect (juce::Rectangle<float> (4.0f, 4.0f).withCentre (wp));
                spark (g, wp, 7.0f, anim);
            }
            g.setColour (Theme::text.withAlpha (0.8f));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (curveAt (view.position)));

            // plugs at both ends
            g.setColour (Theme::knobTop);
            g.fillRoundedRectangle (juce::Rectangle<float> (x0 - 10.0f, cy - thick, 14.0f, thick * 2.0f), 2.0f);
            g.fillRoundedRectangle (juce::Rectangle<float> (x1 - 4.0f, cy - thick, 14.0f, thick * 2.0f), 2.0f);

            // optional microphonic pickup
            const float mic = value (P::cbMic);
            const auto micR = juce::Rectangle<float> (26.0f, 16.0f).withCentre ({ a.getRight() - 30.0f, a.getY() + 12.0f });
            g.setColour (mic > 0.0f ? Theme::accent.withAlpha (0.3f + 0.7f * std::min (1.0f, view.mech * 8.0f)) : Theme::edge);
            g.drawRoundedRectangle (micR, 3.0f, 1.0f);
            Theme::drawLabel (g, mic > 0.0f ? "mic" : "mic off", micR.toNearestInt().translated (-40, 0).withWidth (36), juce::Justification::centredRight, Theme::textMuted, 9.0f);
            scaleX0 = x0;
            scaleW = x1 - x0;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (handlePopup (e)) return;
            beginEdit (P::cbBend);
            setValue (P::cbBend, (e.position.x - scaleX0) / std::max (40.0f, scaleW));
            lastX = e.position.x;
            lastT = juce::Time::getMillisecondCounterHiRes();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (popupActive) return;
            setValue (P::cbBend, (e.position.x - scaleX0) / std::max (40.0f, scaleW));
            const double now = juce::Time::getMillisecondCounterHiRes();
            const float speed = std::abs (e.position.x - lastX) / (float) std::max (1.0, now - lastT); // px/ms
            if (speed > 0.6f || std::abs (e.getDistanceFromDragStartY()) > 30)
                sendGesture (GestureKind::Shake, std::min (1.0f, speed * 0.6f + 0.3f));
            lastX = e.position.x;
            lastT = now;
        }
        void mouseUp (const juce::MouseEvent&) override { if (! finishPopup()) endEdit (P::cbBend); }

    private:
        float scaleX0 = 0, scaleW = 100, lastX = 0;
        double lastT = 0;
    };

    //==========================================================================
    /** Dirty Pot: rotary knob with a revealable interior (resistive track, wiper and the seeded
        worn arc). Drag vertically / rotate = Turn; double-click toggles the interior view. */
    class PotIllustration final : public Illustration
    {
    public:
        using Illustration::Illustration;
        int primaryParam() const override { return P::ptTurn; }

        juce::String stateText() const override
        {
            juce::String s = view.velocity < 0.005f ? "stationary" : (view.openness > 0.2f ? "wiper scratching" : "turning");
            if (view.wearHere > 0.3f) s << " on the worn arc";
            return s + "  |  " + (value (P::ptCircuit) > 0.5f ? "tone " : "level ") + juce::String (juce::roundToInt (view.secondary * 100.0f)) + " %";
        }

        void draw (juce::Graphics& g, juce::Rectangle<float> a) override
        {
            const float rad = std::min (a.getWidth() * 0.5f, a.getHeight()) * 0.42f;
            const auto c = juce::Point<float> (a.getCentreX(), a.getCentreY() + rad * 0.05f);
            const float start = juce::MathConstants<float>::pi * 1.25f, end = juce::MathConstants<float>::pi * 2.75f;
            const float ang = start + view.position * (end - start);

            if (interior)
            {
                // resistive track with the worn arc (DSP wear map), wiper at the physical angle
                for (int i = 0; i < 90; ++i)
                {
                    const float q0 = (float) i / 90.0f, q1 = (float) (i + 1) / 90.0f;
                    juce::Path seg;
                    seg.addCentredArc (c.x, c.y, rad, rad, 0.0f, start + q0 * (end - start), start + q1 * (end - start), true);
                    g.setColour (Theme::edge.interpolatedWith (juce::Colour (0xff6b4a2e), 0.5f));
                    g.strokePath (seg, juce::PathStrokeType (rad * 0.16f));
                    const float w = wearAt ((q0 + q1) * 0.5f);
                    if (w > 0.02f)
                    {
                        g.setColour (heat (w));
                        g.strokePath (seg, juce::PathStrokeType (rad * 0.16f * std::min (1.0f, 0.3f + w)));
                    }
                }
                // terminals
                g.setColour (Theme::textMuted);
                for (float t : { start, end })
                    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ c.x + std::sin (t) * rad, c.y - std::cos (t) * rad }));
                // wiper arm
                const juce::Point<float> wtip (c.x + std::sin (ang) * rad, c.y - std::cos (ang) * rad);
                g.setColour (signalColour (view.openness, false));
                g.drawLine ({ c, wtip }, 2.5f);
                g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (wtip));
                if (view.openness > 0.2f)
                    spark (g, wtip, 6.0f, anim);
                // translucent knob cap over the interior
                g.setColour (Theme::knobTop.withAlpha (0.45f));
                g.fillEllipse (juce::Rectangle<float> (rad * 1.1f, rad * 1.1f).withCentre (c));
            }
            else
            {
                const auto body = juce::Rectangle<float> (rad * 1.7f, rad * 1.7f).withCentre (c);
                Theme::drawShadow (g, body, rad);
                juce::ColourGradient grad (Theme::knobTop, c.x, body.getY(), Theme::knobBottom, c.x, body.getBottom(), false);
                g.setGradientFill (grad);
                g.fillEllipse (body);
                // knurl
                g.setColour (Theme::edge);
                for (int i = 0; i < 36; ++i)
                {
                    const float t = (float) i / 36.0f * juce::MathConstants<float>::twoPi + ang;
                    g.drawLine (c.x + std::sin (t) * rad * 0.78f, c.y - std::cos (t) * rad * 0.78f, c.x + std::sin (t) * rad * 0.85f, c.y - std::cos (t) * rad * 0.85f, 1.0f);
                }
                g.setColour (Theme::accent);
                g.drawLine (c.x, c.y, c.x + std::sin (ang) * rad * 0.7f, c.y - std::cos (ang) * rad * 0.7f, 2.5f);
            }
            // scale ticks
            g.setColour (Theme::textMuted.withAlpha (0.6f));
            for (int i = 0; i <= 10; ++i)
            {
                const float t = start + (float) i / 10.0f * (end - start);
                g.drawLine (c.x + std::sin (t) * rad * 1.12f, c.y - std::cos (t) * rad * 1.12f, c.x + std::sin (t) * rad * 1.2f, c.y - std::cos (t) * rad * 1.2f, 1.0f);
            }
            Theme::drawLabel (g, interior ? "interior (double-click: knob)" : "double-click: show interior", a.toNearestInt().removeFromTop (12), juce::Justification::centredRight, Theme::textMuted, 9.0f);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (handlePopup (e)) return;
            downTurn = value (P::ptTurn);
            beginEdit (P::ptTurn);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (popupActive) return;
            const float d = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / (e.mods.isShiftDown() ? 600.0f : 200.0f);
            setValue (P::ptTurn, downTurn + d);
        }
        void mouseUp (const juce::MouseEvent&) override { if (! finishPopup()) endEdit (P::ptTurn); }
        void mouseDoubleClick (const juce::MouseEvent&) override { interior = ! interior; repaint(); }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            beginEdit (P::ptTurn);
            setValue (P::ptTurn, value (P::ptTurn) + w.deltaY * 0.05f);
            endEdit (P::ptTurn);
        }

    private:
        bool interior = true;
        float downTurn = 0.5f;
    };

    //==========================================================================
    class EmptyIllustration final : public Illustration
    {
    public:
        using Illustration::Illustration;
        int primaryParam() const override { return slot == 0 ? P::slotALevel : P::slotBLevel; }
        juce::String stateText() const override { return "empty slot (identity)"; }
        void draw (juce::Graphics& g, juce::Rectangle<float> a) override
        {
            g.setColour (Theme::edge);
            const float dashes[] = { 4.0f, 4.0f };
            juce::Path p;
            p.addRoundedRectangle (a.reduced (10.0f), 6.0f);
            juce::Path d;
            juce::PathStrokeType (1.0f).createDashedStroke (d, p, dashes, 2);
            g.fillPath (d);
            g.setFont (Theme::bodyFont (13.0f));
            g.setColour (Theme::textMuted);
            g.drawFittedText ("Empty slot: the signal passes unchanged.\nChoose a source above to load it.", a.toNearestInt(), juce::Justification::centred, 3);
        }
    };
}

std::unique_ptr<Illustration> createIllustration (InterferenceProcessor& p, int slot, SourceType t)
{
    switch (t)
    {
        case SourceType::LooseJack:     return std::make_unique<JackIllustration> (p, slot, t);
        case SourceType::PlugTouch:     return std::make_unique<TouchIllustration> (p, slot, t);
        case SourceType::CableHandling: return std::make_unique<CableIllustration> (p, slot, t);
        case SourceType::DirtyPot:      return std::make_unique<PotIllustration> (p, slot, t);
        default:                        return std::make_unique<EmptyIllustration> (p, slot, t);
    }
}

} // namespace ifx
