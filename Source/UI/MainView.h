#pragma once

#include "SlotPanel.h"
#include "LaneEditor.h"
#include "StudioPanel.h"
#include "Overlays.h"

namespace ifx
{

/** The single-window layout (master spec 8): header, mode + source row, two slot columns
    with functional illustrations, context controls, bottom tabs (lane / Studio / Create /
    Routing / Environment) and the output strip. A 30 Hz timer copies engine telemetry into the views. */
class MainView final : public juce::Component, private juce::Timer
{
public:
    explicit MainView (InterferenceProcessor& p);
    ~MainView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;
    void mouseDown (const juce::MouseEvent& e) override;

    /** The one pixel that reveals the hidden Ghost Carrier panel: the upper tip of the
        signature notch in the top-left corner. */
    static constexpr int kSecretX = 14, kSecretY = 2;

    std::function<void (bool)> onTooltipsChanged;

    // exposed for GUI tests
    void showHelp();
    void showOptions();
    void showRender();
    void showDebug();
    void showGhost();
    void closeOverlay();
    juce::Component* currentOverlay() const noexcept { return overlay.get(); }
    void selectTab (int index);
    int getSelectedTab() const noexcept { return tab; }
    void selectSlot (int s);
    void toggleRecord();

private:
    void timerCallback() override;
    void rebuildIllustrations();
    void refreshPresetBox();
    void showMainMenu();
    void savePresetAs();
    void openPreset();
    void setOverlay (std::unique_ptr<Overlay> o);
    void updateHeader();
    void layoutStream();
    /** Keyboard / accessibility order: header, mode row, source palette, slot cards, illustrations,
        context panel, bottom tabs + the visible panel, output strip. */
    void setFocusOrder();
    static constexpr int kFocusIllustrations = 50;
    /** Shows an alert tied to this view (scoped: it closes when the editor goes away). */
    void showMessage (const juce::MessageBoxOptions& options);

    InterferenceProcessor& proc;

    // header
    OutputLed led;
    juce::TextButton menuButton { "Menu" }, prevPreset { "<" }, nextPreset { ">" }, aButton { "A" }, bButton { "B" }, copyAB { "A>B" },
                     resetButton { "Reset" }, randomButton { "Randomize" }, helpButton { "?" }, gearButton;
    juce::ComboBox presetBox;
    juce::Array<juce::File> userPresets;

    // mode row
    ParamMenuButton modeButtons[3]; // right-click: the Mode parameter's menu
    SourcePalette palette;
    juce::Label seedLabel;
    juce::TextButton lockButton { "Lock" }, takeButton { "New Take" };
    ParamChoice policy;

    // slots
    SlotCard cards[2];
    std::unique_ptr<Illustration> illus[2];
    SourceType illusType[2] { SourceType::None, SourceType::None };
    ContextPanel context;
    int selectedSlot = 0;
    ParamToggle orderToggle;       // Swap Order, shown between the slot cards as "A -> B" / "B -> A"
    juce::Rectangle<int> orderArea; // the column between the cards (order arrow drawn here)

    // bottom
    juce::TextButton tabButtons[5];
    juce::TextButton excBadge;     // "EXCITATION ON": visible while internal excitation feeds the circuit
    LaneEditor lane;
    StudioPanel studio;
    CreatePanel create;
    RoutingView routing;
    EnvironmentPanel environment;
    OutputStrip output;
    int tab = 1;
    int lastMode = -1;

    DataStream stream;
    std::unique_ptr<Overlay> overlay;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastDirty = -1;
    juce::String headerName;
    bool modified = false;
    juce::ScopedMessageBox messageBox; // the one alert this view may show; closed with the view
};

} // namespace ifx
