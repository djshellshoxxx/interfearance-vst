#include "Diagnostics.h"
#include "CrashHook.h"
#include "PluginProcessor.h"
#include "UI/HelpContent.h"

namespace ifx
{

namespace
{
    const char* contactStateName (int s)
    {
        switch (s)
        {
            case 0: return "good";
            case 1: return "resistive";
            case 2: return "open";
            case 3: return "recovering";
            default: return "?";
        }
    }

    juce::String db (float linear)
    {
        return linear > 1.0e-6f ? juce::String (juce::Decibels::gainToDecibels (linear), 1) + " dB" : juce::String ("-inf");
    }

    juce::String stamp()
    {
        return juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    }

    float value (InterferenceProcessor& p, int param)
    {
        return p.apvts.getRawParameterValue (paramInfo (param).id)->load();
    }

    juce::String valueText (InterferenceProcessor& p, int param)
    {
        return p.param (param)->getCurrentValueAsText();
    }

    /** The host never changes while the process runs: look it up once (it resolves the host path). */
    const juce::String& hostDescription()
    {
        static const juce::String host (juce::PluginHostType().getHostDescription());
        return host;
    }
}

//==============================================================================
juce::File Diagnostics::supportFolder()
{
    return StateIO::userDataFolder().getChildFile ("Support");
}

juce::StringArray Diagnostics::findMisconfigurations (InterferenceProcessor& p)
{
    juce::StringArray w;
    const auto& t = p.telemetry();
    auto on = [&p] (int param) { return value (p, param) >= 0.5f; };

    if (on (P::bypass))
        w.add ("Plug-in Bypass is on: the output is the dry input.");
    const bool aLive = p.slotType (0) != SourceType::None && on (P::slotAOn);
    const bool bLive = p.slotType (1) != SourceType::None && on (P::slotBOn);
    if (! aLive && ! bLive)
        w.add ("No active source: both slots are empty or disabled, so no fault can occur.");
    if (value (p, P::mix) <= 0.0f)
        w.add ("Dry/Wet is 0 %: only the dry input is heard.");
    if ((int) value (p, P::monitor) != 0)
        w.add ("Monitor is set to '" + valueText (p, P::monitor) + "', not Full Output.");
    if (value (p, P::outTrim) <= -18.0f)
        w.add ("Output trim is very low (" + valueText (p, P::outTrim) + ").");
    if (on (P::ceilOn) && value (p, P::ceiling) <= -12.0f)
        w.add ("Output Ceiling is very low (" + valueText (p, P::ceiling) + "): loud material is limited hard.");
    if (on (P::laneOn) && ! t.clockRunning.load())
        w.add ("The Gesture Lane is on but no clock runs: start the host transport or press Run.");
    if (value (p, P::react) > 0.0f && ! t.sidechainPresent.load())
        w.add ("Sidechain React is up but no sidechain signal is connected.");
    if ((int) value (p, P::mode) == 2 && (int) value (p, P::excType) == 0)
        w.add ("Create mode without Excitation: a silent track stays silent unless a source is biased or touched.");
    if (on (P::rawHw) && ((int) value (p, P::excType) != 0) && ! on (P::keepExc))
        w.add ("Raw Hardware is on: Excitation is muted (Keep Excitation is off).");
    if (on (P::studioOn) && on (P::rawHw))
        w.add ("Studio is on but Raw Hardware bypasses it.");
    if (on (P::ghostOn))
        w.add ("The hidden Ghost Carrier effect is active.");
    const double sr = p.currentSampleRate();
    if (sr < 22050.0 || sr > 192000.0)
        w.add ("Unusual sample rate: " + juce::String (sr, 0) + " Hz (supported: 44.1 - 192 kHz).");
    if (p.getBlockSize() <= 0)
        w.add ("The host has not prepared the plug-in yet (block size 0).");
    if (p.getTotalNumInputChannels() == 0)
        w.add ("No input channels: the host feeds this instance no audio (generator use only).");
    return w;
}

juce::String Diagnostics::liveData (InterferenceProcessor& p)
{
    const auto& t = p.telemetry();
    juce::String s;
    const juce::String nl ("\n");
    s << "INTERFERENCE " << IFX_VERSION_STRING << "  |  " << juce::AudioProcessor::getWrapperTypeDescription (p.wrapperType)
      << "  |  " << hostDescription() << nl;
    s << "sample rate " << juce::String (t.sampleRate.load(), 0) << " Hz, block " << p.getBlockSize()
      << ", channels in/out " << p.getTotalNumInputChannels() << "/" << p.getTotalNumOutputChannels()
      << ", latency 0, tail " << juce::String (t.tailSeconds.load(), 2) << " s" << nl;
    s << "clock: " << (t.hostTransport.load() ? "host" : (t.internalClock.load() ? "internal" : "none"))
      << (t.clockRunning.load() ? " running" : " stopped") << ", " << juce::String (t.bpm.load(), 1) << " BPM, ppq "
      << juce::String (t.ppq.load(), 3) << ", lane steps A/B " << t.laneStep[0].load() + 1 << "/" << t.laneStep[1].load() + 1 << nl;
    s << "seed " << juce::String::toHexString ((juce::int64) p.getSeed()).toUpperCase() << (p.getSeedLock() ? " (locked)" : " (unlocked)")
      << ", take " << (int) p.getTake() << ", effective " << juce::String::toHexString ((juce::int64) t.effectiveSeed.load()).toUpperCase()
      << ", resets " << (int) t.resets.load() << nl;
    s << "sidechain " << (t.sidechainPresent.load() ? "present" : "absent") << ", excitation " << (t.excitationActive.load() ? "active" : "off")
      << ", feedback " << juce::String (t.fbEffective.load(), 3) << (t.fbLimited.load() ? " (limited)" : "") << nl;
    s << "peaks: in " << db (t.inPeak.load()) << ", R " << db (t.rPeak.load()) << ", D " << db (t.dPeak.load()) << ", A " << db (t.aPeak.load())
      << ", C " << db (t.cPeak.load()) << ", P " << db (t.pPeak.load()) << ", W " << db (t.wPeak.load()) << ", out " << db (t.outPeak.load()) << nl << nl;

    for (int sl = 0; sl < 2; ++sl)
    {
        const auto& v = t.slot[sl];
        s << "SLOT " << (sl == 0 ? "A" : "B") << ": " << sourceName (p.slotType (sl)) << (v.enabled.load() ? "" : " (bypassed)")
          << ", contact " << contactStateName (v.state.load()) << (v.shorted.load() ? " (shorted)" : "")
          << ", position " << juce::String (v.position.load(), 3) << ", velocity " << juce::String (v.velocity.load(), 3)
          << ", openness " << juce::String (v.openness.load(), 3) << ", activity " << juce::String (v.activity.load(), 3) << nl
          << "        events: open " << (int) v.openEvents.load() << ", resistive " << (int) v.resistiveEvents.load()
          << ", mechanical " << (int) v.mechEvents.load() << ", gestures " << (int) v.gestures.load()
          << (v.motionLimited.load() ? ", motion limited" : "") << nl;
    }

    const auto warnings = findMisconfigurations (p);
    s << nl << "CHECKS: " << (warnings.isEmpty() ? juce::String ("no misconfiguration found") : juce::String (warnings.size()) + " warning(s)") << nl;
    for (const auto& w : warnings)
        s << "  ! " << w << nl;

    s << nl << "PARAMETERS" << nl;
    for (int i = 0; i < kNumParams; ++i)
        s << "  " << juce::String (paramInfo (i).id).paddedRight (' ', 14) << juce::String (paramInfo (i).name).paddedRight (' ', 22)
          << valueText (p, i) << nl;
    return s;
}

juce::String Diagnostics::troubleshootingReport (InterferenceProcessor& p)
{
    const juce::String nl ("\n");
    juce::String s;
    s << "Interference troubleshooting file" << nl
      << "Created: " << juce::Time::getCurrentTime().toString (true, true, true, true) << nl
      << "Version: " << IFX_VERSION_STRING << " (built " << __DATE__ << ")" << nl
      << "Licence: " << Branding::licenseSummary << nl
      << "Licence activation: none (offline product, no copy protection)" << nl
      << "Support: " << Branding::supportEmail << nl << nl;

    s << "== System ==" << nl
      << "OS: " << juce::SystemStats::getOperatingSystemName() << (juce::SystemStats::isOperatingSystem64Bit() ? " (64-bit)" : "") << nl
      << "CPU: " << juce::SystemStats::getCpuModel() << ", " << juce::SystemStats::getNumPhysicalCpus() << " cores / "
      << juce::SystemStats::getNumCpus() << " threads, SSE2 " << (juce::SystemStats::hasSSE2() ? "yes" : "no")
      << ", AVX " << (juce::SystemStats::hasAVX() ? "yes" : "no") << nl
      << "Memory: " << juce::SystemStats::getMemorySizeInMegabytes() << " MB" << nl << nl;

    s << "== Host (DAW) ==" << nl
      << "Host: " << hostDescription() << nl
      << "Host program: " << juce::File::getSpecialLocation (juce::File::hostApplicationPath).getFileName() << nl
      << "Format: " << juce::AudioProcessor::getWrapperTypeDescription (p.wrapperType) << nl << nl;

    s << "== Audio settings ==" << nl
      << "Sample rate: " << juce::String (p.getSampleRate(), 0) << " Hz" << nl
      << "Block size: " << p.getBlockSize() << nl
      << "Input channels: " << p.getTotalNumInputChannels() << ", output channels: " << p.getTotalNumOutputChannels() << nl
      << "Sidechain bus: " << (p.getBusCount (true) > 1 && p.getBus (true, 1)->isEnabled() ? "enabled" : "disabled") << nl
      << "Offline (non-realtime) render: " << (p.isNonRealtime() ? "yes" : "no") << nl
      << "Latency: " << p.getLatencySamples() << " samples, tail: " << juce::String (p.getTailLengthSeconds(), 2) << " s" << nl << nl;

    s << "== MIDI settings ==" << nl
      << "Accepts MIDI: yes. Notes 36-39 slot A, 48-51 slot B, 62 one-shot, 63 fill, other notes both slots; "
         "CC1 motion, aftertouch pressure, CC120/123 release (see MidiMap.md)" << nl
      << "MIDI learn pending: " << (p.midiLearnTarget() >= 0 ? juce::String (paramInfo (p.midiLearnTarget()).name) : juce::String ("no")) << nl
      << "Mappings (" << p.getMidiMap().size() << "):" << nl;
    for (const auto& m : p.getMidiMap())
        s << "  CC " << m.cc << (m.channel > 0 ? " ch " + juce::String (m.channel) : juce::String (" any channel")) << " -> "
          << paramInfo (m.param).name << nl;
    s << nl;

    // ---- light self-diagnosis ---------------------------------------------------------
    s << "== Self-test ==" << nl;
    int failures = 0;
    auto result = [&s, &failures] (bool ok, const juce::String& what)
    {
        if (! ok) ++failures;
        s << (ok ? "[PASS] " : "[FAIL] ") << what << "\n";
    };

    const auto state = p.getCurrentState();
    bool paramsOk = true;
    for (int i = 0; i < kNumParams; ++i)
    {
        const float v = state.params[i];
        paramsOk = paramsOk && std::isfinite (v) && v >= paramInfo (i).min - 1.0e-4f && v <= paramInfo (i).max + 1.0e-4f;
    }
    result (paramsOk, juce::String (kNumParams) + " parameters finite and in range");

    {
        juce::MemoryBlock blob;
        StateIO::toBinary (state, blob);
        PluginState back;
        const bool ok = StateIO::fromBinary (blob.getData(), (int) blob.getSize(), back) && back.soundEquals (state);
        result (ok, "state save / reload round trip (" + juce::String ((int) blob.getSize()) + " bytes)");
    }
    {
        RenderSettings rs;
        rs.useBars = false;
        rs.seconds = 1.0;
        rs.sampleRate = 48000.0;
        rs.channels = 2;
        const auto r = OfflineRenderer::render (state, rs);
        bool finite = r.ok;
        for (int ch = 0; finite && ch < r.audio.getNumChannels(); ++ch)
            for (int i = 0; i < r.audio.getNumSamples(); ++i)
                if (! std::isfinite (r.audio.getSample (ch, i))) { finite = false; break; }
        result (finite && r.peak <= kSafetyClamp, "offline engine render of the current settings: finite, bounded (peak "
                                                     + db (r.peak) + (r.silent ? ", silent: " + r.silentReason : juce::String()) + ")");
    }
    {
        auto folder = StateIO::userPresetFolder();
        const bool made = folder.createDirectory().wasOk();
        auto probe = folder.getChildFile (".write_test_" + stamp());
        const bool ok = made && probe.replaceWithText ("ok") && probe.deleteFile();
        result (ok, "preset folder writable: " + folder.getFullPathName());
        int n = 0;
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + StateIO::kPresetExtension))
        {
            ++n;
            PluginState ps;
            juce::String err;
            if (! StateIO::loadPresetFile (f, ps, err))
                result (false, "user preset unreadable: " + f.getFileName() + " (" + err + ")");
        }
        s << "       user presets found: " << n << nl;
    }
    {
        const auto f = p.settings().getFile();
        result (! f.exists() || f.hasReadAccess(), "options file readable: " + f.getFullPathName());
    }
    const auto& t = p.telemetry();
    const float outDb = t.outPeakDb.load();
    result (std::isfinite (outDb) && outDb < 24.0f, "live output peak finite (" + juce::String (outDb, 1) + " dBFS)");
    s << "Result: " << (failures == 0 ? juce::String ("all checks passed") : juce::String (failures) + " check(s) failed") << nl << nl;

    const auto warnings = findMisconfigurations (p);
    s << "== Possible misconfiguration ==" << nl;
    if (warnings.isEmpty())
        s << "none found" << nl;
    for (const auto& w : warnings)
        s << "- " << w << nl;
    s << nl;

    s << "== Current settings ==" << nl
      << "Preset: " << p.getPresetName() << (p.isModified() ? " (modified)" : "") << nl
      << "Slot A: " << sourceName (state.slotA) << ", slot B: " << sourceName (state.slotB) << nl
      << "Seed: " << juce::String::toHexString ((juce::int64) state.seed).toUpperCase() << (state.seedLock ? " locked" : " unlocked")
      << ", take " << (int) state.take << nl
      << "Tooltips: " << (p.tooltipsEnabled() ? "on" : "off") << nl;
    for (int i = 0; i < kNumParams; ++i)
        s << "  " << juce::String (paramInfo (i).id).paddedRight (' ', 14) << valueText (p, i) << nl;
    s << nl << "== Full state (XML) ==" << nl << StateIO::toValueTree (state).toXmlString() << nl;
    return s;
}

bool Diagnostics::writeTroubleshootingFile (InterferenceProcessor& p, juce::File& written, juce::String& error)
{
    const auto folder = supportFolder();
    if (! folder.createDirectory().wasOk())
    {
        error = "Cannot create " + folder.getFullPathName();
        return false;
    }
    written = folder.getChildFile ("Interference_Troubleshooting_" + stamp() + ".txt").getNonexistentSibling();
    if (! written.replaceWithText (troubleshootingReport (p)))
    {
        error = "Cannot write " + written.getFullPathName() + " (disk full or no permission?)";
        return false;
    }
    return true;
}

//==============================================================================
// The crash-time work lives in CrashHook (no JUCE, no heap). While a log is open every write
// goes through CrashHook::write, because the hook holds the file open for appending.
namespace
{
    const void* crashOwner = nullptr; // message thread only
    juce::File crashFile;

    bool writeLog (const juce::String& text)
    {
        return CrashHook::write (text.toRawUTF8(), text.getNumBytesAsUTF8());
    }
}

bool CrashLog::start (const void* owner, const juce::String& header, juce::File& written, juce::String& error)
{
    if (crashOwner != nullptr)
    {
        juce::String note;
        note << "== Log handed over to another plug-in instance at " << juce::Time::getCurrentTime().toString (true, true, true, true)
             << " (its newest events may be missing) ==" << juce::newLine;
        writeLog (note);
        stop (crashOwner);
    }
    const auto folder = Diagnostics::supportFolder();
    if (! folder.createDirectory().wasOk())
    {
        error = "Cannot create " + folder.getFullPathName();
        return false;
    }
    crashFile = folder.getChildFile ("Interference_CrashLog_" + stamp() + ".log").getNonexistentSibling();
    if (! crashFile.replaceWithText (header + "\n== Activity log (newest last) ==\n"))
    {
        error = "Cannot write " + crashFile.getFullPathName() + " (disk full or no permission?)";
        crashFile = juce::File();
        return false;
    }
    if (! CrashHook::install (crashFile.getFullPathName().toRawUTF8()))
    {
        error = "Cannot open " + crashFile.getFullPathName() + " for crash logging";
        crashFile = juce::File();
        return false;
    }
    crashOwner = owner;
    written = crashFile;
    return true;
}

void CrashLog::append (const juce::StringArray& lines)
{
    if (crashOwner == nullptr || lines.isEmpty())
        return;
    const auto time = juce::Time::getCurrentTime().formatted ("%H:%M:%S ");
    juce::String text;
    for (const auto& l : lines)
        text << time << l << juce::newLine;
    writeLog (text);
}

void CrashLog::stop (const void* owner)
{
    if (crashOwner == nullptr || owner != crashOwner)
        return;
    juce::String closing;
    closing << "== Log closed normally at " << juce::Time::getCurrentTime().toString (true, true, true, true) << " ==" << juce::newLine;
    writeLog (closing);
    CrashHook::uninstall();
    crashOwner = nullptr;
}

bool CrashLog::isOwnedBy (const void* owner) { return crashOwner != nullptr && owner == crashOwner; }
juce::File CrashLog::currentFile() { return crashOwner != nullptr ? crashFile : juce::File(); }

} // namespace ifx
