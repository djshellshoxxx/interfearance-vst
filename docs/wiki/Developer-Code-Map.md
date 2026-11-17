# Developer Code Map

Paths are relative to the repo root. Line numbers are approximate and drift, so search for the symbol if one is off.

## Directory tree

```
interfearance-vst/
├── CMakeLists.txt             build: JUCE fetch, plugin target, tests, GUI tests, brand defines
├── README.md                  product overview and build summary
├── Docs/                      authoritative docs (Manual, Parameters (generated), MidiMap, Routing, Install, QA, Issues, Decisions, Research, SoundDesign, Privacy)
│   └── research/              research and review reports (electrical interference, spec audit, code reviews)
├── DECISIONS.md, PROGRESS.md, TODO.md, CHANGELOG.md, JUCE_CLAUDE_GUIDELINES.md, INTERFERENCE_VST_*.md   spec and history
├── Source/
│   ├── Core/                  sample-independent models and parameter table
│   │   ├── Params.h           enum P (151 params), ParamInfo, ParamSnapshot, X-macro table
│   │   ├── Environment.h      Environment class: mains, buzz, ground loop, RF, 2G phone
│   │   ├── ContactModel.h     contact state for plugs and jacks
│   │   ├── WearMap.h          seeded worn arcs and weak spots
│   │   ├── MotionDriver.h     motion model that drives the physical sources
│   │   ├── Gesture.h          GestureGenerator and gesture kinds
│   │   ├── Pattern.h          lane pattern: steps, ratchets, probability
│   │   ├── PhysicalLimits.h   Natural caps and physical limits
│   │   └── Random.h           seeded RNG
│   ├── DSP/
│   │   ├── Source.h           SourceType enum, names and descriptions
│   │   ├── Engine.h           class Engine: slots, buses, taps, mix, process(), resets
│   │   ├── LooseJack.h        Loose Jack source
│   │   ├── PlugTouch.h        Exposed Plug Touch source
│   │   ├── CableHandling.h    Cable Handling source
│   │   ├── DirtyPot.h         Dirty Pot source
│   │   ├── StudioChain.h      Studio chain: EQ, Contour, Drive, Crush, Resonator, Rhythm Gate, Echo, feedback
│   │   ├── Filters.h          filter building blocks
│   │   ├── ChargeCircuit.h    charge circuit model (feeds the Pop behaviour)
│   │   └── Telemetry.h        EngineTelemetry (meters and state for the UI)
│   ├── Plugin/
│   │   ├── PluginProcessor.h/.cpp  InterferenceProcessor: processBlock, parameters, presets, events, render entry
│   │   ├── PluginEditor.h/.cpp     editor shell
│   │   ├── StateIO.h/.cpp          PluginState, save and load, migrations, safe repair
│   │   ├── FactoryPresets.h/.cpp   27 factory presets in 7 categories
│   │   ├── Diagnostics.h/.cpp      troubleshooting and diagnostics support
│   │   ├── CrashHook.h, CrashHookWin.cpp  crash capture (Windows)
│   ├── Render/
│   │   ├── OfflineRenderer.h/.cpp  OfflineRenderer namespace (render into memory with a cloned engine) and live Record
│   ├── UI/
│   │   ├── MainView.h/.cpp         main window layout
│   │   ├── SlotPanel.h/.cpp        slot cards A and B, illustrations, Details
│   │   ├── StudioPanel.h/.cpp      Studio tab
│   │   ├── LaneEditor.h/.cpp       Gesture Lane editor
│   │   ├── Overlays.h/.cpp         overlays and pop-up dialogs (help, options, render, debug and others)
│   │   ├── HelpContent.h/.cpp      help text
│   │   ├── Illustrations.h/.cpp    interactive slot drawings
│   │   ├── Theme.h/.cpp            colours, fonts, sizes
│   │   └── Widgets.h/.cpp          knobs, buttons, meters, labels
├── Tests/
│   ├── TestFramework.h, TestHelpers.h   tiny test harness and helpers
│   ├── TestMain.cpp                     entry for InterferenceTests (--slow option)
│   ├── TestCore.cpp, TestEngine.cpp, TestEnvironment.cpp, TestPattern.cpp, TestLimits.cpp, TestSources.cpp, TestStudio.cpp, TestState.cpp, TestRender.cpp, TestRf.cpp, TestScenarios.cpp, TestPerf.cpp
│   └── TestGui.cpp                      GUI and host contract tests (InterferenceGuiTests)
├── tools/
│   ├── gen_param_doc.py       generates Docs/Parameters.md from Source/Core/Params.h
│   ├── make_icon.py           icon generator
│   └── make_release.ps1       release ZIP, checksum and signing
├── shared/scripts/            shared Circuit Drift Labs tooling (not used by this repo's CI)
├── docs/standards/CDL_PLUGIN_BASELINE.md   shared plug-in standard
├── .github/workflows/ci.yml   checks, build, release, slow jobs
└── .github/agents/feature-auditor.agent.md   audit agent prompt
```

## Key symbols

| Symbol (file:approx line) | Purpose |
|---|---|
| `enum P` (`Source/Core/Params.h:180`) | parameter index table. Never renumber or rename a shipped ID, append new ones |
| `struct ParamInfo` (`Params.h:188`), `struct ParamSnapshot` (`Params.h:214`) | parameter metadata and per-block snapshot |
| `enum class SourceType` (`Source/DSP/Source.h:17`) | None, LooseJack, PlugTouch, CableHandling, DirtyPot |
| `class Engine` (`Source/DSP/Engine.h:50`) | slot wiring and processing. `prepare()` (line 64), `process()` (line 137), `reseedAll()`, `fullReset()`, `softReset()` |
| `Engine::handleLaneEvent()`, `handleControlEvent()`, `triggerSlot()` | lane, UI and host events into the engine |
| `Engine::source()` | returns a slot's source object |
| `class Environment` (`Source/Core/Environment.h:42`) | rig surroundings, shared phase |
| `class InterferenceProcessor` (`Source/Plugin/PluginProcessor.h:21`) | the plug-in. `processBlock()` (`PluginProcessor.cpp:260`), `pushControlEvent()` (`:344`), `setSlots()`, `setSeed()`, `newTake()`, `setPattern()`, MIDI learn (`startMidiLearn`, `clearMidiMapping`), `applyState()` |
| `setStateInformation()` / `getStateInformation()` | host state; `StateIO` does the work |
| `namespace OfflineRenderer` (`Source/Render/OfflineRenderer.h:48`) | `RenderSettings` (line 13), `RenderResult` (line 34), render function |
| Record class (`Source/Render/OfflineRenderer.h:72`) | lock-free capture to WAV on a background thread |
| `FactoryPresets.cpp` | the 27 presets |
| `Source/Plugin/Diagnostics.cpp` | troubleshooting report and log files (see also the Debug dialog in Help) |
| colour and font tokens | `Source/UI/Theme.h/.cpp` |

## Where do I find X / to change X, edit Y

| I want to... | Edit |
|---|---|
| add a parameter | `Source/Core/Params.h` (append an entry to the X table and a new `enum P` value), then `python tools/gen_param_doc.py` to refresh `Docs/Parameters.md`, then the UI and any DSP that reads it |
| change how a source fails | `Source/DSP/LooseJack.h`, `PlugTouch.h`, `CableHandling.h`, `DirtyPot.h`, and their state in `Source/Core/ContactModel.h` / `WearMap.h` |
| change the hum, buzz, ground loop, RF or 2G models | `Source/Core/Environment.h` (research basis in `Docs/research/ElectricalInterference.md`) |
| change the Studio chain | `Source/DSP/StudioChain.h` (order is EQ, Contour, Drive, Crush, Resonator, Rhythm Gate, Echo) |
| change the gesture lane timing | `Source/Core/Pattern.h`, `Gesture.h`, `LaneEditor.cpp` for the UI |
| change Natural and Expressive limits | `Source/Core/PhysicalLimits.h` |
| change how state is saved or migrated | `Source/Plugin/StateIO.cpp` and `StateIO.h`. Add a version step and a test in `Tests/TestState.cpp` |
| add a factory preset | `Source/Plugin/FactoryPresets.cpp` |
| change MIDI behaviour | `PluginProcessor.cpp` (MIDI and learn) and `Docs/MidiMap.md` |
| change render or record | `Source/Render/OfflineRenderer.cpp` |
| change the help text | `Source/UI/HelpContent.cpp` |
| change a panel layout | `Source/UI/MainView.cpp`, `SlotPanel.cpp`, `StudioPanel.cpp`, `Overlays.cpp` |
| change colours, fonts or sizes | `Source/UI/Theme.cpp` |
| change release packaging | `tools/make_release.ps1`, `.github/workflows/ci.yml` |
| change the product name or IDs | `CMakeLists.txt` (`PRODUCT_NAME`, `PLUGIN_CODE Ifx1`, `PLUGIN_MANUFACTURER_CODE Ifrn`) |

## Gotchas for navigation

- `DSP` should never include `UI`. The README says the DSP does not depend on the GUI.
- The parameter table is the source of truth. `Docs/Parameters.md` is generated, so edit `Params.h` and rerun the script.
- `CMakeLists.txt` lists sources in `IFX_SHARED_SOURCES`, `IFX_UI_SOURCES` and `IFX_MODULES`. A new file must go in one of those lists.
