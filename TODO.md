# TODO â€” Interference (autonomous run)

Specs: INTERFERENCE_VST_CLAUDE_CODE_MASTER_PROMPT.md (master), INTERFERENCE_VST_REALISM_CONSTRAINTS.md,
INTERFERENCE_VST_EFFECTS_CHAIN_AND_ROUTING.md, INTERFERENCE_VST_INDUSTRIAL_PERFORMANCE.md,
include.md, theme.md, JUCE_CLAUDE_GUIDELINES.md. Decisions: DECISIONS.md. History: PROGRESS.md.

Build: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` then
`cmake --build build --config Release --target InterferenceTests` and run
`build/InterferenceTests_artefacts/Release/InterferenceTests.exe`.

## Remaining
- [ ] WP6 Plugin: APVTS layout from `Core/Params.h` (stable IDs), state (seed, lock, lanes, slot types, version, midi map),
      migration, bus layouts incl. optional sidechain, MIDI -> ControlEvent translation, MIDI learn, A/B, reset,
      randomize, tail/latency (0) reporting. Files expected by CMake: Source/Plugin/{PluginProcessor,PluginEditor,StateIO,FactoryPresets}.cpp
- [ ] WP7 Render/Record UI (engine side done/tested: OfflineRenderer + LiveRecorder): Source/Render/OfflineRenderer.cpp (cloned engine, bars/seconds, 16/24/32f, 44.1/48/96k,
      mono/stereo, tail, normalize), live record FIFO + worker, overwrite confirm, silent-result warning, success popup
- [ ] WP8 Presets UI (bank + StateIO done/tested): factory bank (Repair Faults, Rhythmic, One-Shots, Subtle Texture, Wild + 4 Machine Pattern),
      save/save as/open, modified marker, Natural/Expressive metadata
- [ ] WP9 UI (files listed in CMakeLists IFX_UI_SOURCES): theme.md, header (LED, notch, menus, preset, A/B, gear),
      mode selector, slot cards + replace-A/B affordance, 4 functional illustrations, context controls, Details drawer,
      Why-is-it-quiet, lane editor, studio chain, output strip, meters, routing view, tooltips, right-click menu, keyboard nav, data stream
- [ ] WP10 Help overlay, Options page, custom icon (Resources/icon.png is referenced by CMake but does not exist yet)
- [ ] WP11 Tests still to write: Tests/TestGui.cpp (TestState/TestRender done); run --slow too
- [ ] WP12 Docs: README, Docs/Research.md, Manual, Install/Uninstall, Changelog, LICENSE, THIRD_PARTY, QA report,
      privacy, MIDI map, routing diagram; release zip script; pluginval run
- [ ] WP13 Extras (after all else): debug window, crash log, troubleshooting export, hard reset, easter egg

## In progress
- WP6 plugin processor/editor.

## Done
- [x] Compiled; all offline tests pass (120 tests, fast set) — 2026-09-23 session 2
- [x] StateIO (state/preset serialisation, migration, repair), FactoryPresets (22 presets, 6 categories),
      OfflineRenderer + LiveRecorder, with TestState/TestRender
- [x] WP1 Core: Random.h, PhysicalLimits.h, WearMap.h, ContactModel.h, MotionDriver.h, Environment.h, Gesture.h,
      Params.h (full parameter table), Pattern.h (lane data, seqlock exchange, sample-index scheduler)
- [x] WP2 Sources: LooseJack, PlugTouch, CableHandling, DirtyPot (R / C0 / C paths), ChargeCircuit
- [x] WP3 Engine.h: input conditioning, excitation, two-slot chain, buses, taps, mix, ceiling, failsafe, clocks, MIDI, sidechain
- [x] WP4 StudioChain.h: EQ, Contour, Drive (ADAA), Crush, Resonator, Gate, Echo, bounded local feedback, kills
- [x] WP5 gestures + scheduler (in Core/Engine)
- [x] Tests written: TestCore, TestSources, TestEngine, TestStudio, TestPattern (+ TestHelpers, framework)
- [x] CMakeLists.txt (JUCE 8.0.8 lookup: ../JUCE_8_0_8)
