#pragma once

#include <JuceHeader.h>

namespace ifx
{

/** Contact / branding values in one place (placeholders until the publisher sets them). */
namespace Branding
{
    constexpr const char* productName = "Interference";
    constexpr const char* company = "Interference Audio";
    constexpr const char* homepage = "https://interference-audio.example";
    constexpr const char* supportEmail = "support@interference-audio.example";
    constexpr const char* sourceRepository = ""; // closed source: no public repository
    constexpr const char* licenseSummary =
        "Interference is proprietary software, (c) 2026 Sheldon Davidson. All rights reserved. "
        "Built with JUCE 8 (see THIRD_PARTY.md for the JUCE licence and other notices).";
}

namespace HelpContent
{
    struct Section { juce::String title; juce::String body; };
    const juce::Array<Section>& sections();
}

} // namespace ifx
