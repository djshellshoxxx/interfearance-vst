# TODO — Interference (autonomous run)

Specs: INTERFERENCE_VST_CLAUDE_CODE_MASTER_PROMPT.md (master), INTERFERENCE_VST_REALISM_CONSTRAINTS.md,
INTERFERENCE_VST_EFFECTS_CHAIN_AND_ROUTING.md, INTERFERENCE_VST_INDUSTRIAL_PERFORMANCE.md,
include.md, theme.md, JUCE_CLAUDE_GUIDELINES.md. Decisions: DECISIONS.md. History: PROGRESS.md.

Build: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` then `cmake --build build --config Release -- -m:2`.
Tests: `build/InterferenceTests_artefacts/Release/InterferenceTests.exe [--slow]` (DSP) and
`build/InterferenceGuiTests_artefacts/Release/InterferenceGuiTests.exe` (host + GUI).
Machine limits: low RAM and a higher-priority Claude instance share it. Write code/docs first, build once at the end
with -m:2; 30 min work / 10 min break.

## Remaining
- [ ] Build Release (first compile of WP13 + perf tests); fix errors and warnings
- [ ] Run DSP tests, GUI tests, and DSP tests with --slow (CPU + 1 h soak); fix failures
- [ ] pluginval strictness 10 on the Release VST3
- [ ] Run tools/make_release.ps1; check the ZIP contents and checksum
- [ ] Docs/QA.md with the exact commands, results, CPU numbers and unverified gates; README/manual CPU line

## In progress
- (none)

## Done
- [x] WP13 code: debug window (live data, misconfiguration checks, freeze), opt-in crash log (chained exception filter),
      troubleshooting export with self-test, hard reset (deletes options file), hidden Ghost Carrier + secret pixel;
      tests Engine.ghostCarrier…, Gui.debugToolsCrashLogHardResetAndSecretPixel (written, not yet compiled)
- [x] WP12 docs: README, Docs/Research.md, Routing.md, MidiMap.md, Install.md, Issues.md, Decisions.md (mirror),
      manual compatibility section, CHANGELOG release notes, privacy + credits; tools/make_release.ps1
- [x] Warnings cleanup C4458/C4459/C4244 (commit 5cd5da2)
- [x] LICENSE, THIRD_PARTY.md, CHANGELOG.md, Docs/Privacy.md (commit 5cd5da2)
- [x] pluginval strictness 5 on Release VST3: pass (exit 0) — 2026-09-23 session 3
- [x] GUI tests fixed (reset tolerance for log ranges; compact output strip at 1000x700): 12/12 pass — session 3
- [x] WP6 Plugin processor/editor, APVTS, state, MIDI/learn, A/B, reset, randomize, buses incl. sidechain (commit bb22105)
- [x] WP7 Render/Record engine + UI, WP8 presets + UI, WP9 full UI, WP10 help/options/icon (commit bb22105)
- [x] WP11 TestGui.cpp (host contract, state, MIDI, resize, overlays, pairs, concurrency, teardown during render)
- [x] StateIO, FactoryPresets (22 presets, 6 categories), OfflineRenderer + LiveRecorder, TestState/TestRender
- [x] WP1–WP5 Core, sources, Engine, StudioChain, gestures/scheduler; offline tests (120 passing)
- [x] CMakeLists.txt (JUCE 8.0.8 lookup: ./JUCE, ../JUCE_8_0_8)
