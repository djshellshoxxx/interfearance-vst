# Code review: WP13 diagnostics + Ghost Carrier, and audio-path real-time / DSP audit

Reviewer: read-only review agent, 2026-09-23. Scope: `git diff a2d674f..HEAD -- Source Tests CMakeLists.txt`
(the whitespace-insensitive diff is ~1 000 lines; most of the raw 5 000-line diff is CRLF churn) plus the whole
`processBlock` call tree (`Source/Plugin/PluginProcessor.cpp`, `Source/DSP/*.h`, `Source/Core/*.h`).
JUCE APIs checked against `C:\Users\new\Desktop\JUCE_8_0_8\modules`. Checklists used:
`skills/audio-dsp-review`, `skills/audio-numerics-review`, `skills/audio-artifacts-debug`.

Severity: **Blocker** = won't compile / will crash / fails a test. **Major** = audible defect, dropout, hang or
lifetime bug a user can reach. **Minor** = everything else worth fixing.

Line numbers are for HEAD `59ffcd0`. Uncommitted edits to `Environment.h` and `TestPerf.cpp` were in the working
tree during the review; they were checked and are fine.

---

## 0. Blockers

**None found.** Every JUCE signature used by the new code matches JUCE 8.0.8, and the test expectations hold
when traced through the code. This agrees with commit `59ffcd0` ("121 DSP tests + 13 host/GUI tests pass").

API verification (all OK):

| Call site | JUCE 8.0.8 declaration | Verdict |
|---|---|---|
| `Tests/TestGui.cpp:381` `juce::MouseEvent me (src, p, {}, 1.0f, 0,0,0,0, &v, &v, now, p, now, 1, false)` | `MouseEvent (MouseInputSource, Point<float>, ModifierKeys, float pressure, float orientation, float rotation, float tiltX, float tiltY, Component* eventComponent, Component* originator, Time eventTime, Point<float> mouseDownPos, Time mouseDownTime, int numberOfClicks, bool mouseWasDragged) noexcept` (juce_MouseEvent.h:77) | 15 args, types match. `Desktop::getMainMouseSource()` returns source 0, which `MouseInputSourceList()` always creates, so it works headlessly |
| `Overlays.cpp:253` `AlertWindow::showAsync (MessageBoxOptions, [safe] (int result) {...})` | `static void showAsync (const MessageBoxOptions&, std::function<void (int)>)` (juce_AlertWindow.h:309) | OK. With two buttons `LookAndFeel_V2::createAlertWindow` gives button 1 ("Hard reset") the code **1** plus Return, and button 2 ("Cancel") the code **0** plus Escape. `result == 1` is correct |
| `showAsync (..., nullptr)` | overloads `(options, ModalComponentManager::Callback*)` and `(options, std::function<void(int)>)` | Not ambiguous: `nullptr` to a pointer is a standard conversion, which wins over the user-defined conversion to `std::function` |
| `Diagnostics.cpp:351` `path.copyToUTF16 (reinterpret_cast<CharPointer_UTF16::CharType*> (crashPath), sizeof (crashPath))` | `size_t copyToUTF16 (CharPointer_UTF16::CharType*, size_t maxBufferSizeBytes) const noexcept`; `CharType = wchar_t` on Windows | OK: the size is in bytes and the function null-terminates |
| `SystemStats::getCpuModel / hasSSE2 / hasAVX / getNumPhysicalCpus / getMemorySizeInMegabytes / isOperatingSystem64Bit / getStackBacktrace` | all static, juce_SystemStats.h:105-271 | OK |
| `File::getNonexistentSibling()` | `File getNonexistentSibling (bool putNumbersInBrackets = true) const` | OK |
| `juce::PluginHostType().getHostDescription()` | `const char* getHostDescription() const noexcept`, declared in juce_audio_processors/utilities; the static `jucePlugInClientCurrentWrapperType` is defined in `juce_PluginHostType.cpp` | Links in both the plugin and `InterferenceGuiTests` (both link juce_audio_processors) |
| `AudioProcessor::getWrapperTypeDescription (p.wrapperType)` | `static const char* getWrapperTypeDescription (AudioProcessor::WrapperType) noexcept`; `const WrapperType wrapperType` is public | OK |
| `TextEditor::moveCaretToEnd()`, `isEmpty()`, `setFont (const Font&)` | juce_TextEditor.h:568/378/251 | OK |
| `String << int / uint64 / const char*` in `Diagnostics.cpp` | overloads exist for int, long, unsigned long, int64, uint64, float, double | Every `uint32_t` atomic is cast to `(int)` first, so no ambiguous overload |
| `activitySince (...) const` locks `activityLock` | `GenericScopedLock (const LockType&)`; `CriticalSection::enter() const` | const-correct |

`windows.h` in `Diagnostics.cpp`: it is included after `<JuceHeader.h>` (which does `using namespace juce;`),
with `NOMINMAX` and `WIN32_LEAN_AND_MEAN`. I checked every identifier after line 13 against Win32 macros
(`GetCurrentTime`, `DeleteFile`, `CreateDirectory`, `near`/`far`, `ERROR`, etc.). The JUCE calls are lower-case
(`getCurrentTime`, `deleteFile`, `createDirectory`), so there is no clash. **Latent risk:** any future
*unqualified* `Rectangle`, `Polygon` or `Ellipse` in this file becomes ambiguous with the wingdi functions. Keep
every JUCE name qualified, or move the Win32 block into its own `.cpp` (see finding m1).

---

## 1. Major findings

### M1. Crash-log filter can remain in another component's chain after the DLL unloads, and unhooking breaks the chain (Diagnostics.cpp:360-371)
**What is wrong.** `removeHook()` handles the case "someone installed a filter after ours" by putting their
filter back. Their filter still holds `onUnhandledException` as its "previous" and will call it. Two problems:
1. After `previousFilter = nullptr`, our filter returns `EXCEPTION_CONTINUE_SEARCH` without forwarding, so the
   host's original filter is cut out of the chain.
2. When the last plug-in instance is destroyed, the host unloads the DLL. The other component then calls into
   unmapped code on the next crash. This is a use-after-unload, and the resulting crash is blamed on something
   else.

The POSIX path has a similar problem: `std::signal (sig, previous)` unconditionally overwrites any handler that
was installed after ours, such as a host crash reporter.

**Fix.** If you cannot unhook cleanly, keep forwarding and pin the module so the code stays mapped:
```cpp
void removeHook()
{
   #if JUCE_WINDOWS
    const auto current = SetUnhandledExceptionFilter (previousFilter);
    if (current != onUnhandledException)
    {
        // Another filter was installed after ours and may still chain to us: put it back, keep
        // forwarding to our predecessor, and keep this DLL mapped for the life of the process.
        SetUnhandledExceptionFilter (current);
        HMODULE self = nullptr;
        GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR> (&onUnhandledException), &self);
        stillChained = true;          // new static bool; start() must not re-install while true
        return;                       // previousFilter intentionally kept
    }
    previousFilter = nullptr;
   #else
    for (int i = 0; i < 5; ++i)
    {
        const auto cur = std::signal (kSignals[i], previousHandlers[i] != nullptr ? previousHandlers[i] : SIG_DFL);
        if (cur != onSignal)
            std::signal (kSignals[i], cur);   // someone replaced ours: leave theirs
    }
   #endif
}
```
In `installHook()`, when `stillChained` is true, only set `crashActive` and do not call
`SetUnhandledExceptionFilter`. Re-installing would make ours -> theirs -> ours recurse without end.

### M2. The crash filter allocates and runs `SymInitialize`, which can turn a crash into a host hang (Diagnostics.cpp:312-326)
**What is wrong.** `writeCrash` runs inside the unhandled-exception filter on the faulting thread. It calls
`juce::SystemStats::getStackBacktrace()`, which on Windows calls `SymInitialize (process, nullptr, TRUE)`. That
enumerates and loads symbols for every module, which takes the loader lock, and it builds a `juce::String` on
the heap.
- If the crash happened in `DllMain` or during a plug-in scan (a common case), another thread holds the loader
  lock and the filter deadlocks. The DAW then hangs instead of crashing, and hosts with crash recovery or
  sandboxing never restart.
- On heap corruption, the allocation faults again inside the filter.
- On `EXCEPTION_STACK_OVERFLOW`, the filter has almost no stack left.
- The filter is also not re-entrant: two threads crashing at once interleave their output.

**Fix.** Do only async-safe work in the filter. Open the file at `start()` and capture raw frames with the
exception context. Symbolise offline from module+offset.
```cpp
HANDLE crashHandle = INVALID_HANDLE_VALUE;   // opened in CrashLog::start with CreateFileW(..., FILE_APPEND_DATA,
                                             // FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, ...)
std::atomic_flag inFilter = ATOMIC_FLAG_INIT;

void writeCrashRaw (const EXCEPTION_RECORD* rec)
{
    if (! crashActive.load() || inFilter.test_and_set()) return;
    char line[256];
    auto put = [] (const char* s, int n) { DWORD w; WriteFile (crashHandle, s, (DWORD) n, &w, nullptr); };
    put (line, wsprintfA (line, "\r\n==== CRASH ====\r\ncode 0x%08lX at %p\r\n",
                          rec ? rec->ExceptionCode : 0ul, rec ? rec->ExceptionAddress : nullptr));
    void* frames[62];
    const USHORT n = RtlCaptureStackBackTrace (0, 62, frames, nullptr);
    for (USHORT i = 0; i < n; ++i)
    {
        HMODULE m = nullptr; char name[MAX_PATH] = "?";
        if (GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCSTR) frames[i], &m))
            GetModuleFileNameA (m, name, MAX_PATH);
        put (line, wsprintfA (line, "  %s + 0x%IX\r\n", PathFindFileNameA (name), (UINT_PTR) frames[i] - (UINT_PTR) m));
    }
    FlushFileBuffers (crashHandle);
}
```
If you do not want to link shlwapi, strip the path by hand instead of calling `PathFindFileNameA`. Note that
`abort()`, `std::terminate` and `/GS` / heap fail-fast (`__fastfail`) never reach an unhandled-exception filter.
Also add `signal (SIGABRT, ...)` on Windows and say in the Help text that some crashes cannot be logged.

### M3. The feedback-loop DC blocker is reset every block, giving a block-rate discontinuity and block-size-dependent output (StudioChain.h:170-175)
**What is wrong.** `setParams()` runs once per block and calls `fbDc[ch].prepare (fs, 10.0)`, and
`DcBlocker::prepare` calls `reset()` (Filters.h:31). The first sample of every block therefore gives `y = x`
instead of `x - x1 + r*y1`: a step at every block boundary, re-injected into the feedback loop. With Feedback
above 0 this makes a buzz at the block rate (94 Hz at 512 / 48 kHz, 375 Hz at 128). A live render and an
offline render at a different block size also differ.

**Fix.** Set the coefficient only when the sample rate changes, and never reset state from `setParams`:
```cpp
// StudioChain::prepare()
for (auto& d : fbDc) d.prepare (fs, 10.0);
// StudioChain::setParams(): delete   fbDc[ch].prepare (fs, 10.0);
```
Also add a test: with Feedback on, block sizes 64 and 333 must give outputs within 1e-6 (the ghost test already
uses this pattern).

### M4. Kill Feedback, Kill Tail and lane chokes clear whole delay lines on *every sample* (StudioChain.h:225-226, 361-367) — CPU blow-up and dropouts
**What is wrong.** `process()` runs once per sample. While Kill Feedback is held, or a choke step with "choke
kills feedback" is active, `killRamp` falls below 1e-3 after about 70 ms. From then on,
`for (auto& l : fbLine) l.clear();` runs every sample: 2 x 0.55 s x fs floats, about 53 k writes per sample at
48 kHz, or 2.5 G writes per second. The Kill Tail / choke-echo path is worse: `echo[ch].clear()` is 2.1 s x fs
per channel, per sample. This misses the deadline at once, and 192 kHz is 16x worse. It is reachable in normal
use through a lane step with Choke plus `gt_choke_fb` or `gt_choke_echo`. The slow soak test also toggles
`kill_fb` and pays this cost silently.

**Fix.** Clear once per kill episode:
```cpp
// members
bool fbCleared = false, tailCleared = false;
// process(), replacing line 225-226
if (killNow && killRamp < 1.0e-3f) { if (! fbCleared) { for (auto& l : fbLine) l.clear(); fbCleared = true; } }
else if (! killNow) fbCleared = false;
// after the channel loop, replacing the per-channel block at 361-367
if (tailKill && tailRamp < 1.0e-3f)
{
    if (! tailCleared)
    {
        for (int ch = 0; ch < 2; ++ch) { echo[ch].clear(); res1[ch].reset(); res2[ch].reset(); resFbState[ch] = 0.0f; }
        tailCleared = true;
    }
}
else if (! tailKill) tailCleared = false;
```
Reset both flags in `reset()`.

### M5. Parameters read once per block with no smoothing: zipper noise and block-size dependence (Engine.h:250-278, StudioChain.h:129-196)
**What is wrong.** `snapshotParams()` reads each parameter once per block, and many gains go straight into the
per-sample loop: `dryG`/`wetG` (Dry/Wet, Mix Law, Dry Trim), `outTrim`, `studioLevel`, `preDrive`/`preClip`,
`ceiling`, and the new `ghostAmt`. In the Studio chain: `drivePre`/`drivePost`, `wet[]`, `echoFb`, `resFb`,
`gateFloor`, `crushStep`. Automating any of these steps the gain once per block. That is audible zipper noise,
which becomes clicks at 1024-4096-sample host buffers, and the rendered sound depends on the host block size.
The echo and feedback delay times (`echoDelay`, `fbDelay`) also jump, so the read pointer clicks when Echo Time,
Feedback Delay or the synced tempo changes. Only `inTrim` (`trim`) and the excitation level are smoothed today.

**Fix.** Use per-sample one-pole smoothing with the existing `smoothCoeff`, and snap to the target on
`prepare` / `fullReset` so the exact-identity tests still hold (a one-pole in float converges exactly):
```cpp
// Engine members
float dryS = 0.0f, wetS = 1.0f, outS = 1.0f, studioLvlS = 1.0f, ghostAmtS = 0.0f; bool snapGains = true;
// fullReset(): snapGains = true;
// before the sample loop
if (snapGains) { dryS = dryG; wetS = wetG; outS = outTrim; studioLvlS = studioLevel; ghostAmtS = ghostAmt; snapGains = false; }
// top of the per-sample loop
dryS = dryG + (dryS - dryG) * smoothCoeff;          wetS = wetG + (wetS - wetG) * smoothCoeff;
outS = outTrim + (outS - outTrim) * smoothCoeff;    studioLvlS = studioLevel + (studioLvlS - studioLevel) * smoothCoeff;
ghostAmtS = ghostAmt + (ghostAmtS - ghostAmt) * smoothCoeff;
// ...then use dryS / wetS / outS / studioLvlS / ghostRamp * ghostAmtS in place of the block constants
```
In `StudioChain`, do the same for `drivePre`, `drivePost` and `wet[]`. For the delays, smooth
`echoDelaySmoothed += (echoDelay - echoDelaySmoothed) * k` per sample. That makes a tape-style glide; the
alternative is a 20 ms crossfade between two read heads.

### M6. Per-sample coefficients that do not depend on the sample rate: Contour, Auto Match, ceiling and React sound different at 44.1 and 192 kHz
**What is wrong.** These constants are applied once per sample, so their time constants scale with 1/fs:
- `StudioChain.h:273-274`: Contour `envFast` 0.2 / 0.002 and `envSlow` 0.005 / 0.0003. The release is 11 ms and
  75 ms at 44.1 kHz, but 2.6 ms and 17 ms at 192 kHz.
- `StudioChain.h:280`: `ctGain` 0.01.
- `StudioChain.h:317-322`: Auto Match 0.0002 / 0.0005.
- `Engine.h:551`: ceiling release 0.0005. It is also updated inside the channel loop, so stereo releases twice
  as fast as mono, and the left channel's value leaks into the right.
- `Engine.h:592-593`: sidechain `scFast` 0.999 and `scEnv` attack 0.01.

The offline renderer lets the user pick 44.1, 48 or 96 kHz, so a render does not match the session.

**Fix.** Derive every constant in `prepare()` from a time in seconds, and update `ceilGain` once per sample:
```cpp
// StudioChain::prepare
auto coef = [this] (double sec) { return (float) (1.0 - std::exp (-1.0 / (sec * fs))); };
kFastAtt = coef (0.00011); kFastRel = coef (0.011); kSlowAtt = coef (0.0045); kSlowRel = coef (0.075);
kCtGain = coef (0.0023); kRms = coef (0.113); kAuto = coef (0.045);
// process
envFast[ch] += (a - envFast[ch]) * (a > envFast[ch] ? kFastAtt : kFastRel);   // etc.

// Engine: ceiling, linked across channels, once per sample
float peakAll = 0.0f;  for (ch...) peakAll = std::max (peakAll, std::abs (tap[ch] * outTrim));
const float need = peakAll > ceiling ? ceiling / peakAll : 1.0f;
ceilGain = need < ceilGain ? need : ceilGain + (1.0f - ceilGain) * ceilRelCoef;   // ceilRelCoef = coef (0.045)
```
The values above keep today's 44.1 kHz feel. The sidechain detector needs the same treatment.

---

## 2. Minor findings — part 1 (new code)

**m1. Keep Win32 out of a JUCE translation unit (Diagnostics.cpp:6-16).** It compiles today (see §0), but
`using namespace juce` plus `<windows.h>` is fragile. Move the crash-hook code into
`Source/Plugin/CrashHookWin.cpp`, which should include only `<windows.h>` and `<atomic>` (no JUCE), with a small
C++ interface. That also makes M2 easier.

**m2. The Hard-reset confirm button takes the Return key (Overlays.cpp:251-259).** LookAndFeel_V2 gives Return
to button 1 ("Hard reset"). Swapping the button order does not help, because Escape would then trigger the
reset (code 0). Build the box yourself:
```cpp
auto* aw = new juce::AlertWindow ("Hard reset", msg, juce::MessageBoxIconType::WarningIcon, this);
aw->addButton ("Hard reset", 1);
aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey), juce::KeyPress (juce::KeyPress::returnKey));
aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int r) { if (safe != nullptr && r == 1) safe->performHardReset(); }), true);
```

**m3. Alert boxes are not tied to the plug-in window or its lifetime (Overlays.cpp:253, 317, 331; same pattern
in MainView).** Without `withAssociatedComponent (this)`, the box may open behind the host window. A box left
open when the last instance is deleted and the DLL unloads is a known JUCE teardown crash. Use
`juce::ScopedMessageBox confirmBox;` as a member plus
`confirmBox = juce::AlertWindow::showScopedAsync (opts.withAssociatedComponent (this), cb);`. It is dismissed
automatically when the overlay is destroyed.

**m4. `hardReset()` leaves MIDI learn pending (PluginProcessor.cpp:608-624).** A pending learn and any queued
`ccFifo` entry re-create a mapping on the next timer tick. Add this at the top of `hardReset()`:
```cpp
cancelMidiLearn();
{ int s1, n1, s2, n2; ccFifo.prepareToRead (ccFifo.getNumReady(), s1, n1, s2, n2); ccFifo.finishedRead (n1 + n2); }
```
Note that this is a read-side drain from the message thread, which is the single consumer, so it is safe. The
options file is shared by every open instance, and the others rewrite it from memory on their next save. Say
so in the Help text, or broadcast a reload.

**m5. The crash-log checkbox goes stale in other instances (Diagnostics.cpp:376-377, Overlays.cpp:301-309).**
`CrashLog::start` silently stops another instance's log without flushing that instance's pending activity, and
that instance's Debug window keeps showing "Logging to ...". Call `refreshCrashStatus()` from
`DebugOverlay::timerCallback()`. In `start`, append a line such as "log handed over to another instance"
before stopping.

**m6. The activity history cap drops crash-log lines under automation (PluginProcessor.cpp:725-728).** The
processor timer can add one line per changed parameter per tick, up to about 150 lines at 30 Hz. The crash log
is flushed every 15 ticks, but `history` keeps only 400 lines, so lines between flushes are lost without any
note. Either flush when `historyEnd - crashCursor > 300`, or write a line such as
"... N events not recorded" when `cursor < first` in `activitySince`.

**m7. The debug window's live text rebuilds a ~200-line TextEditor 5 times a second (Overlays.cpp:298).**
`setText` resets the scroll position, so reading needs Freeze, and `liveData` constructs a `PluginHostType`
(which does a path lookup) on every tick. Cache the host description in the constructor, only call `setText`
when the string changed, and consider 2 Hz.

**m8. Ghost Carrier is not really hidden, and it is active in Raw Hardware mode.** The four parameters are
normal APVTS parameters, so every DAW lists "Ghost Carrier" in its automation menu. If it should stay secret,
say so in the Help text or accept it. It is described as "a Studio-style effect", but it applies to `W` even
when Raw Hardware bypasses Studio. Gate it if that is intended: `ghostGain = ghostRamp * ghostAmt * (1.0f - rawMix);`.

**m9. The Ghost carrier phase jumps to 0 on every loop jump or seek (Engine.h:916).** This is deterministic,
which is good for repeatable renders, but it adds a discontinuity to the program material at 100 % Amount. The
DAW loop point is usually a discontinuity anyway, so this is low priority. If needed, crossfade the old and new
carrier over 5 ms after `softReset`.

**m10. Diagnostics.h:23-24 comment is out of date.** It says "no personal data beyond the host's name", but the
report contains full paths that include the Windows user name (Diagnostics.cpp:234, 248). `Docs/Privacy.md`
already discloses this, so only the comment needs fixing. Alternatively, replace
`File::getSpecialLocation (userHomeDirectory)` with `~` in the report.

**m11. The troubleshooting self-test has side effects.** It creates `Documents/Interference/Presets` and writes
a probe file there, and it runs a 1 s offline render on the message thread (UI freeze of roughly 50-200 ms).
This is acceptable, but mention it in the button tooltip.

**m12. GUI test depends on the environment (TestGui.cpp:366-448).**
- It fails with "[FAIL] user preset unreadable" if the developer's real `Documents/Interference/Presets` holds
  any bad `.ifxpreset`.
- It fails if Windows "Controlled folder access" blocks writes to Documents.
- It deletes and restores the real options file. If an earlier check aborts the test, the file is not restored.

Fix: point `StateIO::userPresetFolder()`, `Diagnostics::supportFolder()` and the PropertiesFile folder at a temp
directory in test builds (for example `#if IFX_GUI_TESTS`, or an env-var override). Restore the options file in
an RAII guard.

**m13. Slow perf tests (TestPerf.cpp).**
- `rt < 0.5` at 192 kHz (line 66) is likely to fail in Debug builds and while the developer is compiling at
  the same time. Gate it with `#if JUCE_DEBUG` or relax it in Debug.
- `peak <= kSafetyClamp` (line 109) and `allFinite` are vacuous, because the engine clamps and sanitises its
  output. Measure a pre-clamp bus instead (for example capture `Tap::FullWet`) or use a tighter bound such as
  `peak <= dbToGain (12.0f)`.
- The drift check compares wall time of 5-minute segments that contain 3 or 4 `kill_fb` episodes. With M4 fixed
  that is harmless; before the fix, each episode costs seconds of CPU.

---

## 3. Minor findings — part 2 (audio path)

Checked against the audio-dsp-review checklist. `processBlock` and everything it calls contain no `new`,
container growth, `std::function`, `juce::String`, locks, logging or file IO. FIFOs are
`juce::AbstractFifo` / seqlock, telemetry uses atomics, events are sorted in place with an insertion sort, and
there is a `ScopedNoDenormals`.

**m14. The offline renderer does not set FTZ/DAZ (OfflineRenderer.cpp:66).** Add
`juce::ScopedNoDenormals noDenormals;` at the top of `OfflineRenderer::render`. Without it, renders and the
self-test render run without flush-to-zero. `ContactModel::g1/g2`, `MotionDriver` and several `x += (t-x)*k`
followers then decay into denormals, which is slow and can leave last-bit differences from live playback.

**m15. Non-finite host parameters stick in DSP state (PluginProcessor.cpp:171-175).** A NaN from a host or
automation passes through `NormalisableRange` (`jlimit` does not catch NaN) into `snap`. The output guard turns
it into silence, but `preSat`, the Biquads and the Studio state stay NaN until the next full reset. Fix:
```cpp
for (int i = 0; i < kNumParams; ++i)
{
    float v = raw[i]->load (std::memory_order_relaxed);
    const auto& inf = paramInfo (i);
    snap[i] = std::isfinite (v) ? std::clamp (v, inf.min, inf.max) : inf.def;
}
```

**m16. Auto Match gain (up to 4x) sits inside the local feedback loop, but the loop-gain bound ignores it
(StudioChain.h:176-181, 315-325).** With Drive at net < 0 dB and heavy compression, the small-signal loop gain
can pass 1 while input is present. `tanh` bounds it and it decays once input stops, so it stays bounded, but
it contradicts "every tail is finite". Fix: `if (autoMatch && (on[Drive] || on[Crush])) loopGain *= 4.0;`, or
apply `autoGain` after the feedback tap.

**m17. Instant switches that can click:**
- Monitor tap (Engine.h:545-546)
- Source Circuit / `inputMode` (410)
- Input Stage Drive `preOn` (468-475; it also resets ADAA state)
- Studio Source `studioSrc` (486-491)
- Excitation type (248; the waveform changes at full gain)
- Mix Law
- Ceiling on/off (547)

Fix: reuse the existing 10 ms `rampTo` pattern. For example, keep `prevTap` and crossfade `tapVals[prev]` to
`tapVals[cur]` with a ramp; for `excType`, fade `excGain` to 0, then switch.

**m18. Large memsets on the audio thread at transport start and seek (Engine.h:896-905, StudioChain.h:69-100).**
`fullReset()` calls `studio.reset()`, which clears about 4 MB at 192 kHz. That is bounded and one-off (under
1 ms), but it happens on a real-time callback. Consider clearing only the region the read heads can reach, or
spreading the clear over a few blocks.

**m19. `prepareToPlay` touches message-thread state (PluginProcessor.cpp:158).** It calls
`pushNonParamToEngine()`, which copies `pattern` and iterates `midiMap` in `rebuildCcTable()`. Some hosts call
`prepareToPlay` off the message thread while the UI edits the lane or learn adds a mapping, which is a data
race (the `juce::Array` can reallocate during the loop). Fix: in `prepareToPlay`, publish only what the engine
needs from a copy taken under a small `SpinLock` that the message thread also holds while it mutates, or
defer the call with `MessageManager::callAsync`.

**m20. Negative ppq modulo (Engine.h:641).** In pre-roll or count-in, a negative `ppqEnd` makes `laneStep`
negative, so the UI playhead disappears. Fix:
`((int64_t) std::floor (ppqEnd / ss.stepBeats) % n + n) % n`.

**m21. Ghost Carrier aliasing and cost.** Ring modulation by a carrier up to 2 120 Hz folds content above
`fs/2 - 2120 Hz` (about 19.9 kHz at 44.1 kHz). This is negligible and acceptable for the effect. It also costs
two `std::sin (double)` calls per sample; a quadrature rotator (`c' = c*cos d - s*sin d`, renormalised once per
block) is cheaper if the cost-breakdown test shows it matters.

---

## 4. What is done well

- Thread safety of the new code is correct: all file IO and string work (`noteActivity`, `activitySince`,
  `CrashLog::append`) runs on the message thread under `activityLock`. The only new audio-thread code is the
  ring modulator, which is allocation-free and uses per-sample phase accumulators, so it does not depend on
  block size. Its on/off switch uses the 10 ms `rampTo` declick and is an exact identity when off (the test
  checks this).
- UI lifetimes are handled correctly:
  - `HelpOverlay::onDebug` defers `showDebug()` through `callAsync` plus a `SafePointer`, so the Help overlay is
    never destroyed inside its own button callback.
  - `Overlay::close` is asynchronous.
  - The hard-reset callback captures `SafePointer<DebugOverlay>`.
  - `DebugOverlay` stops its timer in the destructor.
  - The processor stops the crash log in its destructor, before the DLL can unload.
- `CrashLog` is opt-in, off on every load, and has a single owner per process. The Windows path is copied into
  a static wide buffer at `start()`, so no JUCE path conversion happens at crash time. The UTF-16 byte-count
  argument is correct.
- The new tests are well aimed: the ghost test checks wet-only, bounded and block-size-independent behaviour
  with an exact-identity baseline, and the GUI test covers the secret pixel, the misconfiguration text, the
  self-test, the crash log round trip and the hard reset.
- DSP hygiene elsewhere is good: double-precision TDF-II biquads with state flushing, ADAA with a
  small-difference fallback, `undenorm` in one-poles, a loop-gain clamp on the local feedback, an input and
  output NaN guard plus a safety clamp, and a 10 ms linear declick for bypass, slot, order, Studio and Raw
  Hardware switches.
