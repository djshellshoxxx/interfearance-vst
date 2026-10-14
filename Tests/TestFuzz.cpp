// Interference™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

// Hostile-input tests for the state loader: damaged host blobs and preset files must never crash, hang or
// produce values outside the legal ranges (Docs/specs/Quality.md, fuzz layer).
#include "TestHelpers.h"
#include "Plugin/StateIO.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
PluginState seedState()
{
    PluginState s;
    s.seed = 0x1234ABCDull;
    s.slotA = SourceType::LooseJack;
    s.slotB = SourceType::DirtyPot;
    s.params[P::jkWear] = 0.7f;
    s.params[P::ptDirt] = 0.8f;
    s.params[P::laneOn] = 1.0f;
    s.pattern.lanes[0].length = 16;
    s.pattern.lanes[0].steps[2].active = 1;
    s.pattern.lanes[1].length = 7;
    s.midiMap.add ({ 1, 74, P::ptTurn });
    s.presetName = "Fuzz";
    return s;
}

/** Everything a loaded state must satisfy, whatever the input was. */
bool isLegal (const PluginState& s)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& info = paramInfo (i);
        const float v = s.params[i];
        if (! std::isfinite (v) || v < info.min - 1.0e-4f || v > info.max + 1.0e-4f)
            return false;
    }
    if ((int) s.slotA < 0 || (int) s.slotA > kNumSourceTypes || (int) s.slotB < 0 || (int) s.slotB > kNumSourceTypes)
        return false;
    for (const auto& lane : s.pattern.lanes)
    {
        if (lane.length < 1 || lane.length > kMaxSteps)
            return false;
        for (int i = 0; i < kMaxSteps; ++i)
        {
            const auto& st = lane.steps[i];
            if (! std::isfinite (st.intensity) || ! std::isfinite (st.duration) || ! std::isfinite (st.send)
                || ! std::isfinite (st.probability) || ! std::isfinite (st.offset) || ! std::isfinite (st.target))
                return false;
            if (st.ratchet < 1 || st.ratchet > 4 || st.gesture > 3)
                return false;
        }
    }
    if (s.midiMap.size() > PluginState::kMaxMappings)
        return false;
    for (const auto& m : s.midiMap)
        if (m.param < 0 || m.param >= kNumParams || m.cc < 0 || m.cc > 127 || m.channel < 0 || m.channel > 16)
            return false;
    return true;
}
} // namespace

IFX_TEST (Fuzz, corruptedBinaryBlobsNeverCrashAndStayLegal)
{
    juce::MemoryBlock good;
    StateIO::toBinary (seedState(), good);
    IFX_CHECK (good.getSize() > 100);

    juce::Random rnd (0xF00D);
    int accepted = 0, rejected = 0;
    for (int iter = 0; iter < 6000; ++iter)
    {
        juce::MemoryBlock b (good);
        const int mode = iter % 4;
        if (mode == 0) // flip a few bytes
        {
            for (int k = 0; k < 1 + rnd.nextInt (8); ++k)
                static_cast<uint8_t*> (b.getData())[rnd.nextInt ((int) b.getSize())] ^= (uint8_t) (1 + rnd.nextInt (255));
        }
        else if (mode == 1) // truncate
            b.setSize ((size_t) rnd.nextInt ((int) b.getSize()));
        else if (mode == 2) // overwrite a run with random bytes
        {
            const int at = rnd.nextInt ((int) b.getSize());
            const int n = std::min ((int) b.getSize() - at, 1 + rnd.nextInt (40));
            for (int k = 0; k < n; ++k)
                static_cast<uint8_t*> (b.getData())[at + k] = (uint8_t) rnd.nextInt (256);
        }
        else // pure garbage
        {
            b.setSize ((size_t) (1 + rnd.nextInt (600)));
            for (size_t k = 0; k < b.getSize(); ++k)
                static_cast<uint8_t*> (b.getData())[k] = (uint8_t) rnd.nextInt (256);
        }

        PluginState out;
        const bool ok = b.getSize() > 0 && StateIO::fromBinary (b.getData(), (int) b.getSize(), out);
        if (ok)
        {
            ++accepted;
            IFX_CHECK_MSG (isLegal (out), "iteration " + juce::String (iter) + " produced an illegal state");
        }
        else
            ++rejected;
    }
    IFX_CHECK (accepted > 0 && rejected > 0);
}

IFX_TEST (Fuzz, hostileXmlValuesAreRepairedNotTrusted)
{
    const juce::StringArray bad { "nan", "-nan", "inf", "-inf", "1e999", "-1e999", "", "abc", "0x7fffffff", "99999999999999999999",
                                  "-1", "1e-999", "  ", "1,5", "\xff\xfe" };
    juce::Random rnd (42);
    for (const auto& value : bad)
    {
        for (int rep = 0; rep < 12; ++rep)
        {
            auto tree = StateIO::toValueTree (seedState());
            auto params = tree.getChildWithName ("Params");
            IFX_CHECK (params.isValid() || tree.getNumChildren() > 0);
            // poison a random handful of properties anywhere in the tree
            for (int k = 0; k < 6; ++k)
            {
                juce::ValueTree node = tree;
                for (int depth = 0; depth < 3 && node.getNumChildren() > 0; ++depth)
                    if (rnd.nextBool())
                        node = node.getChild (rnd.nextInt (node.getNumChildren()));
                if (node.getNumProperties() > 0)
                    node.setProperty (node.getPropertyName (rnd.nextInt (node.getNumProperties())), value, nullptr);
            }
            PluginState out;
            if (StateIO::fromValueTree (tree, out))
                IFX_CHECK_MSG (isLegal (out), "value '" + value + "' leaked into the state");
        }
    }
}

IFX_TEST (Fuzz, oversizedAndNestedInputIsBounded)
{
    // many mappings, many lanes, deep nesting: bounded work and bounded result
    juce::ValueTree t = StateIO::toValueTree (seedState());
    auto midi = t.getChildWithName ("MidiMap");
    for (int i = 0; i < 5000; ++i)
    {
        juce::ValueTree e ("Map");
        e.setProperty ("ch", i % 40, nullptr);
        e.setProperty ("cc", i % 300, nullptr);
        e.setProperty ("param", paramInfo (i % kNumParams).id, nullptr);
        if (midi.isValid())
            midi.appendChild (e, nullptr);
    }
    juce::ValueTree deep = t;
    for (int i = 0; i < 200; ++i)
    {
        juce::ValueTree child ("Junk");
        deep.appendChild (child, nullptr);
        deep = child;
    }
    PluginState out;
    IFX_CHECK (StateIO::fromValueTree (t, out));
    IFX_CHECK (isLegal (out));
}

IFX_TEST (Fuzz, unknownTreeTypesAreRejected)
{
    PluginState out;
    IFX_CHECK (! StateIO::fromValueTree (juce::ValueTree ("SomethingElse"), out));
    IFX_CHECK (! StateIO::fromValueTree (juce::ValueTree(), out));
    const char junk[] = "not a state at all";
    IFX_CHECK (! StateIO::fromBinary (junk, (int) sizeof (junk), out));
    IFX_CHECK (! StateIO::fromBinary (nullptr, 0, out));
}

IFX_TEST (Fuzz, hugeDeclaredCountsAreRejectedQuickly)
{
    // Regression: juce::ValueTree::readFromData looped ~2^31 times (tens of seconds) or allocated gigabytes on blobs like
    // these. Each is a few bytes; the defensive reader must reject them at once.
    auto compressedInt = [] (juce::MemoryOutputStream& o, int v) { o.writeCompressedInt (v); };
    auto timedReject = [] (const juce::MemoryBlock& b)
    {
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        PluginState out;
        const bool ok = StateIO::fromBinary (b.getData(), (int) b.getSize(), out);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        return ! ok && ms < 1000.0 && ! StateIO::readTree (b.getData(), (int) b.getSize()).isValid();
    };

    { // 2^31-1 properties
        juce::MemoryBlock b; juce::MemoryOutputStream o (b, false);
        o.writeString ("InterferenceState"); compressedInt (o, 0x7fffffff); o.flush();
        IFX_CHECK (timedReject (b));
    }
    { // 2^31-1 children
        juce::MemoryBlock b; juce::MemoryOutputStream o (b, false);
        o.writeString ("InterferenceState"); compressedInt (o, 0); compressedInt (o, 0x7fffffff); o.flush();
        IFX_CHECK (timedReject (b));
    }
    { // one property whose array value declares 2^31-1 elements
        juce::MemoryBlock b; juce::MemoryOutputStream o (b, false);
        o.writeString ("InterferenceState"); compressedInt (o, 1); o.writeString ("x");
        juce::MemoryOutputStream inner;
        inner.writeByte (7); compressedInt (inner, 0x7fffffff);
        compressedInt (o, (int) inner.getDataSize()); o.write (inner.getData(), inner.getDataSize()); // marker is in `inner`
        compressedInt (o, 0); o.flush();
        // skipping an unsupported value is acceptable; what matters is that it is bounded (no loop, no allocation)
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        PluginState out;
        StateIO::fromBinary (b.getData(), (int) b.getSize(), out);
        IFX_CHECK (juce::Time::getMillisecondCounterHiRes() - t0 < 1000.0);
    }
    { // a binary value declaring ~2 GB
        juce::MemoryBlock b; juce::MemoryOutputStream o (b, false);
        o.writeString ("InterferenceState"); compressedInt (o, 1); o.writeString ("x");
        compressedInt (o, 0x7ffffff0); o.writeByte (8); o.flush();
        IFX_CHECK (timedReject (b));
    }
    { // deep nesting
        juce::MemoryBlock b; juce::MemoryOutputStream o (b, false);
        for (int i = 0; i < 500; ++i) { o.writeString ("N"); compressedInt (o, 0); compressedInt (o, 1); }
        o.flush();
        IFX_CHECK (timedReject (b));
    }
}
