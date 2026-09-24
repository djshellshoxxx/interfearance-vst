#pragma once

#include <JuceHeader.h>

namespace ifx
{

class InterferenceProcessor;

/** Help > Debug support tools (include.md "extra features"). Everything here runs on the
    message thread; nothing is ever called from the audio callback. */
namespace Diagnostics
{
    /** Folder for crash logs and troubleshooting files: Documents/Interference/Support. */
    juce::File supportFolder();

    /** Obvious signals of misconfiguration in plain language (empty = nothing found). */
    juce::StringArray findMisconfigurations (InterferenceProcessor& p);

    /** Raw engine / host / parameter data for the live debug window. */
    juce::String liveData (InterferenceProcessor& p);

    /** Light self-diagnosis plus every setting, audio / MIDI configuration, licence, version and
        host information. Contains no audio and no personal data beyond the host's name. */
    juce::String troubleshootingReport (InterferenceProcessor& p);

    /** Writes troubleshootingReport() to a new time-stamped file in supportFolder(). */
    bool writeTroubleshootingFile (InterferenceProcessor& p, juce::File& written, juce::String& error);
}

/** Opt-in crash log. While active, recent activity is appended to a time-stamped file and an
    unhandled-exception hook (chained to, and restored for, the host's own handler) writes the
    exception and a stack trace before the process terminates. One log per process; the
    owning plug-in instance stops it when it is destroyed. */
namespace CrashLog
{
    bool start (const void* owner, const juce::String& header, juce::File& written, juce::String& error);
    void append (const juce::StringArray& lines);
    void stop (const void* owner);
    bool isOwnedBy (const void* owner);
    juce::File currentFile();
}

} // namespace ifx
