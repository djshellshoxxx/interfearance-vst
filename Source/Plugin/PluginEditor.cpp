#include "PluginEditor.h"

namespace ifx
{

InterferenceEditor::InterferenceEditor (InterferenceProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    mainView = std::make_unique<MainView> (p);
    mainView->onTooltipsChanged = [this] (bool on) { setTooltips (on); };
    addAndMakeVisible (*mainView);
    setTooltips (p.tooltipsEnabled());

    setResizable (true, true);
    setResizeLimits (kMinW, kMinH, kMaxW, kMaxH);
    const int w = p.settings().getIntValue ("editorW", kDefaultW);
    const int h = p.settings().getIntValue ("editorH", kDefaultH);
    setSize (juce::jlimit (kMinW, kMaxW, w), juce::jlimit (kMinH, kMaxH, h));
    setWantsKeyboardFocus (false);
}

InterferenceEditor::~InterferenceEditor()
{
    proc.settings().setValue ("editorW", getWidth());
    proc.settings().setValue ("editorH", getHeight());
    proc.settings().saveIfNeeded();
    mainView = nullptr;
    tooltips = nullptr;
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == &lnf)
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void InterferenceEditor::setTooltips (bool on)
{
    if (on && tooltips == nullptr)
    {
        tooltips = std::make_unique<juce::TooltipWindow> (this, 400); // 400 ms hover
        tooltips->setLookAndFeel (&lnf);
    }
    else if (! on)
        tooltips = nullptr;
}

void InterferenceEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
}

void InterferenceEditor::resized()
{
    mainView->setBounds (getLocalBounds());
}

} // namespace ifx
