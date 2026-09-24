// Crash hook for the opt-in crash log (see CrashHook.h). Win32 / POSIX only, no JUCE.
// Everything reachable from a crash handler here avoids the heap, locks, symbol loading and
// the loader lock: frames are written as "module + offset" and symbolised offline.
#include "CrashHook.h"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstring>

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#else
 #include <dlfcn.h>
 #include <execinfo.h>
 #include <fcntl.h>
 #include <signal.h>
 #include <unistd.h>
#endif

namespace ifx
{

namespace
{
    /** Fixed-size text line, formatted without the heap or the C locale machinery. */
    struct LineBuffer
    {
        char data[480];
        std::size_t size = 0;

        void add (const char* s) noexcept
        {
            while (s != nullptr && *s != 0 && size < sizeof (data) - 1)
                data[size++] = *s++;
        }

        void addHex (std::uint64_t v, int minDigits = 1) noexcept
        {
            char tmp[16];
            int n = 0;
            do { tmp[n++] = "0123456789ABCDEF"[v & 0xFu]; v >>= 4; } while (v != 0 && n < 16);
            while (n < minDigits && n < 16) tmp[n++] = '0';
            add ("0x");
            while (n > 0 && size < sizeof (data) - 1)
                data[size++] = tmp[--n];
        }

        void addDec (int v) noexcept
        {
            char tmp[12];
            int n = 0;
            unsigned int u = v < 0 ? 0u - (unsigned int) v : (unsigned int) v;
            do { tmp[n++] = (char) ('0' + u % 10u); u /= 10u; } while (u != 0 && n < 11);
            if (v < 0) add ("-");
            while (n > 0 && size < sizeof (data) - 1)
                data[size++] = tmp[--n];
        }
    };

    std::atomic<bool> active { false };    // a log file is open and recording
    std::atomic<bool> reporting { false }; // one crash report per process: a second crashing thread must not interleave

#if defined (_WIN32)
    //==============================================================================
    HANDLE logFile = INVALID_HANDLE_VALUE;
    LPTOP_LEVEL_EXCEPTION_FILTER previousFilter = nullptr;
    bool filterInstalled = false;   // our filter is somewhere in the process chain
    bool filterStuck = false;       // someone installed after us: we stay as a pass-through for good
    using SignalHandler = void (__cdecl*) (int);
    SignalHandler previousAbort = nullptr;
    bool abortInstalled = false;
    CONTEXT unwindContext;          // static: a crashing thread may have almost no stack left

    void put (const char* s, std::size_t n) noexcept
    {
        DWORD written = 0;
        if (logFile != INVALID_HANDLE_VALUE && n > 0)
            WriteFile (logFile, s, (DWORD) n, &written, nullptr);
    }

    void putText (const char* s) noexcept { put (s, std::strlen (s)); }

    /** The module's own name from its export directory: read from the mapped image, so it
        needs neither the loader lock nor the heap. */
    const char* imageName (const void* base) noexcept
    {
        const auto* b = static_cast<const unsigned char*> (base);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*> (b);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            return "?";
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*> (b + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return "?";
        const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (dir.VirtualAddress == 0 || dir.Size < sizeof (IMAGE_EXPORT_DIRECTORY))
            return "(host program or unnamed module)";
        const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*> (b + dir.VirtualAddress);
        return exports->Name != 0 ? reinterpret_cast<const char*> (b + exports->Name) : "?";
    }

    void addLocation (LineBuffer& line, const void* address) noexcept
    {
        line.addHex ((std::uint64_t) (std::uintptr_t) address, 8);
        void* base = nullptr;
        if (address == nullptr || RtlPcToFileHeader (const_cast<void*> (address), &base) == nullptr || base == nullptr)
            return;
        line.add ("  ");
        line.add (imageName (base));
        line.add (" + ");
        line.addHex ((std::uint64_t) ((std::uintptr_t) address - (std::uintptr_t) base));
    }

    /** Frames from the faulting context where possible (x64 unwind data, bounds-checked
        against the thread's stack), otherwise from the current call chain. */
    int captureFrames (const CONTEXT* context, void** frames, int maxFrames) noexcept
    {
       #if defined (_M_X64)
        if (context != nullptr)
        {
            const auto* tib = reinterpret_cast<const NT_TIB*> (NtCurrentTeb());
            const auto low = (DWORD64) (std::uintptr_t) tib->StackLimit;
            const auto high = (DWORD64) (std::uintptr_t) tib->StackBase;
            unwindContext = *context;
            int n = 0;
            while (n < maxFrames && unwindContext.Rip != 0)
            {
                if (unwindContext.Rsp < low || unwindContext.Rsp + 8 > high)
                    break;
                frames[n++] = reinterpret_cast<void*> (unwindContext.Rip);
                DWORD64 imageBase = 0;
                if (auto* function = RtlLookupFunctionEntry (unwindContext.Rip, &imageBase, nullptr))
                {
                    void* handlerData = nullptr;
                    DWORD64 establisherFrame = 0;
                    RtlVirtualUnwind (UNW_FLAG_NHANDLER, imageBase, unwindContext.Rip, function, &unwindContext,
                                      &handlerData, &establisherFrame, nullptr);
                }
                else
                {
                    // leaf function: the return address is on top of the stack
                    unwindContext.Rip = *reinterpret_cast<const DWORD64*> (unwindContext.Rsp);
                    unwindContext.Rsp += 8;
                }
            }
            if (n > 0)
                return n;
        }
       #else
        (void) context;
       #endif
        return (int) RtlCaptureStackBackTrace (0, (DWORD) maxFrames, frames, nullptr);
    }

    void writeCrash (const char* what, std::uint64_t code, const void* address, const CONTEXT* context) noexcept
    {
        if (! active.load() || reporting.exchange (true))
            return;

        LineBuffer head;
        head.add ("\r\n==== CRASH ====\r\n");
        head.add (what);
        head.add (", code ");
        head.addHex (code, 8);
        head.add ("\r\nat ");
        addLocation (head, address);
        head.add ("\r\nStack (module + offset; symbolise with the matching .pdb):\r\n");
        put (head.data, head.size);

        void* frames[64];
        const int n = captureFrames (context, frames, 64);
        for (int i = 0; i < n; ++i)
        {
            LineBuffer line;
            line.add ("  ");
            addLocation (line, frames[i]);
            line.add ("\r\n");
            put (line.data, line.size);
        }
        putText ("==== END ====\r\n");
    }

    LONG WINAPI onUnhandledException (EXCEPTION_POINTERS* info)
    {
        const auto* record = info != nullptr ? info->ExceptionRecord : nullptr;
        writeCrash ("Unhandled exception",
                    record != nullptr ? (std::uint64_t) record->ExceptionCode : 0,
                    record != nullptr ? record->ExceptionAddress : nullptr,
                    info != nullptr ? info->ContextRecord : nullptr);
        return previousFilter != nullptr ? previousFilter (info) : EXCEPTION_CONTINUE_SEARCH;
    }

    // abort() and std::terminate in this module (static CRT: the handler table is ours alone).
    // The CRT resets the handler before calling it; returning lets abort() end the process.
    void __cdecl onAbort (int)
    {
        writeCrash ("abort() or std::terminate", 3, nullptr, nullptr);
    }

    void pinModule() noexcept
    {
        HMODULE self = nullptr;
        GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR> (&onUnhandledException), &self);
    }

#else
    //==============================================================================
    int logFd = -1;
    const int kSignals[] = { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL };
    constexpr int kNumSignals = (int) (sizeof (kSignals) / sizeof (kSignals[0]));
    struct sigaction previousActions[kNumSignals];
    bool signalInstalled[kNumSignals] {};
    bool signalStuck[kNumSignals] {};

    void putText (const char* s) noexcept
    {
        if (logFd >= 0)
        {
            const auto r = ::write (logFd, s, std::strlen (s));
            (void) r;
        }
    }

    void onSignal (int sig, siginfo_t* info, void* context)
    {
        if (active.load() && ! reporting.exchange (true))
        {
            LineBuffer head;
            head.add ("\n==== CRASH ====\nFatal signal ");
            head.addDec (sig);
            head.add (", address ");
            head.addHex ((std::uint64_t) (std::uintptr_t) (info != nullptr ? info->si_addr : nullptr), 8);
            head.add ("\nStack:\n");
            head.data[head.size] = 0;
            putText (head.data);
            void* frames[64];
            const int n = backtrace (frames, 64);
            backtrace_symbols_fd (frames, n, logFd);
            putText ("==== END ====\n");
        }

        // hand the signal to whatever was installed before us, exactly as it was installed
        for (int i = 0; i < kNumSignals; ++i)
        {
            if (kSignals[i] != sig)
                continue;
            const auto& prev = previousActions[i];
            if ((prev.sa_flags & SA_SIGINFO) != 0 && prev.sa_sigaction != nullptr)
            {
                prev.sa_sigaction (sig, info, context);
                return;
            }
            if (prev.sa_handler == SIG_IGN)
                return;
            if (prev.sa_handler != SIG_DFL && prev.sa_handler != nullptr)
            {
                prev.sa_handler (sig);
                return;
            }
            sigaction (sig, &prev, nullptr); // default action: terminate (with a core dump)
            std::raise (sig);
            return;
        }
    }

    void pinModule() noexcept
    {
        Dl_info info {};
        if (dladdr (reinterpret_cast<void*> (&onSignal), &info) != 0 && info.dli_fname != nullptr)
            dlopen (info.dli_fname, RTLD_NOW | RTLD_NODELETE); // never closed: keeps this module mapped
    }
#endif
}

//==============================================================================
bool CrashHook::install (const char* utf8Path) noexcept
{
    uninstall();
    if (utf8Path == nullptr || *utf8Path == 0)
        return false;

   #if defined (_WIN32)
    wchar_t widePath[MAX_PATH * 4];
    if (MultiByteToWideChar (CP_UTF8, 0, utf8Path, -1, widePath, (int) (sizeof (widePath) / sizeof (widePath[0]))) == 0)
        return false;
    // FILE_SHARE_WRITE: other readers / writers (the troubleshooting export, an editor) may open it too
    logFile = CreateFileW (widePath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logFile == INVALID_HANDLE_VALUE)
        return false;

    if (! filterInstalled)
    {
        previousFilter = SetUnhandledExceptionFilter (onUnhandledException);
        filterInstalled = true;
    }
    if (! abortInstalled)
    {
        const auto prev = std::signal (SIGABRT, onAbort);
        if (prev != SIG_ERR)
        {
            previousAbort = prev;
            abortInstalled = true;
        }
    }
   #else
    logFd = ::open (utf8Path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if (logFd < 0)
        return false;
    {
        void* warmUp[1];
        backtrace (warmUp, 1); // loads the unwinder now rather than inside a signal handler
    }
    struct sigaction action {};
    action.sa_sigaction = onSignal;
    sigemptyset (&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    for (int i = 0; i < kNumSignals; ++i)
        if (! signalInstalled[i] && sigaction (kSignals[i], &action, &previousActions[i]) == 0)
            signalInstalled[i] = true;
   #endif

    reporting.store (false);
    active.store (true);
    return true;
}

bool CrashHook::write (const char* data, std::size_t numBytes) noexcept
{
    if (! active.load())
        return false;
    if (numBytes == 0)
        return true;

   #if defined (_WIN32)
    DWORD written = 0;
    return logFile != INVALID_HANDLE_VALUE
        && WriteFile (logFile, data, (DWORD) numBytes, &written, nullptr) != FALSE
        && written == (DWORD) numBytes;
   #else
    return logFd >= 0 && ::write (logFd, data, numBytes) == (ssize_t) numBytes;
   #endif
}

void CrashHook::uninstall() noexcept
{
    active.store (false);

   #if defined (_WIN32)
    if (logFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle (logFile);
        logFile = INVALID_HANDLE_VALUE;
    }

    if (filterInstalled && ! filterStuck)
    {
        const auto current = SetUnhandledExceptionFilter (previousFilter);
        if (current == onUnhandledException)
        {
            filterInstalled = false;
            previousFilter = nullptr;
        }
        else
        {
            // Another filter was installed after ours and may forward to it: put it back, keep
            // ours in its chain as a pass-through to our predecessor (previousFilter is kept),
            // and pin this module so that chain never calls unloaded code.
            SetUnhandledExceptionFilter (current);
            filterStuck = true;
            pinModule();
        }
    }

    if (abortInstalled)
    {
        const auto current = std::signal (SIGABRT, previousAbort);
        if (current != onAbort && current != SIG_ERR)
            std::signal (SIGABRT, current); // replaced after us: leave that handler in place
        abortInstalled = false;
        previousAbort = nullptr;
    }
   #else
    if (logFd >= 0)
    {
        ::close (logFd);
        logFd = -1;
    }

    bool stuck = false;
    for (int i = 0; i < kNumSignals; ++i)
    {
        if (! signalInstalled[i] || signalStuck[i])
            continue;
        struct sigaction current {};
        sigaction (kSignals[i], nullptr, &current);
        if ((current.sa_flags & SA_SIGINFO) != 0 && current.sa_sigaction == onSignal)
        {
            sigaction (kSignals[i], &previousActions[i], nullptr);
            signalInstalled[i] = false;
        }
        else
        {
            signalStuck[i] = true; // someone replaced ours and may forward to it
            stuck = true;
        }
    }
    if (stuck)
        pinModule();
   #endif
}

bool CrashHook::isActive() noexcept
{
    return active.load();
}

} // namespace ifx
