#pragma once

#include "PluginProcessor.h"
#include "UI/MainView.h"

namespace ifx
{

class InterferenceEditor final : public juce::AudioProcessorEditor
{
public:
    explicit InterferenceEditor (InterferenceProcessor& p);
    ~InterferenceEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    MainView& view() noexcept { return *mainView; }

    static constexpr int kDefaultW = 1200, kDefaultH = 800, kMinW = 1000, kMinH = 700, kMaxW = 2400, kMaxH = 1600;

private:
    void setTooltips (bool on);

    InterferenceProcessor& proc;
    InterferenceLookAndFeel lnf; // declared before the views: destroyed after them
    std::unique_ptr<juce::TooltipWindow> tooltips;
    std::unique_ptr<MainView> mainView;
};

} // namespace ifx
