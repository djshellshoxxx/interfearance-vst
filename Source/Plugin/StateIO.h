#pragma once

#include <JuceHeader.h>
#include "Core/Params.h"
#include "Core/Pattern.h"
#include "DSP/Source.h"

namespace ifx
{

/** One MIDI-learn assignment: a controller on a channel (0 = any) drives a parameter. */
struct MidiMapping
{
    int channel = 0;   // 0 = omni, 1..16
    int cc = 1;        // 0..127
    int param = -1;    // index into P

    bool operator== (const MidiMapping& o) const noexcept { return channel == o.channel && cc == o.cc && param == o.param; }
};

/** Everything that defines a sound: host parameters plus the non-parameter state.
    Pure data, independent of the AudioProcessor, so presets, project state, A/B slots and
    the offline renderer all share one representation. Nothing machine-specific (paths,
    device names, pointers, audio) is ever stored here. */
struct PluginState
{
    static constexpr int kVersion = 1;
    static constexpr int kMaxMappings = 128;

    ParamSnapshot params;
    uint64_t seed = 0x1F2E3D4Cull;
    bool seedLock = true;
    uint32_t take = 0;
    SourceType slotA = SourceType::LooseJack;
    SourceType slotB = SourceType::None;
    Pattern pattern;
    juce::Array<MidiMapping> midiMap;

    // preset metadata (never affects sound)
    juce::String presetName { "Init" };
    juce::String category;
    juce::String description;

    /** Effective range policy label (Natural / Expressive) as saved with presets and exports. */
    bool isExpressive() const noexcept { return params.choice (P::policy) == 1; }
    /** True if the audible result is Studio-processed or uses internal excitation. */
    bool usesStudio() const noexcept { return params.on (P::studioOn) && ! params.on (P::rawHw); }
    bool usesExcitation() const noexcept { return params.choice (P::excType) != 0; }

    bool soundEquals (const PluginState& o) const noexcept;
};

namespace StateIO
{
    /** Serialise to a ValueTree (type "InterferenceState"). */
    juce::ValueTree toValueTree (const PluginState& s);

    /** Parse a ValueTree. Unknown fields are ignored, missing fields keep defaults, every
        value is clamped to its legal range and non-finite values are replaced by defaults.
        Returns false only when the tree is not an Interference state at all; `warnings`
        collects anything that was repaired. */
    bool fromValueTree (const juce::ValueTree& tree, PluginState& s, juce::StringArray* warnings = nullptr);

    /** Binary blob for the host (getStateInformation). */
    void toBinary (const PluginState& s, juce::MemoryBlock& dest);
    bool fromBinary (const void* data, int size, PluginState& s, juce::StringArray* warnings = nullptr);

    /** Preset files (.ifxpreset, XML). */
    bool savePresetFile (const PluginState& s, const juce::File& file, juce::String& error);
    bool loadPresetFile (const juce::File& file, PluginState& s, juce::String& error, juce::StringArray* warnings = nullptr);

    /** Root of the user data folders: Documents/Interference by default. A test build or a
        portable setup can redirect it (presets, support files and the options file) with
        setDataRootOverride() or the environment variable IFX_DATA_ROOT (an absolute path);
        the programmatic override wins. */
    juce::File userDataFolder();
    /** The redirected root, or juce::File() when neither override is set. */
    juce::File dataRootOverride();
    /** Message thread / test setup only. Pass juce::File() to clear. */
    void setDataRootOverride (const juce::File& root);

    /** Default user preset folder: <userDataFolder()>/Presets (Documents/Interference/Presets). */
    juce::File userPresetFolder();
    constexpr const char* kPresetExtension = ".ifxpreset";

    /** Pattern helpers (also used by the lane editor's copy / paste). */
    juce::ValueTree patternToTree (const Pattern& p);
    void patternFromTree (const juce::ValueTree& t, Pattern& p);

    /** Parameter value -> clamped, finite legal value. */
    float sanitise (int param, float value) noexcept;
}

} // namespace ifx
