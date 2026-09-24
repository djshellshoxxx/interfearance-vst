#pragma once

#include <cstddef>

namespace ifx
{

/** Process-wide crash hook behind the opt-in crash log (Diagnostics.h, CrashLog).

    Deliberately free of JUCE. The code that runs while the process is crashing uses only a
    handle opened in advance, static buffers and calls that need no heap, no locks and no
    symbol loading, so a crash can never turn into a hang of the host.

    install(), write() and uninstall() are called on the message thread only. */
namespace CrashHook
{
    /** Opens `utf8Path` for appending and installs the crash handlers: an unhandled-exception
        filter plus a SIGABRT handler (Windows), or fatal-signal handlers (other platforms). Every
        handler forwards to whatever was installed before it. Returns false if the file cannot
        be opened. */
    bool install (const char* utf8Path) noexcept;

    /** Appends bytes to the open log during normal operation. Returns false if no log is open. */
    bool write (const char* data, std::size_t numBytes) noexcept;

    /** Closes the log and removes the handlers. If another component installed its own handler
        after ours (so it may still forward to ours), ours stays in that chain as a pure
        pass-through and this module is pinned in memory, so the chain never calls unloaded code. */
    void uninstall() noexcept;

    /** True while a log file is open. */
    bool isActive() noexcept;
}

} // namespace ifx
