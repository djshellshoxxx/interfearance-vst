#include "StateIO.h"

namespace ifx
{

bool PluginState::soundEquals (const PluginState& o) const noexcept
{
    for (int i = 0; i < kNumParams; ++i)
        if (params[i] != o.params[i])
            return false;
    return seed == o.seed && seedLock == o.seedLock && take == o.take && slotA == o.slotA && slotB == o.slotB
        && pattern == o.pattern && midiMap == o.midiMap;
}

namespace StateIO
{

namespace
{
    const juce::Identifier kRoot ("InterferenceState");
    const juce::Identifier kPresetRoot ("InterferencePreset");
    const juce::Identifier kParams ("Params");
    const juce::Identifier kPattern ("Pattern");
    const juce::Identifier kLane ("Lane");
    const juce::Identifier kStep ("Step");
    const juce::Identifier kMidi ("MidiMap");
    const juce::Identifier kMap ("Map");

    juce::String seedToString (uint64_t s) { return juce::String::toHexString ((juce::int64) s); }

    uint64_t seedFromVar (const juce::var& v, uint64_t fallback)
    {
        if (v.isVoid())
            return fallback;
        const auto str = v.toString().trim();
        if (str.isEmpty() || ! str.containsOnly ("0123456789abcdefABCDEF"))
            return fallback;
        return (uint64_t) str.getHexValue64();
    }

    int clampInt (const juce::var& v, int lo, int hi, int fallback)
    {
        if (v.isVoid()) return fallback;
        const double d = (double) v;
        if (! std::isfinite (d)) return fallback;
        return std::clamp ((int) std::llround (d), lo, hi);
    }

    float clampFloat (const juce::var& v, float lo, float hi, float fallback)
    {
        if (v.isVoid()) return fallback;
        const double d = (double) v;
        if (! std::isfinite (d)) return fallback;
        return (float) std::clamp (d, (double) lo, (double) hi);
    }

    SourceType sourceFromVar (const juce::var& v, SourceType fallback)
    {
        const int i = clampInt (v, 0, kNumSourceTypes, (int) fallback);
        return (SourceType) i;
    }
}

float sanitise (int param, float value) noexcept
{
    const auto& info = paramInfo (param);
    if (! std::isfinite (value))
        return info.def;
    value = std::clamp (value, info.min, info.max);
    if (info.kind == ParamKind::Bool)
        return value >= 0.5f ? 1.0f : 0.0f;
    if (info.kind == ParamKind::Choice || info.kind == ParamKind::Int)
        return std::round (value);
    return value;
}

juce::ValueTree patternToTree (const Pattern& p)
{
    juce::ValueTree t (kPattern);
    const LaneStep def;
    for (int li = 0; li < kNumLanes; ++li)
    {
        const auto& lane = p.lanes[li];
        juce::ValueTree l (kLane);
        l.setProperty ("index", li, nullptr);
        l.setProperty ("length", lane.length, nullptr);
        l.setProperty ("mute", (int) lane.mute, nullptr);
        l.setProperty ("solo", (int) lane.solo, nullptr);
        for (int si = 0; si < kMaxSteps; ++si)
        {
            const auto& st = lane.steps[si];
            if (st == def)
                continue; // only non-default steps are stored
            juce::ValueTree s (kStep);
            s.setProperty ("i", si, nullptr);
            s.setProperty ("on", (int) st.active, nullptr);
            s.setProperty ("g", (int) st.gesture, nullptr);
            s.setProperty ("acc", (int) st.accent, nullptr);
            s.setProperty ("rat", (int) st.ratchet, nullptr);
            s.setProperty ("chk", (int) st.choke, nullptr);
            s.setProperty ("int", st.intensity, nullptr);
            s.setProperty ("dur", st.duration, nullptr);
            s.setProperty ("tgt", st.target, nullptr);
            s.setProperty ("snd", st.send, nullptr);
            s.setProperty ("prb", st.probability, nullptr);
            s.setProperty ("ofs", st.offset, nullptr);
            l.appendChild (s, nullptr);
        }
        t.appendChild (l, nullptr);
    }
    return t;
}

void patternFromTree (const juce::ValueTree& t, Pattern& p)
{
    p.clear();
    if (! t.isValid())
        return;
    for (const auto& l : t)
    {
        if (! l.hasType (kLane)) continue;
        const int li = clampInt (l["index"], -1, kNumLanes - 1, -1);
        if (li < 0) continue;
        auto& lane = p.lanes[li];
        lane.length = clampInt (l["length"], 1, kMaxSteps, 16);
        lane.mute = (uint8_t) clampInt (l["mute"], 0, 1, 0);
        lane.solo = (uint8_t) clampInt (l["solo"], 0, 1, 0);
        for (const auto& s : l)
        {
            if (! s.hasType (kStep)) continue;
            const int si = clampInt (s["i"], -1, kMaxSteps - 1, -1);
            if (si < 0) continue;
            LaneStep st;
            st.active = (uint8_t) clampInt (s["on"], 0, 1, 0);
            st.gesture = (uint8_t) clampInt (s["g"], 0, 3, 3);
            st.accent = (uint8_t) clampInt (s["acc"], 0, 1, 0);
            st.ratchet = (uint8_t) clampInt (s["rat"], 1, 4, 1);
            st.choke = (uint8_t) clampInt (s["chk"], 0, 1, 0);
            st.intensity = clampFloat (s["int"], 0.0f, 1.0f, 0.7f);
            st.duration = clampFloat (s["dur"], 0.1f, 8.0f, 1.0f);
            st.target = clampFloat (s["tgt"], -1.0f, 1.0f, -1.0f);
            if (st.target < 0.0f) st.target = -1.0f;
            st.send = clampFloat (s["snd"], 0.0f, 1.0f, 0.5f);
            st.probability = clampFloat (s["prb"], 0.0f, 1.0f, 1.0f);
            st.offset = clampFloat (s["ofs"], -0.5f, 0.5f, 0.0f);
            lane.steps[si] = st;
        }
    }
}

juce::ValueTree toValueTree (const PluginState& s)
{
    juce::ValueTree t (kRoot);
    t.setProperty ("version", PluginState::kVersion, nullptr);
    t.setProperty ("plugin", "Interference", nullptr);
    t.setProperty ("pluginVersion", IFX_VERSION_STRING, nullptr);
    t.setProperty ("seed", seedToString (s.seed), nullptr);
    t.setProperty ("seedLock", s.seedLock, nullptr);
    t.setProperty ("take", (int) s.take, nullptr);
    t.setProperty ("slotA", (int) s.slotA, nullptr);
    t.setProperty ("slotB", (int) s.slotB, nullptr);
    t.setProperty ("presetName", s.presetName, nullptr);
    t.setProperty ("category", s.category, nullptr);
    t.setProperty ("description", s.description, nullptr);
    t.setProperty ("policyLabel", s.isExpressive() ? "Expressive" : "Natural", nullptr);
    t.setProperty ("studioProcessed", s.usesStudio(), nullptr);
    t.setProperty ("internalExcitation", s.usesExcitation(), nullptr);

    juce::ValueTree p (kParams);
    for (int i = 0; i < kNumParams; ++i)
        p.setProperty (paramInfo (i).id, s.params[i], nullptr);
    t.appendChild (p, nullptr);

    t.appendChild (patternToTree (s.pattern), nullptr);

    juce::ValueTree m (kMidi);
    for (const auto& mm : s.midiMap)
    {
        if (mm.param < 0 || mm.param >= kNumParams) continue;
        juce::ValueTree e (kMap);
        e.setProperty ("ch", mm.channel, nullptr);
        e.setProperty ("cc", mm.cc, nullptr);
        e.setProperty ("param", paramInfo (mm.param).id, nullptr); // stable ID, not an index
        m.appendChild (e, nullptr);
    }
    t.appendChild (m, nullptr);
    return t;
}

bool fromValueTree (const juce::ValueTree& tree, PluginState& s, juce::StringArray* warnings)
{
    juce::ValueTree t = tree;
    if (t.hasType (kPresetRoot))
        t = t.getChildWithName (kRoot);
    if (! t.isValid() || ! t.hasType (kRoot))
        return false;

    auto warn = [warnings] (const juce::String& w) { if (warnings != nullptr) warnings->add (w); };

    PluginState out; // defaults for everything missing
    const int version = clampInt (t["version"], 0, 1000, 0);
    if (version > PluginState::kVersion)
        warn ("State was saved by a newer version (" + juce::String (version) + "); unknown fields were ignored.");

    out.seed = seedFromVar (t["seed"], out.seed);
    out.seedLock = t.hasProperty ("seedLock") ? (bool) t["seedLock"] : out.seedLock;
    out.take = (uint32_t) clampInt (t["take"], 0, 1 << 30, 0);
    out.slotA = sourceFromVar (t["slotA"], out.slotA);
    out.slotB = sourceFromVar (t["slotB"], out.slotB);
    if (out.slotA != SourceType::None && out.slotA == out.slotB)
    {
        warn ("Duplicate source in both slots; slot B was emptied.");
        out.slotB = SourceType::None;
    }
    out.presetName = t["presetName"].toString().substring (0, 128);
    if (out.presetName.isEmpty()) out.presetName = "Init";
    out.category = t["category"].toString().substring (0, 64);
    out.description = t["description"].toString().substring (0, 1024);

    const auto p = t.getChildWithName (kParams);
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& info = paramInfo (i);
        if (! p.isValid() || ! p.hasProperty (info.id))
            continue; // new parameter since this state was written: keep default
        const juce::var v = p[info.id];
        const double d = (double) v;
        if (! std::isfinite (d))
        {
            warn (juce::String ("Invalid value for ") + info.name + " replaced by the default.");
            continue;
        }
        const float clean = sanitise (i, (float) d);
        if (clean != (float) d)
            warn (juce::String ("Out-of-range value for ") + info.name + " was clamped.");
        out.params[i] = clean;
    }

    patternFromTree (t.getChildWithName (kPattern), out.pattern);

    const auto m = t.getChildWithName (kMidi);
    for (const auto& e : m)
    {
        if (out.midiMap.size() >= PluginState::kMaxMappings) break;
        const auto id = e["param"].toString();
        int idx = -1;
        for (int i = 0; i < kNumParams; ++i)
            if (id == paramInfo (i).id) { idx = i; break; }
        if (idx < 0) continue;
        MidiMapping mm;
        mm.channel = clampInt (e["ch"], 0, 16, 0);
        mm.cc = clampInt (e["cc"], 0, 127, 1);
        mm.param = idx;
        out.midiMap.add (mm);
    }

    s = out;
    return true;
}

void toBinary (const PluginState& s, juce::MemoryBlock& dest)
{
    dest.reset();
    juce::MemoryOutputStream mo (dest, false);
    toValueTree (s).writeToStream (mo);
}

bool fromBinary (const void* data, int size, PluginState& s, juce::StringArray* warnings)
{
    if (data == nullptr || size <= 0)
        return false;
    const auto* c = static_cast<const char*> (data);
    if (c[0] != '<')
    {
        const auto t = juce::ValueTree::readFromData (data, (size_t) size);
        if (t.isValid() && fromValueTree (t, s, warnings))
            return true;
    }
    // tolerate an XML blob (hand-edited or from a text-based host)
    if (auto xml = juce::parseXML (juce::String::fromUTF8 ((const char*) data, size)))
        return fromValueTree (juce::ValueTree::fromXml (*xml), s, warnings);
    return false;
}

bool savePresetFile (const PluginState& s, const juce::File& file, juce::String& error)
{
    juce::ValueTree root (kPresetRoot);
    root.setProperty ("format", 1, nullptr);
    root.appendChild (toValueTree (s), nullptr);
    auto xml = root.createXml();
    if (xml == nullptr)
    {
        error = "Could not create the preset data.";
        return false;
    }
    if (! file.getParentDirectory().createDirectory())
    {
        error = "Could not create the folder " + file.getParentDirectory().getFullPathName();
        return false;
    }
    juce::TemporaryFile tmp (file);
    if (! xml->writeTo (tmp.getFile()) || ! tmp.overwriteTargetFileWithTemporary())
    {
        error = "Could not write " + file.getFullPathName() + " (is the folder writable?)";
        return false;
    }
    return true;
}

bool loadPresetFile (const juce::File& file, PluginState& s, juce::String& error, juce::StringArray* warnings)
{
    if (! file.existsAsFile())
    {
        error = "File not found: " + file.getFullPathName();
        return false;
    }
    if (file.getSize() > 4 * 1024 * 1024)
    {
        error = "This file is too large to be an Interference preset.";
        return false;
    }
    auto xml = juce::parseXML (file);
    if (xml == nullptr)
    {
        error = "This file is not a readable preset (malformed XML).";
        return false;
    }
    if (! fromValueTree (juce::ValueTree::fromXml (*xml), s, warnings))
    {
        error = "This file is not an Interference preset.";
        return false;
    }
    if (s.presetName == "Init" || s.presetName.isEmpty())
        s.presetName = file.getFileNameWithoutExtension();
    return true;
}

juce::File userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Interference").getChildFile ("Presets");
}

} // namespace StateIO
} // namespace ifx
