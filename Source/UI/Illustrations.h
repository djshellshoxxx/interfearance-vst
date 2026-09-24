#pragma once

#include "Widgets.h"

namespace ifx
{

/** Plain copy of one slot's telemetry, taken by the UI timer (never touches the DSP). */
struct SlotView
{
    SourceType type = SourceType::None;
    float position = 0, secondary = 0.5f, velocity = 0, openness = 0, auxOpenness = 0, activity = 0, wearHere = 0, gesture = 0, mech = 0;
    int state = 0;
    bool shorted = false, enabled = true, motionLimited = false;
    uint32_t quiet = 0, openEvents = 0, resistiveEvents = 0, gestures = 0;

    static SlotView read (const SlotTelemetry& t);
};

/** Base for the functional illustrations. They draw the actual causal mechanism from the
    engine's measured state and act as controls: drags write parameters with host gesture
    begin/end, quick motions send gestures through the processor's lock-free queue. */
class Illustration : public juce::Component, public juce::SettableTooltipClient
{
public:
    Illustration (InterferenceProcessor& p, int slotIndex, SourceType t);
    ~Illustration() override;

    void update (const SlotView& v);
    SourceType getType() const noexcept { return type; }

    void paint (juce::Graphics& g) override;
    bool keyPressed (const juce::KeyPress& k) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    /** Right-click anywhere: the primary parameter's menu (MIDI learn, reset, type a value). */
    void mouseDown (const juce::MouseEvent& e) override { handlePopup (e); }
    void mouseUp (const juce::MouseEvent&) override { finishPopup(); }

    /** Parameter the keyboard / accessible adjustment acts on. */
    virtual int primaryParam() const = 0;
    virtual juce::String stateText() const = 0;
    /** The primary parameter of a source type (same as primaryParam(), usable in constructors). */
    static int primaryParamFor (SourceType t, int slotIndex) noexcept;

protected:
    virtual void draw (juce::Graphics& g, juce::Rectangle<float> area) = 0;

    /** Opens the parameter menu on a right-click and returns true; the following drag and
        mouse-up of that click must then be ignored (see finishPopup). */
    bool handlePopup (const juce::MouseEvent& e);
    /** Call on mouse-up: returns true (and resets) if the click was a right-click menu. */
    bool finishPopup() noexcept;
    bool popupActive = false;

    float value (int param) const;
    void beginEdit (int param);
    void setValue (int param, float realValue);
    void endEdit (int param);
    void sendGesture (GestureKind k, float intensity);
    void releaseGesture();
    /** Seeded fault map of this source (identical to the DSP's). */
    float wearAt (float q) const;
    static juce::Colour heat (float w);

    InterferenceProcessor& proc;
    int slot;
    SourceType type;
    SlotView view;
    double lastGestureMs = 0.0;
    int editing = -1;
    float anim = 0.0f;
};

std::unique_ptr<Illustration> createIllustration (InterferenceProcessor& p, int slot, SourceType t);

} // namespace ifx
