// Interference™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once

#include <JuceHeader.h>
#include "DSP/Engine.h"
#include "StateIO.h"
#include "FactoryPresets.h"
#include "Render/OfflineRenderer.h"

namespace ifx
{

/** Host bridge: APVTS parameters (stable IDs from Core/Params.h), non-parameter state,
    MIDI translation / learn, GUI gesture queue, A/B, reset / randomize, live record.
    Everything the audio thread touches is preallocated; it never locks, allocates or
    does IO. */
class InterferenceProcessor final : public juce::AudioProcessor,
                                    private juce::Timer
{
public:
    InterferenceProcessor();
    ~InterferenceProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Interference"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return params[P::bypass]; }

    //==========================================================================
    // Message-thread API used by the editor
    juce::AudioProcessorValueTreeState apvts;
    Engine& getEngine() noexcept { return engine; }
    const EngineTelemetry& telemetry() const noexcept { return engine.telemetry; }
    juce::RangedAudioParameter* param (int index) const noexcept { return params[index]; }

    PluginState getCurrentState() const;
    void applyState (const PluginState& s, bool markAsLoaded = true);

    SourceType slotType (int slot) const noexcept { return slot == 0 ? slotA : slotB; }
    /** Loads a source into a slot. A source already in the other slot is moved (swapped). */
    void setSlotSource (int slot, SourceType t);
    void setSlots (SourceType a, SourceType b);

    uint64_t getSeed() const noexcept { return seed; }
    void setSeed (uint64_t s);
    bool getSeedLock() const noexcept { return seedLock; }
    void setSeedLock (bool l);
    uint32_t getTake() const noexcept { return take; }
    void newTake();

    const Pattern& getPattern() const noexcept { return pattern; }
    void setPattern (const Pattern& p);

    /** GUI gestures / triggers (single producer: message thread). */
    void pushControlEvent (const ControlEvent& e) noexcept;

    // MIDI learn
    void startMidiLearn (int paramIndex) noexcept { learnParam.store (paramIndex); }
    void cancelMidiLearn() noexcept { learnParam.store (-1); }
    int midiLearnTarget() const noexcept { return learnParam.load(); }
    void clearMidiMapping (int paramIndex);
    juce::String describeMidiMapping (int paramIndex) const;
    const juce::Array<MidiMapping>& getMidiMap() const noexcept { return midiMap; }

    // presets / A-B / reset / randomize
    juce::String getPresetName() const { return presetName; }
    juce::String getPresetCategory() const { return presetCategory; }
    bool isModified() const;
    void loadFactoryPreset (int index);
    bool loadPresetFile (const juce::File& f, juce::String& error);
    bool savePresetFile (const juce::File& f, juce::String& error);
    int getABSlot() const noexcept { return abIndex; }
    void selectAB (int index);
    void copyABTo (int index);
    void resetToDefaults();
    void randomize();
    void hardReset();

    // live record
    LiveRecorder& recorder() noexcept { return liveRecorder; }
    std::atomic<int> recordTap { (int) Tap::FullOutput };
    double currentSampleRate() const noexcept { return sampleRateNow; }
    int currentChannels() const noexcept { return getTotalNumOutputChannels() > 1 ? 2 : 1; }

    // Options (persisted in the user settings file, never in presets)
    juce::PropertiesFile& settings() { return *settingsFile; }
    bool tooltipsEnabled() const;
    void setTooltipsEnabled (bool);

    /** Parameter-change / event messages for the editor's data stream (message thread). */
    juce::StringArray drainActivity();
    /** The same messages for other readers (debug window, crash log): lines after `cursor`. */
    juce::StringArray activitySince (juce::int64& cursor) const;

    // Help > Debug: opt-in crash log (off every time the plug-in loads, never saved)
    bool startCrashLog (juce::File& written, juce::String& error);
    void stopCrashLog();
    bool isCrashLogging() const;

    std::atomic<int> uiDirty { 0 }; // bumps when non-parameter state changes (editor refresh)

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override;
    void translateMidi (const juce::MidiBuffer& midi, int numSamples) noexcept;
    void pullUiEvents (int numSamples) noexcept;
    void snapshotParams() noexcept;
    void pushNonParamToEngine();
    void rebuildCcTable();
    void noteActivity (const juce::String& s);
    juce::int64 pendingActivity (juce::int64 cursor) const; // history lines after `cursor`

    Engine engine;
    std::atomic<float>* raw[kNumParams] {};
    juce::RangedAudioParameter* params[kNumParams] {};
    ParamSnapshot snap;

    // non-parameter state (message thread owns; engine receives copies)
    uint64_t seed = PluginState().seed;
    bool seedLock = true;
    uint32_t take = 0;
    SourceType slotA = SourceType::LooseJack, slotB = SourceType::None;
    Pattern pattern;
    juce::Array<MidiMapping> midiMap;
    juce::String presetName { "Init" }, presetCategory, presetDescription;
    PluginState loadedState;
    int currentProgram = 0;

    // A/B
    PluginState abStates[2];
    bool abValid[2] { false, false };
    int abIndex = 0;
    bool randomizedOnce = false;

    // audio-thread buffers
    static constexpr int kMaxEvents = 1024;
    ControlEvent events[kMaxEvents];
    int numEvents = 0;
    juce::AbstractFifo uiFifo { 256 };
    ControlEvent uiQueue[256];
    juce::AudioBuffer<float> captureBuf, silentSidechain;
    double sampleRateNow = 48000.0;

    // MIDI CC -> parameter (audio thread reads, message thread writes)
    std::atomic<int16_t> ccTable[17][128];
    std::atomic<int> learnParam { -1 };
    struct CcMsg { int16_t param; int16_t channel; int16_t cc; float value; };
    juce::AbstractFifo ccFifo { 512 };
    CcMsg ccQueue[512];

    LiveRecorder liveRecorder;
    std::unique_ptr<juce::PropertiesFile> settingsFile;

    // parameter activity for the data stream
    float lastValues[kNumParams] {};
    juce::StringArray activity, history;
    juce::int64 historyEnd = 0;   // number of lines ever added to history
    juce::int64 crashCursor = 0;
    int crashTicks = 0;
    juce::CriticalSection activityLock; // message thread only (never the audio thread)
    uint32_t lastLogRead = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InterferenceProcessor)
};

} // namespace ifx
