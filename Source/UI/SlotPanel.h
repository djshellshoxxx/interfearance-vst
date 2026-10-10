#pragma once

#include "Illustrations.h"

namespace ifx
{

/** Plain-language explanation of why a slot / the plugin is quiet (realism spec 7, 9),
    computed from the engine's actual dependencies. */
juce::String explainQuiet (InterferenceProcessor& proc, int slot, const SlotView& v);

/** Source card for slot A or B: name, one-sentence cause, activity meter, gesture,
    enable, disturbance, trigger and "Why is it quiet?". Slots stay visible when empty. */
class SlotCard final : public juce::Component
{
public:
    SlotCard (InterferenceProcessor& p, int slotIndex);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void refresh (const SlotView& v, bool selected);

    std::function<void (int)> onSelect;
    std::function<void (int, SourceType)> onChooseSource;
    /** "Move toward worn area" chosen in the Why-quiet dialog of a stable Loose Jack. */
    std::function<void (int)> onGuideToWear;

private:
    InterferenceProcessor& proc;
    int slot;
    SlotView view;
    bool selected = false;
    juce::ComboBox sourceBox;
    ParamToggle enable;
    ParamToggle solo;
    ParamChoice gesture;
    ParamKnob level;
    ParamKnob life;
    ParamMenuButton trigger;        // momentary gesture trigger for this slot
    juce::Label triggerHint;
    ParamToggle gate;
    ParamChoice gateRate;
    ParamKnob gateDepth;
    juce::TextButton why { "Why quiet?" };
    LevelMeter meter { {}, false };
    juce::Rectangle<int> causeArea;
    bool updating = false, trigDown = false;
    juce::ScopedMessageBox messageBox; // the Why-quiet dialog; closed with the card
};

/** The four sources as buttons. Picking a third source when both slots are full opens an
    explicit Replace A / Replace B choice: a source is never silently dropped. */
class SourcePalette final : public juce::Component
{
public:
    explicit SourcePalette (InterferenceProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh();
    void pick (SourceType t);

private:
    InterferenceProcessor& proc;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::String limitNote;
};

/** Context controls for the selected slot's source + a Details drawer (circuit mode, bias,
    stereo, calibration, diagnostics). Controls irrelevant to the current circuit are
    greyed with a reason, never removed or reset. */
class ContextPanel final : public juce::Component, private juce::Timer
{
public:
    explicit ContextPanel (InterferenceProcessor& p);
    ~ContextPanel() override;
    void setSlot (int slotIndex, SourceType t);
    /** Realism spec 3: guided action for a stable healthy jack. Ramps Move Plug toward the
        worn zone (Wear Position) over ~0.6 s inside one host gesture. */
    void moveTowardWornArea();
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refresh (const SlotView& v);
    bool detailsOpen() const noexcept { return details.getToggleState(); }
    /** Opens / closes the Details drawer (the button does the same; exposed for GUI tests). */
    void setDetailsOpen (bool open);
    /** The drawer's scrolled content (exposed for GUI tests). */
    juce::Component& getDrawerContent() noexcept { return drawerContent; }
    juce::Viewport& getDrawer() noexcept { return drawer; }
    juce::Rectangle<int> getFreeArea() const noexcept { return freeArea; }
    std::function<void()> onLayoutChanged;

private:
    void rebuild();
    void timerCallback() override;
    void endGuide();
    int layoutDrawer (int width); // lays out the drawer content, returns its height
    ParamKnob* knob (int param);

    InterferenceProcessor& proc;
    int slot = 0;
    SourceType type = SourceType::LooseJack;
    juce::OwnedArray<juce::Component> primary, detailControls;
    juce::TextButton details { "Details" };
    juce::TextButton moveToWear { "Move toward worn area" };
    float guideFrom = 1.0f, guideTo = 1.0f, guidePos = 0.0f;
    float guideLast = -1.0f; // normalised value the ramp wrote last (-1 = none yet)
    bool guiding = false;
    LevelMeter throughMeter { "through" }, elecMeter { "electrical" }, mechMeter { "mechanical" };
    juce::Label diag;
    juce::Rectangle<int> freeArea;
    // Details drawer: detail controls, meters and diagnostics scroll inside a viewport, so nothing
    // is ever squeezed to zero size when the window is small. Declared last: the viewport is
    // destroyed first and only detaches (never deletes) its content.
    juce::Component drawerContent;
    juce::Viewport drawer;
};

} // namespace ifx
