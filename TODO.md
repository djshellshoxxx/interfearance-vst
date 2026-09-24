# TODO — Interference (autonomous run)

Specs: INTERFERENCE_VST_CLAUDE_CODE_MASTER_PROMPT.md (master), INTERFERENCE_VST_REALISM_CONSTRAINTS.md,
INTERFERENCE_VST_EFFECTS_CHAIN_AND_ROUTING.md, INTERFERENCE_VST_INDUSTRIAL_PERFORMANCE.md,
include.md, theme.md, JUCE_CLAUDE_GUIDELINES.md. Decisions: DECISIONS.md. History: PROGRESS.md.
Agent reports: Docs/research/SpecAudit.md, CodeReview.md, ElectricalInterference.md.

Build: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` then `cmake --build build --config Release -- -v:m`
(from Git Bash use `-v:m`, not `/v:m`). Tests: `build/InterferenceTests_artefacts/Release/InterferenceTests.exe [--slow]`
and `build/InterferenceGuiTests_artefacts/Release/InterferenceGuiTests.exe`. Machine: 4 GB RAM, 2 cores. Only the main
session builds; helper agents edit disjoint files.

## Remaining
- [ ] reviewer (in progress): crash hook rewrite (CrashHookWin.cpp), diagnostics/overlay/processor minor fixes, test
      data-folder isolation (IFX_DATA_ROOT)
- [ ] auditor (in progress): effective-value / "Limited by" UI, guided "move toward worn area", stage meters, order
      toggle + excitation badge, doc contradictions X1-X10, preset X9, Environment controls for the 10 new params,
      Manual/Help sections
- [ ] researcher (in progress): Tests/TestLimits.cpp + Tests/TestScenarios.cpp (missing tests from the audit)
- [ ] Integrate agent work: add new files to CMake, full build, fix errors/warnings, run all tests
- [ ] Factory presets showcasing ground loop / dimmer / phone
- [ ] Normalize line endings (.gitattributes `* text=auto`, `git add --renormalize .`) once agents are done
- [ ] Re-run --slow (CPU + 1 h soak) on the final build; pluginval strictness 10; reinstall VST3 to Common Files\VST3
- [ ] tools/make_release.ps1 -> dist ZIP + sha256; Docs/QA.md results; PROGRESS.md; CHANGELOG
- [ ] Ask user to load in FL Studio (DAW gate) — cannot be done unattended

## Done
- [x] DSP fixes from review/audit (commit 9bbe9e8): smoothing, declicks, linked ceiling, SR-independent Studio timing,
      feedback DC blocker, kill clears once, no pop without contact change, stage peaks, excitation-cap telemetry
- [x] Environment research features (commit 3c9d96a): coupling-aware hum, ground loop + interface, dimmer, 2G phone
- [x] CPU: worst case 12 % of one i5-2520M core at 48 kHz, 39 % at 192 kHz (under ScopedNoDenormals)
- [x] VST3 installed to C:\Program Files\Common Files\VST3 (FL Studio scans it) — reinstall after final build
- [x] WP13: debug window, crash log, troubleshooting export, hard reset, hidden Ghost Carrier + secret pixel
- [x] WP12 docs: README, Research, Routing, MidiMap, Install, Issues, Decisions mirror, release script
- [x] Warnings cleanup, LICENSE, THIRD_PARTY, CHANGELOG, Privacy; pluginval strictness 5 pass (session 3)
- [x] WP1–WP11: core, sources, engine, Studio, gestures, plugin, UI, render, presets, help, tests
