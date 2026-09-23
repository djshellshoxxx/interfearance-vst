# TODO — Interference (autonomous run)

Specs: INTERFERENCE_VST_CLAUDE_CODE_MASTER_PROMPT.md (master), INTERFERENCE_VST_REALISM_CONSTRAINTS.md,
INTERFERENCE_VST_EFFECTS_CHAIN_AND_ROUTING.md, INTERFERENCE_VST_INDUSTRIAL_PERFORMANCE.md,
include.md, theme.md, JUCE_CLAUDE_GUIDELINES.md. Decisions: DECISIONS.md. History: PROGRESS.md.

Build: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` then `cmake --build build --config Release -- -m:2`.
Tests: `build/InterferenceTests_artefacts/Release/InterferenceTests.exe` (DSP, 120) and
`build/InterferenceGuiTests_artefacts/Release/InterferenceGuiTests.exe` (host + GUI, 12).
Pacing: user asked for 30 min work / 10 min break cycles (other Claude instance has priority); build with -m:2.

## Remaining
- [ ] WP12 Docs: README, Docs/Decisions.md (mirror), Docs/Research.md, Docs/Manual.md (quick-start per mode, sources' physical
      basis, effect vs generator, two-slot order/linking, seed/loop reproducibility, export limits, level guidance),
      Docs/Install.md (install/uninstall/update), CHANGELOG.md (semver, compat policy), LICENSE, THIRD_PARTY.md,
      Docs/QA.md (commands, results, unverified gates incl. DAW load + calibration), Docs/Privacy.md, Docs/MidiMap.md,
      Docs/Routing.md (diagram), Docs/Issues.md
- [ ] Release: tools/make_release.ps1 producing a reproducible ZIP (VST3 + docs); run it
- [ ] Validation: pluginval strictness 10; run DSP tests with --slow
- [ ] WP13 audit: debug window, crash log, troubleshooting export, hard reset, easter egg — verify each is wired
- [ ] Warnings cleanup (C4458/C4459/C4244 in PluginProcessor/Theme/Widgets/Illustrations)

## Done
- [x] pluginval strictness 5 on Release VST3: pass (exit 0) — 2026-09-23 session 3
- [x] GUI tests fixed (reset tolerance for log ranges; compact output strip at 1000x700): 12/12 pass — session 3
- [x] WP6 Plugin processor/editor, APVTS, state, MIDI/learn, A/B, reset, randomize, buses incl. sidechain (commit bb22105)
- [x] WP7 Render/Record engine + UI, WP8 presets + UI, WP9 full UI, WP10 help/options/icon (commit bb22105)
- [x] WP11 TestGui.cpp (host contract, state, MIDI, resize, overlays, pairs, concurrency, teardown during render)
- [x] StateIO, FactoryPresets (22 presets, 6 categories), OfflineRenderer + LiveRecorder, TestState/TestRender
- [x] WP1–WP5 Core, sources, Engine, StudioChain, gestures/scheduler; offline tests (120 passing)
- [x] CMakeLists.txt (JUCE 8.0.8 lookup: ../JUCE_8_0_8)
