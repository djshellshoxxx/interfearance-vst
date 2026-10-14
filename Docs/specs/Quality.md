# Quality plan — tests, gates, tooling

## 1. Local gate (run before every push)
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_COMPILE_WARNING_AS_ERROR=ON   # Windows: VS 2022 generator
cmake --build build --parallel
IFX_DATA_ROOT=$(mktemp -d) build/InterferenceTests_artefacts/Release/InterferenceTests
xvfb-run -a build/InterferenceGuiTests_artefacts/Release/InterferenceGuiTests      # Linux; Windows runs it directly
python3 tools/gen_param_doc.py && git diff --exit-code Docs/Parameters.md
python3 tools/check_params_append_only.py origin/master
```
Fast loop while developing: `InterferenceTests --filter <text>` (~seconds).

## 2. Test layers
| Layer | Where | Purpose | Budget |
|---|---|---|---|
| Unit (Core) | `Tests/Test*.cpp` | pure functions: Randomizer, PatternGen, PresetCode, MacroMap | ms each |
| DSP / scenario | `TestEngine/Sources/Scenarios/Rf/Studio` | physical realism rows, bounds, determinism, block-size independence | total ≤ 40 s |
| State | `TestState` + `Tests/fixtures` | round trips, old files, unknown fields | ≤ 5 s |
| GUI / host contract | `TestGui` | editor builds headless, parameter count, accessibility, focus order, snapshots | ≤ 30 s |
| Perf (slow) | `TestPerf --slow` | CPU gate, 1 h soak | nightly |
| Fuzz | libFuzzer targets in `Tests/fuzz/` (state XML, preset code, pack manifest, MIDI) | no crash, bounded memory | nightly 10 min each |
| Golden audio | `Tests/golden/*.hash` | SHA-256 of 24-bit renders of 30 scenarios | updated only with "sound changed" |

## 3. Cross-cutting automated checks (add once, benefit every feature)
* **No-allocation test**: in test builds override global `operator new/delete` with a counter; `processBlock` for 1 000 blocks
  across all modes/sources must allocate 0 times (catches regressions from every feature).
* **Parameter sweep fuzz**: existing random-jump tests extended to every new parameter automatically via `kNumParams` loops.
* **Block-size independence**: render the same scenario at block sizes {1, 31, 64, 480, 4096}; outputs must be identical (already
  required by determinism policy) — run it for every new stage.
* **Sample-rate matrix**: 44.1/48/88.2/96/192 kHz for new DSP.
* **Denormal/NaN guard**: `isfinite` + `abs < 4` on every output in all tests (helpers exist in `TestHelpers.h`).
* **Thread safety**: nightly TSAN build running FIFO/seqlock tests and a two-thread hammer test (UI mutating params while rendering).
* **ASAN/UBSAN**: nightly Linux Debug, both binaries.
* **Static analysis**: `clang-tidy` (bugprone-*, performance-*, concurrency-*) report-only first, then gate once clean.
* **Header hygiene**: `Core/` includes no JUCE (compile a TU that includes all Core headers with JUCE not on include path).

## 4. Bug triage and tracking
Severity: S1 crash/data loss/audio blast · S2 wrong sound/state · S3 UI defect · S4 cosmetic. S1/S2 block releases.
`Docs/Issues.md` keeps the fixed-issue log; open bugs live in GitHub Issues with labels `S1..S4`, `area/*`, `platform/*`.
Every bug fix PR adds a regression test named `regress.<issue>`.

## 5. Manual QA matrix (per release; results into QA.md)
| Area | Cases |
|---|---|
| Hosts (Windows) | FL Studio, Reaper, Ableton Live, Cubase/Studio One if available: scan, insert, automate, save/reopen, bounce, offline render equals real-time within tolerance |
| Standalone | audio device pick, MIDI in, sample rates 44.1–192, device change while running, window resize/scale |
| Install/uninstall | clean machine, upgrade over previous, uninstall leaves user presets |
| Safety | first load level sane (no blast), bypass, Reset, extreme parameter randomise, 1 h run |
| Accessibility | keyboard-only flow, NVDA, high contrast |
| Locale | non-English Windows, user name with spaces/unicode in path, Documents redirected to OneDrive |

## 6. Release gate (a release is blocked unless all are true)
All automated suites green on Windows + Linux · nightly green 3 nights · pluginval strictness 10 pass · no S1/S2 open · CHANGELOG
assembled · QA.md results updated · SHA-256 + SIGNATURE.txt correct · fixtures updated for any "sound changed" · tag created from `master`.

## 7. Speed-up notes (for humans and AI agents)
* `CLAUDE.md` at repo root: build/test commands, hotspot map, "where things live" table, rules from Standards.md (kept ≤ 120 lines).
* `tools/dev.sh` / `tools/dev.ps1`: one command each for configure, build target, run test filter, run GUI test, regenerate docs.
* ccache/sccache configured by default in CMake when found (`CMAKE_CXX_COMPILER_LAUNCHER`).
* Use `-DINTERFERENCE_BUILD_CLAP=OFF` and build only `InterferenceTests` for DSP work (skips plugin wrappers; ~3× faster).
* Unity builds are **not** used (they hide ODR/include errors); precompiled header for `JuceHeader.h` is acceptable.
* Scenario recorder: Help → Debug can export the current state + gesture script as a `scenarios.yaml` entry to turn any bug
  report into a reproducible test in one step.
* PR template + CODEOWNERS (Standards §8) so reviews are fast and consistent.
