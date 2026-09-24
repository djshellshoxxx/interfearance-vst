#pragma once

#include <JuceHeader.h>

namespace ifx
{

class InterferenceProcessor;

/** Help > Debug support tools (include.md "extra features"). Everything here runs on the
    message thread; nothing is ever called from the audio callback. */
namespace Diagnostics
{
    /** Folder for crash logs and troubleshooting files: Documents/Interference/Support
        (under StateIO::userDataFolder(), so IFX_DATA_ROOT / test overrides apply). */
    juce::File supportFolder();

    /** Obvious signals of misconfiguration in plain language (empty = nothing found). */
    juce::StringArray findMisconfigurations (InterferenceProcessor& p);

    /** Raw engine / host / parameter data for the live debug window. */
    juce::String liveData (InterferenceProcessor& p);

    /** Light self-diagnosis plus every setting, audio / MIDI configuration, licence, version and
        host information. Contains no audio. It does contain folder paths, which include the
        user name (disclosed in Docs/Privacy.md). The self-test runs a 1 s offline render and a
        write test in the preset folder (which it creates if missing). */
    juce::String troubleshootingReport (InterferenceProcessor& p);

    /** Writes troubleshootingReport() to a new time-stamped file in supportFolder(). */
    bool writeTroubleshootingFile (InterferenceProcessor& p, juce::File& written, juce::String& error);
}

/** Opt-in crash log. While active, recent activity is appended to a time-stamped file and a
    crash hook (CrashHook.h: unhandled-exception filter + SIGABRT on Windows, fatal signals
    elsewhere, each chained to the handler installed before it) writes the exception and a raw
    stack trace before the process terminates. Some crashes cannot be logged: fast-fail
    (__fastfail, /GS, heap corruption detected by Windows) and hosts that catch plug-in crashes
    themselves. One log per process; the owning plug-in instance stops it when it is destroyed. */
namespace CrashLog
{
    bool start (const void* owner, const juce::String& header, juce::File& written, juce::String& error);
    void append (const juce::StringArray& lines);
    void stop (const void* owner);
    bool isOwnedBy (const void* owner);
    juce::File currentFile();
}

} // namespace ifx
