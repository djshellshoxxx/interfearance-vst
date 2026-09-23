#pragma once

#include "StateIO.h"

namespace ifx
{

struct FactoryPreset
{
    juce::String name;
    juce::String category;
    juce::String description;
    PluginState state;
};

namespace FactoryPresets
{
    /** Category order shown in the preset browser. */
    const juce::StringArray& categories();

    /** The factory bank (built once, message thread). Every preset starts from the plugin
        defaults, so presets saved with an older version stay valid when parameters are added. */
    const juce::Array<FactoryPreset>& all();

    /** Default state ("Init"): what Reset returns to. */
    PluginState initState();
}

} // namespace ifx
