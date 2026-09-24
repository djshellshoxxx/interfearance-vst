# TODO — Interference (autonomous run)

Specs: INTERFERENCE_VST_CLAUDE_CODE_MASTER_PROMPT.md (master), INTERFERENCE_VST_REALISM_CONSTRAINTS.md,
INTERFERENCE_VST_EFFECTS_CHAIN_AND_ROUTING.md, INTERFERENCE_VST_INDUSTRIAL_PERFORMANCE.md,
include.md, theme.md, JUCE_CLAUDE_GUIDELINES.md. Decisions: DECISIONS.md. History: PROGRESS.md.
Agent reports: Docs/research/ (SpecAudit, CodeReview 1-3, ReleaseReview, ElectricalInterference).

Build: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` then `cmake --build build --config Release -- -v:m`.
Tests: `build/InterferenceTests_artefacts/Release/InterferenceTests.exe [--slow]` and
`build/InterferenceGuiTests_artefacts/Release/InterferenceGuiTests.exe`. Release: `tools/make_release.ps1`.

## Remaining
(none — every spec item is implemented or recorded as a documented decision / external release gate)

External release gates that need a person, a DAW session or the publisher (not build tasks; see Docs/QA.md
"Unverified release gates"): DAW load in FL Studio, real-time host soak, sidechain in a host, empirical calibration,
real homepage / support e-mail, JUCE commercial + Steinberg VST3 licences, code-signing certificate, ASAN run,
other platforms.

## Done (this session)
- [x] Final build: all targets, 0 errors, 0 warnings; 171 DSP + 17 GUI tests pass; --slow CPU + 1 h soak pass
- [x] pluginval strictness 10: pass (25 groups); release ZIP built twice, identical SHA-256 (Docs/QA.md)
- [x] Final VST3 installed to C:\Program Files\Common Files\VST3 (FL Studio 2025 scans it), byte-identical
- [x] Release review fixes: licence texts in ZIP, HarfBuzz/SheenBidi/IJG notices, FLAC/Ogg off, options file split,
      signing on tags only, reproducible ordinal ZIP, SIGNATURE.txt
- [x] Spec audit (d) items: focus order, Studio dormancy, right-click menus, EQ smoothing, realism rows at other rates,
      tap x layout test, linked pops, project reopen, Why-quiet/dormant and accessibility tests, CI warnings-as-errors
- [x] UI review fixes: scrolling Details drawer, readable Environment tab, ramp aborts on user input, Create row
- [x] Environment research features: coupling-aware hum, ground loop + interface, dimmer, 2G phone, RF transmitters
      (square law), triboelectric handling, impedance-aware shield pickup; 5 environment presets; Environment tab
- [x] DSP review/audit fixes: smoothing, declicks, linked ceiling, SR-independent timing, kill clears, no pop without
      a contact change, policy glide, pot switch crossfades, Expressive bandwidth after re-prepare
- [x] WP13: debug window, crash log (async-safe chained hook), troubleshooting export, hard reset, Ghost Carrier
- [x] Docs: README, Manual, SoundDesign (13 recipes), Research, Routing, MidiMap, Install, Issues, QA, Decisions,
      Privacy, Parameters (151), CHANGELOG; CI workflow; line endings normalised
- [x] Earlier sessions: WP1–WP11 (core, sources, engine, Studio, gestures, plugin, UI, render, presets, help, tests)
