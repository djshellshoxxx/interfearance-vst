#include "PluginProcessor.h"
#if ! IFX_HEADLESS || IFX_GUI_TESTS
 #include "PluginEditor.h"
#endif

namespace ifx
{

namespace
{
    juce::String unitSuffix (const ParamInfo& info)
    {
        const juce::String u (info.unit);
        return u.isEmpty() || u == "%" ? juce::String() : " " + u;
    }

    juce::NormalisableRange<float> rangeFor (const ParamInfo& info)
    {
        juce::NormalisableRange<float> r (info.min, info.max);
        if (info.skew == 1 && info.min > 0.0f)
        {
            // logarithmic: equal ratios per equal travel
            const float lo = info.min, hi = info.max;
            r = juce::NormalisableRange<float> (lo, hi,
                [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                [] (float, float, float v) { return v; });
        }
        return r;
    }

    juce::String formatValue (const ParamInfo& info, float v)
    {
        const juce::String unit (info.unit);
        if (unit == "%") return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        if (unit == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 100.0f ? 1 : 0) + " Hz";
        if (unit == "dB") return juce::String (v, 1) + " dB";
        if (unit == "ms") return juce::String (v, v < 10.0f ? 1 : 0) + " ms";
        if (unit == "s") return juce::String (v, 2) + " s";
        if (unit == "pF") return juce::String (juce::roundToInt (v)) + " pF";
        if (unit == "BPM") return juce::String (v, 1) + " BPM";
        if (unit == "bit") return juce::String (v, 1) + " bit";
        if (unit == "x") return juce::String (v, 2) + " x";
        return juce::String (v, 2) + unitSuffix (info);
    }

    float parseValue (const ParamInfo& info, const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        float v = t.getFloatValue();
        if (juce::String (info.unit) == "%") v *= 0.01f;
        if (juce::String (info.unit) == "Hz" && t.contains ("k")) v *= 1000.0f;
        return v;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout InterferenceProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& info = paramInfo (i);
        const juce::ParameterID pid (info.id, 1); // version hint 1: every shipped ID stays stable
        switch (info.kind)
        {
            case ParamKind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, info.name, info.def >= 0.5f));
                break;
            case ParamKind::Choice:
            {
                const auto choices = juce::StringArray::fromTokens (info.choices, "|", "");
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, info.name, choices, (int) info.def));
                break;
            }
            case ParamKind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, info.name, (int) info.min, (int) info.max, (int) info.def));
                break;
            case ParamKind::Float:
            default:
            {
                auto attrs = juce::AudioParameterFloatAttributes()
                                 .withLabel (info.unit)
                                 .withStringFromValueFunction ([&info] (float v, int) { return formatValue (info, v); })
                                 .withValueFromStringFunction ([&info] (const juce::String& s) { return parseValue (info, s); });
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, info.name, rangeFor (info), info.def, attrs));
                break;
            }
        }
    }
    return layout;
}

InterferenceProcessor::InterferenceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        raw[i] = apvts.getRawParameterValue (paramInfo (i).id);
        params[i] = apvts.getParameter (paramInfo (i).id);
        jassert (raw[i] != nullptr && params[i] != nullptr);
        lastValues[i] = raw[i]->load();
    }
    for (auto& ch : ccTable)
        for (auto& c : ch)
            c.store (-1);

    juce::PropertiesFile::Options o;
    o.applicationName = "Interference";
    o.filenameSuffix = ".settings";
    o.folderName = "Interference";
    o.osxLibrarySubFolder = "Application Support";
    o.commonToAllUsers = false;
    settingsFile = std::make_unique<juce::PropertiesFile> (o);

    loadedState = FactoryPresets::initState();
    abStates[0] = abStates[1] = loadedState;
    pushNonParamToEngine();
    startTimerHz (30);
}

InterferenceProcessor::~InterferenceProcessor()
{
    stopTimer();
    liveRecorder.stop();
}

//==============================================================================
bool InterferenceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    // mono->mono or stereo->stereo only: no implicit sum, duplication or swap
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in != out)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void InterferenceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateNow = sampleRate;
    const int ch = std::max (1, std::min (2, getMainBusNumOutputChannels()));
    engine.prepare (sampleRate, std::max (1, samplesPerBlock), ch);
    pushNonParamToEngine();
    engine.applyPendingNow();
    captureBuf.setSize (2, std::max (1, samplesPerBlock));
    setLatencySamples (0); // zero-latency design (ADAA instead of oversampling, no lookahead)
}

void InterferenceProcessor::releaseResources() {}

double InterferenceProcessor::getTailLengthSeconds() const
{
    return std::clamp ((double) engine.getTailSeconds(), 0.0, 30.0);
}

void InterferenceProcessor::snapshotParams() noexcept
{
    for (int i = 0; i < kNumParams; ++i)
        snap[i] = raw[i]->load (std::memory_order_relaxed);
}

void InterferenceProcessor::translateMidi (const juce::MidiBuffer& midi, int numSamples) noexcept
{
    const int learn = learnParam.load();
    for (const auto meta : midi)
    {
        if (numEvents >= kMaxEvents)
            break;
        const auto m = meta.getMessage();
        const int off = std::clamp (meta.samplePosition, 0, std::max (0, numSamples - 1));
        if (m.isNoteOn())
            events[numEvents++] = { off, ControlEvent::NoteOn, m.getNoteNumber(), m.getFloatVelocity() };
        else if (m.isNoteOff())
            events[numEvents++] = { off, ControlEvent::NoteOff, m.getNoteNumber(), 0.0f };
        else if (m.isChannelPressure())
            events[numEvents++] = { off, ControlEvent::Pressure, 0, m.getChannelPressureValue() / 127.0f };
        else if (m.isAftertouch())
            events[numEvents++] = { off, ControlEvent::Pressure, m.getNoteNumber(), m.getAfterTouchValue() / 127.0f };
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            const float v = m.getControllerValue() / 127.0f;
            const int chn = std::clamp (m.getChannel(), 1, 16);
            // learned mappings go to the message thread (host-visible parameter change)
            int target = learn >= 0 ? learn : ccTable[chn][cc].load (std::memory_order_relaxed);
            if (target < 0) target = ccTable[0][cc].load (std::memory_order_relaxed);
            if (target >= 0 || learn >= 0)
            {
                int start1, size1, start2, size2;
                ccFifo.prepareToWrite (1, start1, size1, start2, size2);
                if (size1 > 0)
                {
                    ccQueue[start1] = { (int16_t) target, (int16_t) (learn >= 0 ? chn : 0), (int16_t) cc, v };
                    ccFifo.finishedWrite (1);
                }
                if (learn >= 0)
                    continue; // a learning CC does not also act as a gesture controller
            }
            events[numEvents++] = { off, ControlEvent::Controller, cc, v };
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            events[numEvents++] = { off, ControlEvent::Controller, 123, 0.0f };
    }
}

void InterferenceProcessor::pullUiEvents (int numSamples) noexcept
{
    juce::ignoreUnused (numSamples);
    int start1, size1, start2, size2;
    uiFifo.prepareToRead (uiFifo.getNumReady(), start1, size1, start2, size2);
    auto drain = [this] (int start, int size)
    {
        for (int i = 0; i < size && numEvents < kMaxEvents; ++i)
        {
            auto e = uiQueue[start + i];
            e.offset = 0; // GUI gestures act at the start of the next block
            events[numEvents++] = e;
        }
    };
    drain (start1, size1);
    drain (start2, size2);
    uiFifo.finishedRead (size1 + size2);
}

void InterferenceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;

    snapshotParams();

    // UI events first (offset 0), then MIDI in time order
    numEvents = 0;
    pullUiEvents (n);
    translateMidi (midi, n);
    // stable insertion sort (bounded, no allocation: std::stable_sort may allocate)
    for (int i = 1; i < numEvents; ++i)
    {
        const auto e = events[i];
        int j = i - 1;
        while (j >= 0 && events[j].offset > e.offset) { events[j + 1] = events[j]; --j; }
        events[j + 1] = e;
    }

    TransportInfo tr;
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            tr.playing = pos->getIsPlaying();
            if (const auto ppq = pos->getPpqPosition()) { tr.ppq = *ppq; tr.hasHostTransport = true; }
            if (const auto bpm = pos->getBpm()) tr.bpm = *bpm;
            if (const auto sig = pos->getTimeSignature()) { tr.sigNum = sig->numerator; tr.sigDen = sig->denominator; }
            if (const auto sec = pos->getTimeInSeconds()) tr.hostSeconds = *sec;
        }
    }
    if (! tr.hasHostTransport)
        tr.playing = false; // no musical position: internal clock fallback (Run)
    tr.offline = isNonRealtime();

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int nch = std::min (2, mainOut.getNumChannels());
    if (nch <= 0)
        return;

    const float* in[2] = { mainIn.getNumChannels() > 0 ? mainIn.getReadPointer (0) : nullptr,
                           mainIn.getNumChannels() > 1 ? mainIn.getReadPointer (1) : (mainIn.getNumChannels() > 0 ? mainIn.getReadPointer (0) : nullptr) };
    float* out[2] = { mainOut.getWritePointer (0), mainOut.getWritePointer (nch > 1 ? 1 : 0) };

    const float* sc[2] = { nullptr, nullptr };
    int scCh = 0;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto side = getBusBuffer (buffer, true, 1);
        if (side.getNumChannels() > 0)
        {
            scCh = std::min (2, side.getNumChannels());
            sc[0] = side.getReadPointer (0);
            sc[1] = side.getReadPointer (scCh > 1 ? 1 : 0);
        }
    }

    const bool rec = liveRecorder.isRecording() && captureBuf.getNumSamples() >= n;
    float* cap[2] = { nullptr, nullptr };
    if (rec)
    {
        cap[0] = captureBuf.getWritePointer (0);
        cap[1] = captureBuf.getWritePointer (1);
    }

    // in and out alias in JUCE's in-place buffer: the engine reads a sample before writing it
    engine.process (in, out, n, snap, tr, events, numEvents, scCh > 0 ? sc : nullptr, scCh,
                    rec ? cap : nullptr, (Tap) recordTap.load());

    if (rec)
    {
        const float* c[2] = { captureBuf.getReadPointer (0), captureBuf.getReadPointer (nch > 1 ? 1 : 0) };
        liveRecorder.push (c, nch, n);
    }

    for (int ch = nch; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

//==============================================================================
void InterferenceProcessor::pushControlEvent (const ControlEvent& e) noexcept
{
    int start1, size1, start2, size2;
    uiFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        uiQueue[start1] = e;
        uiFifo.finishedWrite (1);
    }
}

void InterferenceProcessor::pushNonParamToEngine()
{
    engine.setSeed (seed);
    engine.setSeedLock (seedLock);
    engine.setTake (take);
    engine.setSlotTypes (slotA, slotB);
    engine.patternExchange().publish (pattern);
    rebuildCcTable();
}

void InterferenceProcessor::rebuildCcTable()
{
    for (auto& ch : ccTable)
        for (auto& c : ch)
            c.store (-1);
    for (const auto& m : midiMap)
        if (m.param >= 0 && m.param < kNumParams)
            ccTable[std::clamp (m.channel, 0, 16)][std::clamp (m.cc, 0, 127)].store ((int16_t) m.param);
}

void InterferenceProcessor::setSlotSource (int slot, SourceType t)
{
    SourceType a = slotA, b = slotB;
    SourceType& mine = slot == 0 ? a : b;
    SourceType& other = slot == 0 ? b : a;
    if (t != SourceType::None && other == t)
        other = mine; // moving a source between slots swaps them: never a duplicate
    mine = t;
    setSlots (a, b);
}

void InterferenceProcessor::setSlots (SourceType a, SourceType b)
{
    if (a != SourceType::None && a == b)
        b = SourceType::None;
    slotA = a;
    slotB = b;
    engine.setSlotTypes (a, b);
    noteActivity ("slots: A=" + juce::String (sourceName (a)) + "  B=" + juce::String (sourceName (b)));
    ++uiDirty;
}

void InterferenceProcessor::setSeed (uint64_t s)
{
    seed = s;
    engine.setSeed (s);
    noteActivity ("seed " + juce::String::toHexString ((juce::int64) s).toUpperCase());
    ++uiDirty;
}

void InterferenceProcessor::setSeedLock (bool l)
{
    seedLock = l;
    engine.setSeedLock (l);
    ++uiDirty;
}

void InterferenceProcessor::newTake()
{
    ++take;
    engine.setTake (take);
    noteActivity ("new take " + juce::String ((int) take));
    ++uiDirty;
}

void InterferenceProcessor::setPattern (const Pattern& p)
{
    pattern = p;
    engine.patternExchange().publish (pattern);
    ++uiDirty;
}

//==============================================================================
PluginState InterferenceProcessor::getCurrentState() const
{
    PluginState s;
    for (int i = 0; i < kNumParams; ++i)
        s.params[i] = StateIO::sanitise (i, raw[i]->load());
    s.seed = seed;
    s.seedLock = seedLock;
    s.take = take;
    s.slotA = slotA;
    s.slotB = slotB;
    s.pattern = pattern;
    s.midiMap = midiMap;
    s.presetName = presetName;
    s.category = presetCategory;
    s.description = presetDescription;
    return s;
}

void InterferenceProcessor::applyState (const PluginState& s, bool markAsLoaded)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        if (i == P::bypass)
            continue; // presets never bypass the plugin
        auto* p = params[i];
        const float norm = p->convertTo0to1 (StateIO::sanitise (i, s.params[i]));
        if (std::abs (p->getValue() - norm) > 1.0e-7f)
            p->setValueNotifyingHost (norm);
    }
    // momentary controls never load "held"
    for (int i : { (int) P::trigA, (int) P::trigB, (int) P::fill, (int) P::killFb, (int) P::killTail })
        if (params[i]->getValue() > 0.5f)
            params[i]->setValueNotifyingHost (0.0f);
    seed = s.seed;
    seedLock = s.seedLock;
    take = s.take;
    slotA = s.slotA;
    slotB = (s.slotB == s.slotA && s.slotA != SourceType::None) ? SourceType::None : s.slotB;
    pattern = s.pattern;
    midiMap = s.midiMap;
    presetName = s.presetName;
    presetCategory = s.category;
    presetDescription = s.description;
    pushNonParamToEngine();
    if (markAsLoaded)
        loadedState = getCurrentState();
    noteActivity ("state: " + presetName);
    ++uiDirty;
}

bool InterferenceProcessor::isModified() const
{
    auto cur = getCurrentState();
    auto ref = loadedState;
    // host bypass / momentary controls are not part of a sound
    for (int i : { (int) P::bypass, (int) P::trigA, (int) P::trigB, (int) P::fill, (int) P::killFb, (int) P::killTail })
        cur.params[i] = ref.params[i];
    cur.midiMap = ref.midiMap;
    return ! cur.soundEquals (ref);
}

void InterferenceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto s = getCurrentState();
    auto t = StateIO::toValueTree (s);
    // project-only extras (never in preset files): A/B slots and which one is active
    t.setProperty ("abIndex", abIndex, nullptr);
    for (int i = 0; i < 2; ++i)
        if (abValid[i] && i != abIndex)
        {
            auto ab = StateIO::toValueTree (abStates[i]);
            juce::ValueTree holder ("AB" + juce::String (i));
            holder.appendChild (ab, nullptr);
            t.appendChild (holder, nullptr);
        }
    t.setProperty ("loadedPreset", StateIO::toValueTree (loadedState).toXmlString (juce::XmlElement::TextFormat().singleLine()), nullptr);
    juce::MemoryOutputStream mo (destData, false);
    t.writeToStream (mo);
}

void InterferenceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    PluginState s;
    juce::StringArray warnings;
    if (! StateIO::fromBinary (data, sizeInBytes, s, &warnings))
        return; // malformed: keep the current (valid) state

    applyState (s, false);
    loadedState = s;
    const auto t = juce::ValueTree::readFromData (data, (size_t) std::max (0, sizeInBytes));
    if (t.isValid())
    {
        if (auto xml = juce::parseXML (t["loadedPreset"].toString()))
        {
            PluginState ref;
            if (StateIO::fromValueTree (juce::ValueTree::fromXml (*xml), ref))
                loadedState = ref;
        }
        abIndex = std::clamp ((int) t["abIndex"], 0, 1);
        abValid[0] = abValid[1] = false;
        for (int i = 0; i < 2; ++i)
        {
            const auto holder = t.getChildWithName ("AB" + juce::String (i));
            if (holder.isValid() && StateIO::fromValueTree (holder.getChild (0), abStates[i]))
                abValid[i] = true;
        }
    }
    for (const auto& w : warnings)
        noteActivity ("state repair: " + w);
}

//==============================================================================
int InterferenceProcessor::getNumPrograms()
{
    return std::max (1, FactoryPresets::all().size());
}

void InterferenceProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, FactoryPresets::all().size()))
        loadFactoryPreset (index);
}

const juce::String InterferenceProcessor::getProgramName (int index)
{
    const auto& bank = FactoryPresets::all();
    return juce::isPositiveAndBelow (index, bank.size()) ? bank[index].category + ": " + bank[index].name : juce::String();
}

void InterferenceProcessor::loadFactoryPreset (int index)
{
    const auto& bank = FactoryPresets::all();
    if (! juce::isPositiveAndBelow (index, bank.size()))
        return;
    auto s = bank[index].state;
    s.midiMap = midiMap; // MIDI mappings are a user setup, kept across presets
    currentProgram = index;
    applyState (s);
}

bool InterferenceProcessor::loadPresetFile (const juce::File& f, juce::String& error)
{
    PluginState s;
    juce::StringArray warnings;
    if (! StateIO::loadPresetFile (f, s, error, &warnings))
        return false;
    if (s.midiMap.isEmpty())
        s.midiMap = midiMap;
    applyState (s);
    for (const auto& w : warnings)
        noteActivity ("preset repair: " + w);
    return true;
}

bool InterferenceProcessor::savePresetFile (const juce::File& f, juce::String& error)
{
    auto s = getCurrentState();
    s.presetName = f.getFileNameWithoutExtension();
    if (! StateIO::savePresetFile (s, f, error))
        return false;
    presetName = s.presetName;
    loadedState = getCurrentState();
    ++uiDirty;
    return true;
}

void InterferenceProcessor::selectAB (int index)
{
    index = std::clamp (index, 0, 1);
    if (index == abIndex)
        return;
    abStates[abIndex] = getCurrentState();
    abValid[abIndex] = true;
    abIndex = index;
    if (abValid[index])
    {
        const auto ref = loadedState;
        applyState (abStates[index], false);
        loadedState = ref;
    }
    ++uiDirty;
}

void InterferenceProcessor::copyABTo (int index)
{
    index = std::clamp (index, 0, 1);
    abStates[index] = getCurrentState();
    abValid[index] = true;
    ++uiDirty;
}

void InterferenceProcessor::resetToDefaults()
{
    auto s = FactoryPresets::initState();
    s.midiMap = midiMap; // Reset restores sound settings; Hard Reset (Help > Debug) also clears MIDI mappings
    currentProgram = 0;
    randomizedOnce = false;
    applyState (s);
}

void InterferenceProcessor::hardReset()
{
    liveRecorder.stop();
    midiMap.clear();
    abValid[0] = abValid[1] = false;
    abIndex = 0;
    settingsFile->clear();
    settingsFile->saveIfNeeded();
    applyState (FactoryPresets::initState());
    engine.requestReset();
}

void InterferenceProcessor::randomize()
{
    // include.md: each press resets everything first, then randomises (Natural, safe gain)
    auto s = FactoryPresets::initState();
    s.midiMap = midiMap;
    auto& r = juce::Random::getSystemRandom();
    auto& p = s.params;
    auto u = [&r] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };

    const SourceType all[4] = { SourceType::LooseJack, SourceType::PlugTouch, SourceType::CableHandling, SourceType::DirtyPot };
    const int a = r.nextInt (4);
    int b = r.nextInt (5) - 1; // -1 = empty
    if (b == a) b = -1;
    s.slotA = all[a];
    s.slotB = b >= 0 ? all[b] : SourceType::None;
    s.seed = (uint64_t) r.nextInt64();

    p[P::hum] = u (0.0f, 0.5f);
    p[P::mains] = (float) r.nextInt (2);
    p[P::stereo] = u (0.0f, 0.5f);
    p[P::jkInsert] = u (0.6f, 1.0f); p[P::jkWiggle] = u (0.0f, 0.6f); p[P::jkPressure] = u (0.2f, 0.9f);
    p[P::jkWear] = u (0.1f, 0.9f); p[P::jkWearPos] = u (0.5f, 1.0f); p[P::jkIntermit] = u (0.1f, 0.8f);
    p[P::jkRecovery] = u (0.2f, 0.9f); p[P::jkShort] = u (0.0f, 0.6f); p[P::jkPop] = u (0.0f, 0.5f); p[P::jkShield] = u (0.3f, 1.0f);
    p[P::tcTouch] = r.nextBool() ? u (0.0f, 0.5f) : 0.0f; p[P::tcPressure] = u (0.1f, 0.9f); p[P::tcBody] = u (0.0f, 1.0f);
    p[P::tcShield] = u (0.0f, 0.7f); p[P::tcExposure] = u (0.3f, 1.0f); p[P::tcPop] = u (0.0f, 0.5f);
    p[P::cbBend] = u (0.0f, 1.0f); p[P::cbShake] = u (0.0f, 0.5f); p[P::cbDamage] = u (0.1f, 0.9f); p[P::cbShieldDmg] = u (0.0f, 0.7f);
    p[P::cbMic] = r.nextBool() ? u (0.0f, 0.6f) : 0.0f; p[P::cbWeakPos] = u (0.1f, 0.9f); p[P::cbIntermit] = u (0.1f, 0.8f);
    p[P::ptTurn] = u (0.3f, 1.0f); p[P::ptTaper] = (float) r.nextInt (3); p[P::ptCircuit] = (float) r.nextInt (2);
    p[P::ptDirt] = u (0.1f, 0.9f); p[P::ptWearPos] = u (0.1f, 0.9f); p[P::ptWearWidth] = u (0.05f, 0.3f); p[P::ptScratch] = u (0.2f, 0.9f);

    if (r.nextBool())
    {
        // a random but sparse lane
        p[P::laneOn] = 1.0f;
        p[P::mode] = 1.0f;
        for (int lane = 0; lane < 2; ++lane)
            for (int i = 0; i < 16; ++i)
                if (r.nextFloat() < 0.25f)
                {
                    auto& st = s.pattern.lanes[lane].steps[i];
                    st.active = 1;
                    st.intensity = u (0.4f, 0.95f);
                    st.gesture = (uint8_t) r.nextInt (4);
                    st.accent = (uint8_t) (r.nextFloat() < 0.2f);
                }
    }
    if (r.nextFloat() < 0.35f)
    {
        p[P::studioOn] = 1.0f;
        p[P::studioSrc] = (float) r.nextInt (3);
        p[P::eqOn] = (float) r.nextBool();
        p[P::eqHP] = u (20.0f, 600.0f);
        p[P::drOn] = (float) r.nextBool();
        p[P::drPre] = u (3.0f, 18.0f);
        p[P::drPost] = -p[P::drPre] * 0.5f;
        p[P::ecOn] = (float) (r.nextFloat() < 0.3f);
        p[P::ceilOn] = 1.0f;
    }
    for (int i = 0; i < kNumParams; ++i)
        p[i] = StateIO::sanitise (i, p[i]);
    s.presetName = "Random";
    s.category = {};
    randomizedOnce = true;
    applyState (s);
}

void InterferenceProcessor::clearMidiMapping (int paramIndex)
{
    for (int i = midiMap.size(); --i >= 0;)
        if (midiMap.getReference (i).param == paramIndex)
            midiMap.remove (i);
    rebuildCcTable();
    ++uiDirty;
}

juce::String InterferenceProcessor::describeMidiMapping (int paramIndex) const
{
    for (const auto& m : midiMap)
        if (m.param == paramIndex)
            return "CC " + juce::String (m.cc) + (m.channel > 0 ? " ch " + juce::String (m.channel) : juce::String (" (any channel)"));
    return {};
}

//==============================================================================
bool InterferenceProcessor::tooltipsEnabled() const { return settingsFile->getBoolValue ("tooltips", true); }

void InterferenceProcessor::setTooltipsEnabled (bool b)
{
    settingsFile->setValue ("tooltips", b);
    settingsFile->saveIfNeeded();
    ++uiDirty;
}

void InterferenceProcessor::noteActivity (const juce::String& s)
{
    const juce::ScopedLock sl (activityLock);
    activity.add (s);
    if (activity.size() > 200)
        activity.removeRange (0, activity.size() - 200);
}

juce::StringArray InterferenceProcessor::drainActivity()
{
    const juce::ScopedLock sl (activityLock);
    auto a = activity;
    activity.clear();
    return a;
}

void InterferenceProcessor::timerCallback()
{
    // learned / mapped CCs -> host-visible parameter changes (message thread)
    int start1, size1, start2, size2;
    ccFifo.prepareToRead (ccFifo.getNumReady(), start1, size1, start2, size2);
    auto apply = [this] (int start, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            const auto m = ccQueue[start + i];
            const int learn = learnParam.load();
            if (learn >= 0)
            {
                clearMidiMapping (learn);
                for (int k = midiMap.size(); --k >= 0;) // one parameter per CC/channel
                    if (midiMap.getReference (k).cc == m.cc && midiMap.getReference (k).channel == m.channel)
                        midiMap.remove (k);
                midiMap.add ({ m.channel, m.cc, learn });
                rebuildCcTable();
                learnParam.store (-1);
                noteActivity ("MIDI learn: CC " + juce::String (m.cc) + " -> " + paramInfo (learn).name);
                ++uiDirty;
                continue;
            }
            if (m.param >= 0 && m.param < kNumParams)
                params[m.param]->setValueNotifyingHost (m.value);
        }
    };
    apply (start1, size1);
    apply (start2, size2);
    ccFifo.finishedRead (size1 + size2);

    // parameter changes -> data stream
    for (int i = 0; i < kNumParams; ++i)
    {
        const float v = raw[i]->load();
        if (v != lastValues[i])
        {
            lastValues[i] = v;
            noteActivity (juce::String (paramInfo (i).name) + " = " + params[i]->getCurrentValueAsText());
        }
    }

    // engine event log -> data stream
    const auto& t = engine.telemetry;
    const uint32_t w = t.logWrite.load (std::memory_order_acquire);
    if (w - lastLogRead > (uint32_t) EngineTelemetry::kLogSize)
        lastLogRead = w - (uint32_t) EngineTelemetry::kLogSize;
    for (; lastLogRead != w; ++lastLogRead)
    {
        const auto e = t.log[lastLogRead % EngineTelemetry::kLogSize];
        switch (e.code)
        {
            case LogGesture:   noteActivity ("gesture " + juce::String (e.a / 100 == 0 ? "A " : "B ") + gestureName ((GestureKind) (e.a % 100)) + " " + juce::String (e.b, 2)); break;
            case LogCoalesced: noteActivity ("motion limit: gesture coalesced"); break;
            case LogNoteOn:    noteActivity ("midi note on " + juce::String (e.a) + " vel " + juce::String (e.b, 2)); break;
            case LogNoteOff:   noteActivity ("midi note off " + juce::String (e.a)); break;
            case LogCC:        noteActivity ("midi cc " + juce::String (e.a) + " = " + juce::String (e.b, 2)); break;
            case LogLaneStep:  noteActivity ("lane " + juce::String (e.a / 100 == 0 ? "A" : "B") + " step " + juce::String (e.a % 100 + 1)); break;
            case LogChoke:     noteActivity ("choke"); break;
            case LogReset:     noteActivity ("reset (" + juce::String (e.a == 0 ? "transport start" : e.a == 1 ? "seek" : e.a == 2 ? "loop" : "prepare") + ")"); break;
            case LogReact:     noteActivity ("sidechain react " + juce::String (e.b, 2)); break;
            case LogBar:       noteActivity ("bar " + juce::String (e.a + 1)); break;
            default: break;
        }
    }
}

juce::AudioProcessorEditor* InterferenceProcessor::createEditor()
{
#if ! IFX_HEADLESS || IFX_GUI_TESTS
    return new InterferenceEditor (*this);
#else
    return nullptr;
#endif
}

} // namespace ifx

#if ! IFX_HEADLESS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ifx::InterferenceProcessor();
}
#endif
