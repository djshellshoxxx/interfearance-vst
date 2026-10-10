# Spec A — Essentials (E5–E8)

---
## E5 — Working, trustworthy CI

**Problem.** On PR #2 every job (Windows, Linux, and a docs check that only reads two files) failed in 20–50 s, a re-run
failed identically, and job logs return 404 through the API. The cause is almost certainly outside the code (billing /
spending limit, Actions policy, or runner availability) but is unconfirmed. CI is the foundation of every other task.

### E5.0 Triage (owner action, 10 min) — do this before any code
1. Repo → Actions → open a failed job → read the red annotation at the top (it shows "recent account payments have
   failed", "spending limit", "runner image removed" or a step error).
2. Settings → Actions → General: Actions allowed; workflow permissions "Read" is fine (workflow declares `contents: read`).
3. Settings → Billing: private repos need Actions minutes. Windows minutes cost 2×, macOS 10×.
4. If the annotation names an image (`windows-2022`, `ubuntu-22.04`) that is retired, switch to the current label
   (`windows-2025`, `ubuntu-24.04`) — check the VS generator string (`Visual Studio 17 2022` is on both).
5. Paste the annotation into the PR; fix before continuing.

### E5.1 Goals
* Every push/PR: format check, docs check, Linux build+tests, Windows build+tests, artifacts, in ≤ 25 min wall time.
* Nightly: slow suite, sanitizers, pluginval, and a dependency-drift build.
* Tags `v*`: unsigned (or signed if the secret exists) Windows ZIP + checksum attached to a GitHub Release.
* Failures are readable: logs uploaded as artifacts even when a step fails, and a summary table in `$GITHUB_STEP_SUMMARY`.

### E5.2 Workflow design (`.github/workflows/`)
Split the single `ci.yml` into files so a change to one does not conflict with another:

| File | Trigger | Jobs |
|---|---|---|
| `ci.yml` | push, pull_request | `checks` (licence, params-doc, **param-append-only guard**, `git clang-format` on diff, changelog-fragment present), `linux` (GCC 13 Release: build, `InterferenceTests`, `InterferenceGuiTests` under `xvfb-run`, VST3+CLAP+Standalone upload), `windows` (MSVC, warnings-as-errors, tests, VST3/CLAP/Standalone + ZIP) |
| `nightly.yml` | cron `0 3 * * *`, dispatch | `slow` (TestPerf + 1 h soak), `linux-asan-ubsan` (Debug + `-fsanitize=address,undefined`, both test binaries), `linux-tsan` (Debug + thread sanitizer, FIFO/seqlock tests only), `pluginval` (strictness 10 against Windows VST3 + Linux VST3), `clap-validator` (CLAP), `clang-tidy` (subset, report only) |
| `release.yml` | tag `v*` | build from the tagged commit, run tests, make ZIP/`.sha256`, sign if `SIGNING_CERT_BASE64` exists in environment `release`, create Release with notes from `changelog.d` |

Details that matter:
* `concurrency: { group: ci-${{ github.ref }}, cancel-in-progress: true }` on PR branches only (never on tags/nightly).
* Pin every third-party action to a full commit SHA (comment the tag); Dependabot `github-actions` ecosystem weekly.
* Cache: JUCE clone (`actions/cache`, key = tag), CMake `_deps` (clap-juce-extensions), **ccache** on Linux (`hendrikmuhs/ccache-action`),
  `sccache` or MSBuild `/MP` + restored `build/` intermediate dir on Windows keyed by hash of `CMakeLists.txt` and `Source/**`.
  Target: warm Windows build ≤ 12 min (cold was ~7 min on 4 cores for Linux).
* Linux system deps in one script `tools/ci/install_linux_deps.sh` (also used by `CLAUDE.md` and developers).
* `IFX_DATA_ROOT` set to a temp folder for all test runs (already supported).
* Build type matrix on Linux: GCC 13 and Clang 17 (`-Werror` via `-DCMAKE_COMPILE_WARNING_AS_ERROR=ON`); Windows MSVC only.
* Upload on `if: always()`: `Testing logs`, GUI snapshots, crash dumps.
* Artifact names are stable: `interference-<ver>-<sha7>-<platform>`.
* **Param append-only guard** `tools/check_params_append_only.py`: parse `Core/Params.h` + `ParamsExt/*.def` on the PR
  and on `origin/master`; fail if any `id` string present on master is missing/changed, if any choice list lost or reordered
  an entry, or if min/max narrowed. (Pure Python, ~60 lines.)
* **Changelog fragment check**: PRs touching `Source/` must add `changelog.d/<id>.md` (skip label `no-changelog`).

### E5.3 Branch protection (owner action)
`master`: require PR, require `checks`, `linux`, `windows` to pass, linear history, dismiss stale approvals off (so bot
pushes don't churn), require conversation resolution. Disallow force-push.

### E5.4 Acceptance criteria
* A trivial docs PR goes green in ≤ 5 min (path filters skip builds for `Docs/**`-only changes but still run `checks`).
* A deliberately broken test makes `linux` and `windows` red with the failing test name in the summary.
* Re-running a failed job reproduces the same result (no flaky infra); `nightly.yml` completes at least 3 nights.
* A tag `v0.0.1-beta.N` produces a Release with ZIP + checksum.

### E5.5 Tests of the CI itself
`tools/ci/selftest.sh` runs the guard scripts against fixture trees (renamed id → fails; appended id → passes).

---
## E6 — Crash reporting on every platform

**Current state (verified in code).** `CrashHook.h/CrashHookWin.cpp` already implement: Windows unhandled-exception filter +
`SIGABRT`; POSIX `sigaction` handlers (`kSignals`, `onSignal`) with `backtrace()` warm-up and chaining to previous handlers.
Opt-in via Help → Debug (`InterferenceProcessor::startCrashLog`), never persisted. Not covered: any automated test of the
POSIX path, stack-overflow crashes, symbolication, macOS, or a way to *offer* the log after an abnormal exit.

### E6.1 Requirements
1. **Prove the POSIX path.** Test `CrashHookPosix` (Linux CI): fork a child that calls `CrashHook::install(path)`, then
   raises `SIGSEGV`, `SIGABRT`, `SIGFPE`, `SIGILL`, `SIGBUS`; parent waits, asserts the child died by that signal, the log
   exists, contains `signal`, the signal number, ≥ 3 frame lines, and that a previously installed handler also ran
   (chaining). Add a stack-overflow child (infinite recursion) — requires E6.2.
2. **`sigaltstack`**: allocate a 64 KiB alternate stack at `install()` (per installing thread; document that other
   threads rely on the host's) and register handlers with `SA_ONSTACK`, otherwise stack overflow cannot be reported.
3. **Async-signal-safety audit.** The handler may call only `write`, `_exit`, `sigaction`, `backtrace_symbols_fd`,
   `dladdr` is *not* async-signal-safe: record raw addresses (`backtrace`) and module base from a table captured at
   `install()` (`dl_iterate_phdr` on Linux, `_dyld_get_image_header` on macOS) so the handler prints `module+0xOFFSET`.
   Audit comment block listing each call and why it is safe.
4. **Symbols.** Build step produces separate debug files: Windows `Interference.pdb` (already produced), Linux
   `objcopy --only-keep-debug … && strip --strip-unneeded` + `.gnu_debuglink`, macOS `dsymutil`. Release workflow uploads
   them as a private artifact (never inside the public ZIP). `tools/symbolize.py <log> <symbols-dir>` resolves
   `module+offset` with `addr2line` (Linux), `llvm-symbolizer` (any), or `dbghelp` via `cdb`/`llvm-symbolizer` (Windows).
5. **Abnormal-exit detection (opt-in prompt, no telemetry).** On editor open, if `Support/running.lock` exists from a
   previous session (written at `startCrashLog` or at plug-in construct, removed in destructor) show a one-time,
   dismissible banner: "Interference may have closed unexpectedly last time. Turn on the crash log to capture details
   next time?" with [Turn on] [Not now] [Don't ask]. Stored in options. Never sends anything anywhere (Privacy.md unchanged).
6. **Report bundle.** Help → Debug → "Create support bundle": ZIP in `Support/` containing latest crash log,
   troubleshooting report, current preset, version/host info; shows the folder. Uses `juce::ZipFile::Builder`; size cap 5 MB.
7. **Platform paths.** `StateIO::userDataFolder()` uses Documents. On Linux use `$XDG_DATA_HOME/Interference`
   (default `~/.local/share/Interference`) unless the legacy Documents folder already exists; macOS
   `~/Library/Application Support/Interference`. Migration: if the new folder is absent and the old exists, keep using old.
8. **Windows**: keep behaviour. Add `MiniDumpWriteDump` **not** in-process; instead document using WER LocalDumps registry
   key (Docs/Troubleshooting.md) — avoids dbghelp in a crashing process.

### E6.2 Files
`Plugin/CrashHookWin.cpp` (rename → `CrashHook.cpp`, git mv in its own commit to keep history), `Plugin/Diagnostics.cpp`
(`SupportBundle`, lock file), `UI/Overlays.cpp` → new `UI/DebugOverlay.cpp` additions, `tools/symbolize.py`,
`tools/ci/make_symbols.sh`, `Tests/FeatureCrash.cpp`, `Docs/Troubleshooting.md`.

### E6.3 Acceptance criteria
* `Tests FeatureCrash.posixSignalsAreLogged` (5 signals) and `…stackOverflowIsLogged` pass on Linux CI.
* `symbolize.py` turns a log from a deliberately crashing test build into function names (golden test with a tiny
  `crash_demo` binary built in CI).
* Banner appears only after a simulated abnormal exit (GUI test creates the lock file) and never again after "Don't ask".
* Support bundle opens in any unzip tool, contains ≤ 5 files, no audio.
* No regression in `CrashLog` tests (chained-handler and uninstall-order rows).

### E6.4 Risks
Signal handlers inside a host process are dangerous: keep them chained, off by default, uninstall on plug-in destroy
(already designed). Do not install on threads we don't own.

---
## E7 — UI scale and HiDPI

**Current state.** `PluginEditor`: `setResizable(true,true)`, limits `kMinW 1000, kMinH 700, kMaxW 2400, kMaxH 1600`, default
1200×800, size persisted in `settings()` (`editorW/H`). Knobs scale with layout cells (Decisions.md). No user zoom, no
verified behaviour at 125/150/200 % OS scaling or on multi-monitor DPI changes, no keyboard zoom, min size excludes small
laptop screens at 200 % (1000×700 physical needs ≥ 1920×1080 at 150 %).

### E7.1 Design
* **UI Scale option**: 75 %, 90 %, 100 %, 115 %, 125 %, 150 %, 175 %, 200 % (Options overlay, plus `Ctrl+=` / `Ctrl+-` /
  `Ctrl+0` when the editor has focus). Stored globally in the options file (`uiScale`, float), not in presets/projects.
* **Implementation**: apply `mainView->setTransform(AffineTransform::scale(s))` and size the *logical* content to
  `editor size / s`; never call `AudioProcessorEditor::setScaleFactor` (hosts and JUCE wrappers also call it; combining them
  double-scales). Hit-testing and tooltips work through transforms in JUCE; drag handles in custom components must use
  `e.getEventRelativeTo(this)` (audit `LaneEditor`, `Illustrations`, `FineSlider`).
* **Limits**: `setResizeLimits` in physical px = `base limits × s` clamped to the host's available screen
  (`Desktop::getInstance().getDisplays().getDisplayForPoint(...)->userArea`). Default window = `min(base default × s, 90 % of screen)`.
* **Auto-fit**: on first run choose the largest preset scale such that the default window fits 85 % of the work area.
* **Per-monitor DPI**: rely on JUCE's native scale; add `componentMovedOrResized`/`displayChanged` hook to re-clamp limits.
* **Crispness**: custom-painted components use `Graphics` vector paths (no cached bitmaps at fixed size). Audit
  `Illustrations.cpp` for `Image` caches: key them by `scaleFactor`. Fonts via `Theme::*Font(size)` already sized in points.
* **Minimum hit target** 24×24 logical px for any clickable control (checked by a GUI test that walks the tree).

### E7.2 Files
`Plugin/PluginEditor.cpp/.h`, `UI/Overlays.cpp` (OptionsOverlay: add a `ComboBox`), `UI/Theme.*` (no change expected),
`UI/LaneEditor.cpp`, `UI/Illustrations.cpp`, `Tests/FeatureUiScale.cpp`.

### E7.3 Acceptance criteria / tests
* `uiScale.persistsAndRestores`, `uiScale.limitsFollowScale`, `uiScale.keyboardShortcuts`.
* `GuiSnapshots at 75/100/150/200 %` render without clipped text (existing snapshot harness + automated check that every
  `Label`/`Button` text fits its bounds: `getFont().getStringWidthFloat(text) <= getWidth()`).
* Lane editor click at the centre of step N hits step N at every scale (hit-test table).
* Manual matrix recorded in QA.md: Windows 100/125/150/200 %, two monitors with different scale, Linux X11 2×, FL Studio,
  Reaper (Windows).

---
## E8 — "Does it sound right?" calibration and listening validation

**Problem.** `Docs/Research.md` says models are research-informed, not measured; PhysicalLimits are "tunable, unmeasured".
Without evidence the realism claims are marketing. This spec creates the measurement pipeline and the decision rules; it
does not claim results.

### E8.1 Deliverables
1. `Docs/Calibration.md` — protocol, results tables, claim wording policy.
2. `tools/calibrate/` (Python 3.11, numpy, scipy, soundfile; `requirements.txt`; no GPL deps):
   * `features.py` — extracts the feature set below from a WAV.
   * `compare.py` — compares recording features vs `ifx-render` output for a scenario; emits `report.json` + plots.
   * `scenarios.yaml` — each scenario = hardware condition + gesture script + preset/state file + expected bands.
3. `Tests/data/calibration/` — tiny (≤ 5 MB total) CC0 reference recordings and the extracted feature JSONs used by CI.
4. Listening-test kit `tools/calibrate/abx/` — static HTML (no server) that plays randomised A/B/X trials from WAVs and
   exports CSV; analysis script reports per-listener p-values (binomial) and overall d′.

### E8.2 Measurement protocol (fieldwork, ~2 days)
Rig: audio interface with Hi-Z DI, 1/4" TS cables (new, worn, damaged), jacks (healthy, loose), 250 kΩ pots (clean, dirty),
a handheld finger-touch rig, constant 1 kHz −20 dBFS tone and silence as sources. Record 24-bit/96 kHz, 60 s each, 3 takes,
note room (mains 50/60 Hz, nearby gear). Scenarios (minimum viable set, 12): jack healthy still · jack wiggled · jack
half-inserted · finger on tip (hum) · finger pressure sweep · cable flexed at weak spot · cable shaken · pot still · pot
turned slowly · pot turned fast · dimmer-lit room hum · ground-loop with two-prong gear.

### E8.3 Features compared (all computed on the *wet minus dry* component where possible)
* Level: RMS dBFS, crest factor, percentile (p50/p95/p99) of short-term (50 ms) level.
* Spectrum: 1/3-octave band levels 20 Hz–20 kHz; hum harmonic ratios (H2/H1…H8/H1 at 50/60 Hz); spectral centroid and slope.
* Events: crackle event rate (threshold at median + 12 dB MAD, 5 ms refractory), inter-event interval histogram, burst
  length distribution, event amplitude distribution (log-normal fit).
* Envelope: 0.1–20 Hz modulation spectrum of the noise envelope.
* Stereo: inter-channel correlation (when stereo recorded).

### E8.4 Acceptance bands and decisions
* Each scenario gets a band per feature = recording mean ± max(1.5×SD across takes, tolerance floor: level ±3 dB,
  harmonic ratios ±4 dB, event rate ×/÷ 1.5). A model **passes** a scenario when ≥ 80 % of its features are in band.
* Failing features generate a tuning ticket pointing at the `PhysicalLimits.h` row or source constant to change.
  Changes are made only with the comparison report attached; fixtures update only with a CHANGELOG "sound changed".
* **Claim policy:** UI/marketing may say "calibrated against N recordings of …" only for scenarios passing; everything else
  keeps "research-informed" wording (already used in Manual/README). `Docs/Research.md` calibration table lists pass/fail.
* **Blind test:** ≥ 8 listeners × 20 ABX trials per scenario group (real vs plug-in, same level-matched, 6 s excerpts).
  Reported honestly: "indistinguishable" only if the pooled hit rate's 95 % CI includes 50 %; otherwise state the rate.

### E8.5 Engineering tasks
1. Depends on F0.1 (`ifx-render`) with `--metrics` and `--gesture-script` (JSON list of `{t, gesture, intensity, target}`)
   so scenarios are reproducible headlessly.
2. `Tests/FeatureCalibration.cpp` loads the checked-in feature JSONs and asserts renders stay in band (regression guard, runs
   in normal CI, ≤ 20 s).
3. `PhysicalLimits.h` rows gain a `status` enum {Unmeasured, Measured(date, report id)} printed by Help → Debug.

### E8.6 Risks
Fieldwork needs a person with hardware; blocked work is clearly marked "measured: pending" and never fabricated.
