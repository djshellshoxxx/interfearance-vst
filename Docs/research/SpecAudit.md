# Spec compliance audit — Interference 1.0.0

Audit date: 2026-09-23. Scope: every concrete, checkable requirement in `INTERFERENCE_VST_CLAUDE_CODE_MASTER_PROMPT.md`,
`INTERFERENCE_VST_REALISM_CONSTRAINTS.md`, `INTERFERENCE_VST_EFFECTS_CHAIN_AND_ROUTING.md`,
`INTERFERENCE_VST_INDUSTRIAL_PERFORMANCE.md`, `include.md`, `theme.md` and `JUCE_CLAUDE_GUIDELINES.md`, checked against
`Source/`, `Tests/`, `Docs/`, `README.md`, `DECISIONS.md` and `TODO.md` at commit `ab0d807` (plus the untracked,
in-progress `Docs/QA.md`).

Method: static reading only. Nothing was compiled or run (a build was in progress and RAM is limited), so
"Implemented" means the code and, where named, a test exist. Whether those tests pass is not known yet.
`INTERFERENCE_VST_CREATIVE_FEATURES.md` is missing. That is a recorded decision (DECISIONS.md): its features are
built from the routing spec.

Status key: **Implemented** · **Partial** · **Missing** · **Deviation** (a recorded decision or interpretation) ·
**Not verifiable** (needs a DAW, a listening test or a run) · **N/A**. File references are `path:line`. Line
numbers are for `ab0d807`.

---

## 1. Master prompt (`INTERFERENCE_VST_CLAUDE_CODE_MASTER_PROMPT.md`)

### Preamble, §1 product and completion contract

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| M0.1 | VST3 on Windows x64 first, with a portable DSP core | Implemented | `CMakeLists.txt` FORMATS VST3 Standalone (AU on Apple); `Source/Core`, `Source/DSP` include only the standard library | |
| M0.2 | No paid SDKs, internet services or extra runtimes | Implemented | static MSVC runtime, `JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0` (CMakeLists.txt) | |
| M0.3 | JUCE version and toolchain pinned and documented | Implemented | CMakeLists.txt JUCE 8.0.8 lookup; DECISIONS.md line 1 | |
| M0.4 | Do not claim builds or DAW tests that were not done | Implemented | Docs/Issues.md O1–O3; Docs/QA.md "It is not claimed" | |
| M1.1 | Four sources: Loose Jack, Exposed Plug Touch, Cable Handling, Dirty Pot | Implemented | `Source/DSP/LooseJack.h`, `PlugTouch.h`, `CableHandling.h`, `DirtyPot.h` | |
| M1.2 | One source or any two at once; no three-source state possible | Implemented | `Engine.h:96-102` rejects duplicates; only two slots exist; `PluginProcessor.cpp:355-375` | |
| M1.3 | UI makes the two-source limit obvious | Implemented | `SlotPanel.cpp:193-243` (Replace A/B menu, "2 of 2 slots used") | |
| M1.4 | Bypassed modules keep their settings | Implemented | settings are stored per source type (DECISIONS.md); `slotA_on`/`slotB_on` parameters | |
| M1.5a | Deliverable: working VST3 | Not verifiable | build in progress (TODO.md "Remaining") | |
| M1.5b | Deliverable: offline DSP tests | Implemented | `Tests/Test*.cpp`, 140+ tests | |
| M1.5c | Deliverable: host/plugin validation | Partial | pluginval strictness 5 passed (TODO.md Done); strictness 10 pending; DAW load not possible (Issues O1) | Run pluginval 10 and record the result in QA.md |
| M1.5d | Deliverable: in-plugin help and tooltips | Implemented | `HelpContent.cpp`, `paramTooltip()` in `Widgets.cpp:7-16` | |
| M1.5e | Deliverable: factory presets | Implemented | 22 presets in `FactoryPresets.cpp` | |
| M1.5f | Deliverable: install, uninstall and update paths | Implemented | Docs/Install.md | |
| M1.5g | Deliverable: licence and third-party file | Implemented | LICENSE, THIRD_PARTY.md | |
| M1.5h | Deliverable: QA evidence | Partial | `Docs/QA.md` exists but is untracked and in progress: it has reproduction commands only, with no results, CPU numbers or unverified-gates section yet. README, CHANGELOG release notes, Issues.md and Manual all point to it | Finish QA.md: per-suite results, pluginval output, CPU and latency figures, unverified gates |
| M1.5i | Deliverable: reproducible release archive | Partial | `tools/make_release.ps1` (deterministic ZIP plus SHA-256); not run yet (TODO.md) | Run it and record the checksum in QA.md |
| M1.5j | Deliverable: concise user manual | Implemented | Docs/Manual.md | |
| M1.6 | Stand-alone only if it does not compromise VST3 | Implemented | FORMATS includes Standalone | |
| M1.7 | Module split: Core, DSP, Plugin, UI, Render, Tests, Docs | Implemented | `Source/*` folders | |
| M1.8 | DSP independent of the GUI | Implemented | Core/DSP headers have no JUCE includes | |
| M1.9 | No allocation, file IO, locks, logging or blocking in the audio callback | Implemented | `PluginProcessor.cpp:240-321`, `Engine::process`; telemetry is atomic; LiveRecorder uses a lock-free FIFO. Bounded insertion sorts do run on the audio thread (see I5.3) | |
| M1.10 | Bounded buffers and event budgets | Implemented | `Engine::kMaxEvents=512`, processor `kMaxEvents=1024`, 256-entry UI FIFO | |

### §2 Scientific basis

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| M2.1 | Cite the seven papers in Docs/Research.md; say when only the abstract was available | Implemented | Docs/Research.md "What was available" table | |
| M2.2 | Label observations, inferences and artistic choices | Implemented | Research.md uses Observed / Inference / Artistic labels | |
| M2.3 | Numeric defaults documented as tunable, not measured | Implemented | `PhysicalLimits.h` status column "tunable, unmeasured"; Research.md | |

### §3 Operating model

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| M3.1 | Hardware, Play and Create modes | Implemented | `mode` parameter; `MainView.cpp:86-100, 453-461` (mode selects the bottom tab; the DSP is shared) | |
| M3.2 | Hardware: restrained, asynchronous, no forced tempo | Implemented | lane off by default (`lane_on` default 0) | |
| M3.3 | Play: MIDI, GUI, automation and the lane disturb the hardware | Implemented | `Engine::handleControlEvent`, `handleLaneEvent` | |
| M3.4 | Create: self-excitation, audition, bars or seconds, render WAV | Implemented | `exc_type`, `CreatePanel`, `RenderOverlay`, `OfflineRenderer` | |
| M3.5 | Action labels: Move Plug, Touch, Shake Cable, Turn Knob, Wear, Hum, Pop, Density, Output | Implemented | `Params.h:49,60,68,75,52,38,57,88,158` | |
| M3.6 | Advanced controls in an expandable Details panel | Implemented | `SlotPanel.cpp:257-320` (Details drawer) | |

### §4 Unified physical and state model

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| M4.1 | Host Fs; double-precision timeline; sample-accurate transitions | Implemented | `Engine.h:335-606` loops per sample; events carry sample offsets | |
| M4.2 | Parameter smoothing kept separate from fast contact switching | Partial | positions are smoothed by `MotionDriver`; Input Trim and Excitation are smoothed (`Engine.h:401-402`). **Dry/Wet, Dry Trim, Output, Studio Level, Ceiling (`Engine.h:258-269`), Hum/Buzz/RF (`Engine.h:234-236`), Drive pre/post, stage wets and EQ coefficients (`StudioChain.h:102-202`) change in a step at each block boundary**: zipper noise under automation | Add one-pole or `SmoothedValue` ramps (~10–20 ms) for these gains in `Engine::process` and `StudioChain::process` (~40 lines) |
| M4.3 | Reset on sample-rate change and explicit reset; none on ordinary block changes | Implemented | `Engine::prepare`, `requestReset`; `Engine.blockSizeDoesNotChangeOutput` | |
| M4.4 | Shared MotionDriver gives q, v, a plus bounded correlated micro-motion | Implemented | `MotionDriver.h` | |
| M4.5 | Same gesture clock; separate parameters, state and random streams per source | Implemented | `Engine::reseedAll` hash(seed, type, bar) | |
| M4.6 | Source-specific gesture depth; explicit option to couple motion | Implemented | `slotA_level`/`slotB_level`, `link` | |
| M4.7 | Inline transform plus a separate injected bus; user-swappable physical order | Implemented | `Source::process(paths[3])`, `Engine.h:447-465` | |
| M4.8 | A bypassed module is an exact identity | Implemented | `Engine.bothSlotsBypassedIsExactIdentity`, `oneSlotBypassedIsIdentityForThatSlot` | |
| M4.9 | Wet-only and Noise-only monitoring | Implemented | Monitor taps Full Wet and Injected Artifacts (`Engine.h:545`) | |
| M4.10 | No excitation plus an open circuit gives no strong continuous output | Implemented | `Engine.openCircuitDoesNotConjureContinuousOutput` | |
| M4.11 | True bypass with consistent latency | Implemented | `Engine.h:555-563`; latency 0 | |
| M4.12 | Document that series pairs interact but are not one exact device | Implemented | Manual "Two slots"; Help | |
| M4.13 | Health h, wear map W(q), conductance g(t); semi-Markov GOOD→RESISTIVE→OPEN→RECOVER; hysteresis; hard rate caps | Implemented | `ContactModel.h`; `Core.contactEventRateIsCapped` | |
| M4.14 | Deterministic seeded event proposals; correlated roughness, not per-sample white noise | Implemented | `ContactModel`, `WearMap::valueNoise` | |
| M4.15 | Bandlimited contact transitions; sharp transients only from the pop model | Implemented | `ContactModel.h` two-pole edge; `Core.contactTransitionsAreBandlimited` | |
| M4.16 | Wear, Pressure, Position, Motion, Intermittency and Recovery each have a distinct role | Implemented | `LooseJack::control` | |
| M4.17 | Energy and event counts clamped | Implemented | `PhysicalLimits`, `kSafetyClamp` | |
| M4.18 | Mains 50/60 Hz, limited harmonics, phase-continuous; Hum sets it, Shield attenuates it | Implemented | `Environment.h`; `Core.environmentPhaseIsContinuousAndSeekable` | |
| M4.19 | Buzz and RF bands separately controlled, off by default | Implemented | `buzz`, `rf` parameters default 0 | |
| M4.20 | Channels share contact state; small electrical stereo difference | Implemented | `Source::envWave`, `stereo` parameter | |
| M4.21 | Silence, healthy cable and hum off give silence | Implemented | `Engine.silenceInvariantEverySourcePairModeAndMix` | |
| M4.22 | Bias/Charge relaxes and gives a bounded pop at a contact event; no canned pop on every event | Partial | `ChargeCircuit.h:23-35` high-passes `g*bias`, so **a change of the bias itself (Pop or Circuit Bias knob, their automation, or a Natural↔Expressive switch that changes `popPeak`) makes a pop on a closed, still contact** (`LooseJack.h:112`, `PlugTouch.h:95`, `DirtyPot.h:138`) | Make the pop depend on contact changes only: `y = smooth(HP(g)) * biasSmoothed`, with the bias ramped over ~50 ms. Add a test: step Pop and policy on a seated jack and expect no transient (~15 lines + test) |
| M4.23 | Pop documented as the modelled circuit contribution, separate from interruption clicks | Implemented | Pop tooltips; Help "Loose Jack" | |
| M4.24 | Buffered/Line vs Passive Pickup RLC; Cable Load only relevant in Passive mode | Implemented | `Engine::updatePassive`; Cable Load is greyed with a reason (`SlotPanel.cpp:335`) | |
| M4.25 | Optional saturation: antialiased, denormal-safe, DC blocking, latency reported | Implemented | ADAA `preSat` on all three paths (`Engine.h:467-475`); latency 0 | |

### §5 Source-specific DSP

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| M5.1 | Loose Jack: one TS model; Insert, Wiggle, Pressure, Wear | Implemented | `LooseJack.h` | |
| M5.2 | A partial or damaged contact attenuates, goes resistive or open, and is more exposed to hum | Implemented | `LooseJack::control` exposure term | |
| M5.3 | Contact changes gate the real signal and inject interference | Implemented | `LooseJack::process` | |
| M5.4 | Distinct short/ground quieting mode | Implemented | `jk_short`; `LooseJack.shortToSleeveIsQuieterThanOpen` | |
| M5.5 | No spontaneous high-rate failure at a healthy still position unless Intermittency is raised | Implemented | hazard is 0 at e=1 with W≈0; `LooseJack.seatedHealthyIsNearTransparent` | |
| M5.6 | Plug drag axial → Insert, lateral → Wiggle; MIDI triggers a time-bounded wiggle | Implemented | `Illustrations.cpp:297-313`; gestures capped at 8 s and released by note-off | |
| M5.7 | Touch: Touch, Pressure, Mains, Body, Shield, Circuit Exposure | Implemented | `PlugTouch.h` | |
| M5.8 | Touch onset/offset chatter and charge click | Implemented | `PlugTouch.pressAndReleaseChatterAroundTransitions` | |
| M5.9 | Touch is described as an artistic simplified model | Implemented | `PlugTouch.h` doc comment, Help, Manual | |
| M5.10 | Mouse press/drag or MIDI pressure → touch; release returns to baseline | Implemented | `Illustrations.cpp:387-410`; aftertouch → `ccPressure` | |
| M5.11 | Cable: Electrical and Handling sublayers; Bend, Shake, Damage, Mic | Implemented | `CableHandling.h` | |
| M5.12 | Handling sound from gesture-linked friction bursts and resonant taps, never uniform white noise | Implemented | stick-slip impulses into three resonators | |
| M5.13 | A sound cable with Mic 0 and no damage is quiet | Implemented | `CableHandling.soundCableWithoutMicIsSilentAndTransparent` | |
| M5.14 | Localized fault at a reproducible bend position | Implemented | seeded weak-spot map; `bendOverWeakSpotFaultsMoreThanElsewhere` | |
| M5.15 | Pot: Turn, Taper (lin/log), a documented Tone topology | Implemented | `DirtyPot.h` header comment (Rs, R, C transfer function) | |
| M5.16 | Stopping stops the scratch; a severely damaged pot may stay intermittent | Implemented | `heldStillStopsScratch`, `severeDirtUnderVibrationMayStayIntermittent` | |
| M5.17 | Dirt, Wear Position, Scratch, Circuit; transfer follows the wiper at zero Dirt; scratch depends on input and bias | Implemented | `cleanPotFollowsTaperWithoutScratch`, `silentInputZeroBiasIsSilentBiasIsNot` | |
| M5.18 | Smooth automated movement; a fixed worn region reproduces bursts | Implemented | `MotionDriver`; seed-only wear map | |

### §6 Two-source behaviour

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| M6.1 | Slots A/B take any two different types; a source can move between slots; no duplicates | Implemented | `PluginProcessor::setSlotSource`; `Host.slotRulesNeverDuplicate` | |
| M6.2 | A third source opens a clear Replace A / Replace B choice and never drops a source silently | Implemented | `SlotPanel.cpp:193-220` | |
| M6.3 | Per slot: enable, disturbance, gesture, seed substream, settings | Implemented | Params.h lines 20-29 | |
| M6.4 | Shared mains phase and signal path | Implemented | single `Environment` | |
| M6.5 | The UI has **Swap Order** | Partial | exists only as a toggle in the Details drawer (`SlotPanel.cpp:319`); the main view never shows the processing order (the A and B cards are fixed) | Add an "A → B / B → A" order button between the slot cards in `MainView` and show the order on the cards (~30 lines) |
| M6.6 | Compact comparison audition | Partial | header A/B settings compare exists; no per-slot audition | Add a momentary "Solo" on each card that temporarily bypasses the other slot (engine already ramps slot enable), or document A/B as the audition |
| M6.7 | Linked Gesture, off by default | Implemented | `link` default 0 | |
| M6.8 | Per-source bypass keeps settings, state capture and MIDI assignments; documented clock policy | Implemented | Help "Two slots"; `Engine.bypassedSlotAdvancesTimeSilently` | |
| M6.9 | Test all six pairs in both orders, each alone, both bypassed | Partial | `Engine.pairMatrixBothOrdersFiniteAndDistinct` checks only finite/bounded output. Despite its name it never asserts that the orders differ. `swapOrderIsAudible` covers one pair | Assert per pair that swapping changes the output or event telemetry (~15 lines) |

### §7 Sound generation, timing and repeatability

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| M7.1 | Excitation: Off, bounded noise, filtered buzz, DC/bias impulse, test tone | Implemented | `Engine::excitation` | |
| M7.2 | Artifacts-only output in Create mode | Implemented | Monitor "Injected Artifacts" | |
| M7.3 | Excitation goes through the same chain and is labelled a studio source | Implemented | `Engine.h:403-411`; Create panel text | |
| M7.4 | Gesture inputs: drag, momentary, latched, MIDI note/velocity, CC/aftertouch, automation, lane | Implemented | processor MIDI translation; Trigger A/B parameters; illustrations | |
| M7.5 | Lane lengths 1–16; divisions 1/4 to 1/32 including triplets | Implemented | 1–64 steps; 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T (no 1/4T) | |
| M7.6 | Gesture presets Wiggle, Slow Turn, Tap, Shake, Touch and Release | Implemented | `Gesture.h:13-21` | |
| M7.7 | The scheduler quantizes gesture starts only | Implemented | `scheduleLane` | |
| M7.8 | Humanize bounded and deterministic | Implemented | `Pattern.h` hash-based offset | |
| M7.9 | Chaos varies only within physical limits | Implemented | `GestureGenerator::trigger` | |
| M7.10 | Lane optional, with a visible Off state | Implemented | Lane On toggle | |
| M7.11 | Portable PRNG, stable hashing, no `random_device` | Implemented | `Random.h` | |
| M7.12 | User-visible Seed, Lock, New Take | Implemented | `MainView.cpp:104-121` | |
| M7.13 | Seed, map derivation, lane and every parameter saved in state | Implemented | `StateIO::toValueTree` | |
| M7.14 | Host-time placement; defined fallback when there is no transport | Implemented | internal clock with Run (DECISIONS) | |
| M7.15 | Same result for repeated playback, block sizes, bounce and loop | Implemented | `blockSizeDoesNotChangeOutput`, `lockedSeedBounceIsNullIdentical`, `loopJumpRepeatsHardwareEvents` | |
| M7.16 | DAW bounce is deterministic by default | Implemented | `seedLock` default true | |
| M7.17 | Explicit policy and tests for seek, loop, stop/start, sample-rate switch and offline render | Partial | policy in DECISIONS; tests cover loop-back, block size and render. **No test** for forward seek, stop→start, or a sample-rate switch | Add three Engine tests (~40 lines) |
| M7.18 | Document the external automation resolution limit honestly | Partial | Help ("the engine smooths it") and MidiMap.md ("Continuous parameters are smoothed") overstate: see M4.2 | Fix M4.2, or correct both documents |
| M7.19 | Record into a preallocated bounded FIFO drained by a worker | Implemented | `LiveRecorder` (ThreadedWriter) | |
| M7.20 | Offline Render with a cloned engine and fixed tempo | Implemented | `OfflineRenderer::render` | |
| M7.21 | 1–16 bars or 0.1–30 s; 16/24/32f; 44.1/48/96 kHz; mono/stereo; tail; normalize with headroom | Implemented | `RenderSettings::sanitise`; `RenderOverlay` | |
| M7.22 | File IO and offline processing off the audio thread | Implemented | worker `std::thread` in RenderOverlay | |
| M7.23 | No promise of DAW drag-and-drop | Implemented | not offered | |
| M7.24 | Explain a silent result before exporting | Implemented | `OfflineRenderer::explainSilence`; `Overlays.cpp:528-535` | |
| M7.25 | Predictable names; sidecar only on request and privacy-safe | Implemented | `suggestedFileName`, `writeSidecar`; `Render.sidecarOnlyOnRequestAndNoPaths` | |
| M7.26 | Never overwrite without confirmation | Implemented | `warnAboutOverwriting`; `Render.neverOverwritesWithoutConfirmation` | |

### §8 GUI and engine bindings

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| M8.1 | Single responsive window with four areas (mode selector, illustrated canvas, context controls, bottom gesture/output strip) | Implemented | `MainView::resized` | |
| M8.2 | Source cards: active state, name, one-sentence cause, activity meter, primary gesture | Implemented | `SlotCard` (`SlotPanel.cpp:64-179`) | |
| M8.3 | Slots A/B stay visible when only one is loaded | Implemented | `EmptyIllustration` | |
| M8.4 | Selecting a source changes context controls, never the audio | Implemented | `MainView::selectSlot` | |
| M8.5 | Details drawer: circuit mode, bias, stereo, advanced calibration, diagnostics | Partial | circuit, Circuit Bias, stereo, meters and diagnostics are present. "Advanced calibration" is only trims and Chaos; the engineering caps are not visible | Add a read-only view of the PhysicalLimits table in Details or Debug, or record the interpretation in DECISIONS.md |
| M8.6 | Preset browser: factory categories, custom save/load, modified marker | Implemented | `MainView::refreshPresetBox`, `updateHeader` ("*") | |
| M8.7 | Text descriptions, keyboard navigation, focus order, high-DPI, no reliance on colour alone | Partial | titles/descriptions, state captions and keyboard control exist; focus order is implicit (no `setExplicitFocusOrder`); illustrations have no tooltip | Set an explicit focus order in MainView and ContextPanel; make `Illustration` a `SettableTooltipClient` |
| M8.8 | Automated parameter attachment rather than duplicated GUI state | Implemented | APVTS attachments in `Widgets.cpp` | |
| M8.9 | Jack illustration: cutaway, engaged/partial/open/ground, axial drag, lateral chatter, signal path highlighted only in engine states | Implemented | `Illustrations.cpp:193-313` | |
| M8.10 | Touch illustration: tip, finger patch, input path, field. Field intensity follows engine coupling and is never an unrelated animation | Partial | field alpha also uses the **Hum parameter** and the rings **animate all the time** (`Illustrations.cpp:341-347`) | Drive the field only from `view.auxOpenness`/activity; freeze the rings when coupling ≈ 0 |
| M8.11 | Cable illustration: bend, core, shield, weak spot, pickup; drag the bend | Implemented | `Illustrations.cpp:420-530` | |
| M8.12 | Pot illustration: rotary knob, revealable interior, wiper, the seeded worn arc used by the DSP | Implemented | `PotIllustration`; `Illustration::wearAt` uses `WearMap::compute` | |
| M8.13 | Original scalable vector illustrations | Implemented | drawn in code | |
| M8.14 | UI reads a throttled lock-free snapshot; never calls DSP setters from paint | Implemented | `SlotTelemetry` atomics, 30 Hz timer | |
| M8.15 | Mouse edits go through parameters with begin/end, or a safe queue | Implemented | `Illustration::beginEdit/endEdit`, `pushControlEvent` | |
| M8.16 | MIDI mapping and right-click automation are discoverable | Implemented | `showParamMenu` (`Widgets.cpp:40-74`) | |
| M8.17 | Redundant labelled sliders for the illustrations | Implemented | ContextPanel knobs | |
| M8.18 | Reset Control action | Implemented | right-click "Reset to default"; double-click | |
| M8.19 | Compare and bypass | Implemented | A/B header buttons; Bypass toggle in the output strip | |
| M8.20 | Tooltips describe audible results | Implemented | Params.h tooltip column (illustrations excepted, see M8.7) | |
| M8.21 | Visual explanation when the two-source limit is reached | Implemented | SourcePalette note + replace menu | |

### §9 Acceptance workflows

| # | Workflow | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| W1 | Subtle coloration (Jack, Passive Pickup, worn spot; seating reduces faults; bypass transparent; Line reduces cable load) | Implemented | preset "Worn Guitar Jack"; `seatedHealthyIsNearTransparent`, `pluginBypassIsDry`. No test that Buffered/Line removes the cable-load response | Add a Passive-vs-Line spectral test (P3) |
| W2 | Authentic dirty knob (level follows knob, clustered scratch at the arc, still = no scratch, Tone really filters) | Implemented | DirtyPot tests | |
| W3 | Playable rhythm (Jack + Cable, beat wiggles, offbeat shakes, locked bounce null-identical) | Implemented | preset "Wiggle Every Beat"; `lockedSeedBounceIsNullIdentical` | |
| W4 | Fresh one-shot ("record a touch-and-turn gesture", Artifacts-only, 2 s WAV, New Take keeps timing) | Partial | render, monitor tap and New Take work. There is **no gesture recorder**: GUI drags change parameters, and Render replays a static parameter snapshot plus lane/triggers. A performed gesture reaches a WAV only through live Record | Add "Record gestures to lane" (quantize Play/illustration gestures into lane steps; LaneEditor ~80 lines), or document the limitation in Manual "Render and record" |
| W5 | Source order (a downstream jack interrupts pot artifacts; A/B meters and audition make it clear) | Partial | engine behaviour tested (`downstreamOpenContactInterruptsUpstreamArtifacts`); Swap is hidden and there is no audition (M6.5, M6.6) | as M6.5/M6.6 |

### §10 Plugin, state and host contract

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| H1 | Mono and stereo effect buses; no duplication or level jump | Implemented | `PluginProcessor.cpp:135-151`; `Host.busLayoutsMonoStereoOnly` | |
| H2 | Silence and zero-length blocks | Implemented | `processBlock` n≤0 guard; `Engine.zeroLengthAndOneSampleBlocks` | |
| H3 | GUI and automation cover the Play workflow when MIDI is unavailable | Implemented | Trigger A/B, lane, Fill parameters | |
| H4 | Stable, versioned parameter IDs | Implemented | `ParameterID(id, 1)`; `Host.parameterIdsAreUniqueAndStable` | |
| H5 | Physical ranges, readable units, normalized host mapping | Implemented | `formatValue`, log ranges | |
| H6 | Automation smoothing without smearing faults | Partial | see M4.2 | see M4.2 |
| H7 | Proper begin/change/end gestures | Implemented | attachments, illustrations, mode buttons. MIDI-learned CCs call `setValueNotifyingHost` without begin/end (`PluginProcessor.cpp:793`), which is minor | |
| H8 | Preset compatibility migration | Implemented | `State.olderStateMissingFieldsMigratesToDefaults` | |
| H9 | APVTS plus explicit non-parameter state (seed, maps, lane, slot order, UI mode, version) | Implemented | StateIO | |
| H10 | Older states load safely; unknown fields ignored | Implemented | `State.newerVersionLoadsWithWarning` | |
| H11 | No pointers, device names, raw audio, absolute paths or machine state in presets | Implemented | StateIO | |
| H12 | Malformed preset data recovers to bounded defaults | Implemented | `State.malformedDataIsRejected…`, `outOfRangeAndNonFiniteValuesAreRepaired` | |
| H13 | Correct latency and tail reporting | Partial | latency 0. Tail comes from the stage toggles **even when Studio is off** (`StudioChain.h:383-391` checks `on[]`, not `studio_on`), which contradicts Routing.md ("With Studio off it is 0") | Return 0 from `Engine::getTailSeconds` when Studio is off or Raw Hardware is on |
| H14 | Safe at 44.1, 48, 88.2, 96, 192 kHz with variable blocks | Implemented | `Engine.allSampleRatesStayFiniteAndBounded`, fuzz with random block sizes | |
| H15 | Sample-rate-aware filter coefficients | Implemented | all filters recomputed in prepare/setParams | |
| H16 | Smooth mode changes and bypass | Implemented | 10 ms ramps for bypass, slot, studio, raw, order | |
| H17 | No network or telemetry | Implemented | Privacy.md; no network code | |
| H18 | Bounded, finite, DC-managed, anti-denormal, stable feedback, thread-safe metering, deterministic | Implemented | `kSafetyClamp`, `ScopedNoDenormals`, `undenorm`, fuzz test | |
| H19 | Any limiter is explicit and its audible behaviour tested | Partial | Ceiling is explicit (`Engine.h:547-554`), but no test covers it | Add a Ceiling test: overs are held at the ceiling, off means identity |
| H20 | Parameters are immutable snapshots during render | Implemented | per-block `ParamSnapshot` | |

### §11 Tests and calibration

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| T1 | Test with impulse, silence, DC, sine sweeps, low/high tones, guitar and drum material, transients, mono/stereo | Partial | impulse, silence, sine, noise, synthetic drums, mono/stereo. **No DC-input test, no swept sine, no guitar-like material** | Add DC-input and log-sweep tests (TestEngine ~40 lines) |
| T2 | No NaN/Inf | Implemented | `invalidInputIsSanitised`, fuzz | |
| T3 | No uncontrolled DC | Missing | DC blockers exist but nothing tests them end to end (only Drive: `asymmetricDriveDcIsBlocked`) | Test: DC input plus bias/pop sources, check the output mean after settling |
| T4 | Low hum in a healthy shielded idle path | Implemented | `PlugTouch.shieldedTerminatedPicksUpLittle` | |
| T5 | One-source and two-source bypass identity | Implemented | Engine tests | |
| T6 | A stationary dirty pot under normal conditions makes no new scratch | Implemented | `DirtyPot.heldStillStopsScratch` | |
| T7 | Sample/block sizes and host seek keep defined repeatability | Partial | block size and loop-back tested; forward seek not | see M7.17 |
| T8 | Mains phase stays continuous | Implemented | `Core.environmentPhaseIsContinuousAndSeekable` | |
| T9 | Peak energy bounded | Implemented | fuzz, sample-rate test | |
| T10 | Pair matrix: six pairs in both orders | Partial | see M6.9 | |
| T11 | Seek, loop, tempo change | Partial | no tempo-change test | Test a mid-run BPM change: the lane stays on the ppq grid |
| T12 | MIDI note-off | Implemented | `Pattern.midiOneShotPatternAndNoteOffPolicy`, `Host.midiNotesTriggerGesturesAndCcLearn` | |
| T13 | Unloaded plugin and project reopen | Partial | `Host.stateRoundTripThroughHostBlob` checks the state only, not that the audio is identical after reopen with Lock on | Render, save the blob, create a new processor, load it, render again, compare |
| T14 | Preset migrations | Implemented | State tests | |
| T15 | Offline and real-time agreement | Missing | Render tests compare render with render only | Compare `OfflineRenderer::render` with a Harness run of the same state and transport (TestRender ~30 lines) |
| T16 | Render WAV headers, duration and format | Implemented | `Render.wavHeadersMatchEveryFormat`, `durationFollowsBarsAndSeconds` | |
| T17 | Concurrent GUI actions, resizing, teardown during export | Implemented | `Gui.concurrentAudioAndGuiActions`, `editorOpensRendersAndResizes`, `teardownWhileRenderIsRunning` | |
| T18 | Run a VST3 validator | Partial | pluginval 5 passed; 10 pending (TODO) | Run strictness 10 |
| T19 | Load in at least one real DAW | Not verifiable | no DAW on the machine; Issues O1 | Release gate (documented) |
| T20 | Calibration recordings or an honest research-informed label | Deviation | shipped research-informed; Issues O2; Research.md "Calibration status" | |
| T21 | Sanitizer debug builds | Partial | `INTERFERENCE_ASAN` option; no recorded run | Record one ASAN run in QA.md |
| T22 | Host automation stress | Partial | `Gui.concurrentAudioAndGuiActions` | |
| T23 | 1 h+ soak for leaks and CPU drift | Partial | `Perf.soakOneHourOfAudioNoDriftFiniteBounded` written (slow); not run yet | Run `--slow` and record it |
| T24 | Plugin scan | Partial | pluginval | |
| T25 | Preset compatibility | Implemented | State tests | |
| T26 | Export cancellation | Implemented | `Render.cancellationStopsEarly` | |
| T27 | Inaccessible export path | Implemented | `Render.badPathsFailCleanly` | |
| T28 | Invalid WAV length | Implemented | `RenderSettings::sanitise` clamps | |
| T29 | Low disk space | Partial | error path "Writing failed (disk full?)" exists; not testable here | Document as manually verified or untested |
| T30 | Failure recovery | Implemented | state repair, render errors | |
| T31 | UI accessibility checks | Missing | no test | GUI test: walk focus, check each focusable has a title and tooltip |
| T32 | CI builds, tests, packages, checks licence notices; never fabricates a signature | Partial | no CI config (no `.github/`); licence check exists in `make_release.ps1`; no signing | Add `.github/workflows/windows.yml` running the build, both test suites, pluginval and make_release |
| T33 | Issue list with severity, repro, fix and regression evidence | Implemented | Docs/Issues.md | |
| T34 | Release not marked green while a critical gate is unverified | Implemented | Issues O1–O3; CHANGELOG release notes | |

### §12 Documentation and release

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| D1 | Quick start for each mode | Implemented | Manual, Help | |
| D2 | Routing diagrams | Implemented | Docs/Routing.md | |
| D3 | Each source's physical basis | Implemented | Research.md, Manual | |
| D4 | Effect vs generator | Implemented | Manual "Modes" | |
| D5 | Two-slot order and linking | Implemented | Manual | |
| D6 | Seed and loop reproducibility | Implemented | Manual | |
| D7 | MIDI and automation map | Implemented | Docs/MidiMap.md | |
| D8 | Attribution of the external style spec | Implemented | THIRD_PARTY.md:21 | |
| D9 | Format/OS/CPU support matrix from tested builds | Partial | Manual "Compatibility" table points to QA.md for CPU; no figures yet | Fill CPU numbers in QA.md and the Manual |
| D10 | Known host limitations | Implemented | Manual | |
| D11 | Installation | Implemented | Install.md | |
| D12 | Presets and preset migration | Implemented | Manual, CHANGELOG | |
| D13 | Export limitations | Implemented | Manual "Render and record" | |
| D14 | Privacy statement | Implemented | Docs/Privacy.md | |
| D15 | Credits and third-party licence inventory | Implemented | THIRD_PARTY.md | |
| D16 | Accurate level guidance | Implemented | Manual "Level guidance". It calls Ceiling "an explicit, audible soft ceiling", but the code is an instant-attack gain rider plus a hard clamp (`Engine.h:547-554`) | Reword to "peak ceiling" (P3) |
| D17 | Installer only with a trustworthy pipeline, else ZIP | Deviation | DECISIONS.md "No installer" | |
| D18 | Semantic version, changelog, release notes, compatibility policy | Implemented | CHANGELOG.md | |
| D19 | Licence activation extension point, never blocking offline users | Partial | "Licence activation: none" in the troubleshooting file; no extension point described | One paragraph in Docs/Decisions.md naming where a check would go |

### §13 Execution and handoff

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| E1 | Assumptions recorded in Docs/Decisions.md | Implemented | Docs/Decisions.md mirrors DECISIONS.md | |
| E2 | Handoff: built artifacts, hosts/OS/rates tested, commands and results, CPU and latency, scenarios, unresolved gates | Partial | QA.md in progress (commands only) | Complete QA.md (see M1.5h) |
| E3 | Commits grouped logically | Implemented | git log | |

---

## 2. Realism constraints (`INTERFERENCE_VST_REALISM_CONSTRAINTS.md`)

### §1 Product rule and range policies

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| R1.1 | Natural default; Expressive bounded; no hidden anything-goes mode | Implemented | `policy` default 0; `PhysicalLimits` columns | |
| R1.2 | Realistic factory presets use Natural | Implemented | Expressive only in "Wild" presets | |
| R1.3 | Mark Expressive on preset save and export | Implemented | `MainView.cpp:320-329` alert; StateIO `policyLabel`; render popup "Policy:"; sidecar | |
| R1.4 | Changing policy never rewrites stored knob values | Implemented | policy only selects limit columns | |
| R1.5 | Show the **effective value** and a small **"Limited by …"** explanation when a request exceeds what the circuit can produce | Partial | `ParamKnob::setLimited` draws only an **unlabelled 5 px dot** (`Widgets.cpp:206-210`); the reason text is never shown (not in the tooltip, `Widgets.cpp:129-141`). `SourceTelemetry::effective[]` and `limited` are computed but never displayed, and `limited` is always false. Only three conditions set the dot (`SlotPanel.cpp:343-352`). **Excitation Level is capped silently**: range up to −6 dB, Natural cap 0.1 FS ≈ −20 dB (`Engine.h:247`), so the top 14 dB of the knob do nothing, with no indication | Show `limitedReason` as caption text and in the tooltip; draw an effective-value tick on the arc; set `tele.limited/effective` in the sources; add clamps to the indicator for Excitation Level (vs `excitationPeak`), Feedback (already reported in Studio) and Pop/Hum (~60 lines in Widgets/SlotPanel/Source headers) |
| R1.6 | Smooth policy transitions without a false contact event | Missing | switching policy changes `popPeak` in steps, so the charge node jumps and pops (see M4.22); hum and bandwidth also step; no test | Fix M4.22; ramp the policy-dependent scalars; add a test |
| R1.7 | Tempo grids schedule gestures, not microscopic events | Implemented | `scheduleLane` | |

### §2 General invariants

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| R2.1 | A healthy closed contact at rest makes no crackle | Implemented | `Core.contactWithZeroHazardNeverLeavesGood` | |
| R2.2 | Silence invariant for every source, pair, circuit mode and wet mix | Implemented | `Engine.silenceInvariantEverySourcePairModeAndMix` sweeps input circuit × mix × pairs. Pot Tone circuit and Expressive policy are not swept | Extend the loops (P3) |
| R2.3 | Events follow movement, condition, wear, pressure or enabled excitation; not Density alone | Implemented | hazards proportional to motion/W; `Pattern.densityZeroSchedulesNothing` | |
| R2.4 | Attenuation on the through path; pops from interruption or charge; handling on a separate path; buses distinguishable | Implemented | `paths[3]`; per-slot electrical/mechanical telemetry | |
| R2.5 | An open path keeps no full-strength signal; a shielded path has no strong hum | Implemented | `withdrawnPlugLosesThroughSignal`, `withdrawnPlugHumDependsOnShielding` | |
| R2.6 | DC bounded and removed; stereo shares movement | Implemented | ChargeCircuit high-pass; shared contact | |
| R2.7 | Intermittency/Density cannot invent events | Implemented | hazard multiplies motion; Density scales gesture probability only | |
| R2.8 | Pop cannot generate pops without a transition | Partial | turning or automating Pop / Circuit Bias on a closed still contact makes a transient (M4.22) | Fix M4.22 |
| R2.9 | Mix/Output/Level do not change whether an event is possible | Implemented | applied after the core | |
| R2.10 | 100 % means the highest credible value, never unlimited | Implemented | limits scale every amount | |
| R2.11 | Central `PhysicalLimits` table with Natural and Expressive columns, units, empirical status | Implemented | `PhysicalLimits.h` | |
| R2.12 | A test for every bound | Partial | tested: maxEventRate, maxOpenMs, maxMotionSpeed/Acceleration, popPeak, backgroundCeiling, excitationPeak, minRetriggerMs, noise budget. **Untested: maxResistiveMs, humPeak, scratchGain, mechanicalPeak, injectionBandwidth, gestureRate.** The header comment cites a non-existent `Tests/TestLimits.cpp` (`PhysicalLimits.h:6`) | Add six bound tests (~80 lines); fix the comment |
| R2.13 | Digital safety limits kept separate from perceptual caps | Implemented | `kSafetyClamp` | |

### §3–§6 Source tables (every row)

| # | Row | Status | Evidence |
|---|---|---|---|
| R3.1 | Jack seated, healthy, little movement → near-transparent | Implemented | `LooseJack.seatedHealthyIsNearTransparent` |
| R3.2 | Moved through the unstable region → clustered events, not evenly spaced | Implemented | `unstableZoneMovementMakesClusteredFaults` |
| R3.3 | Withdrawn → through signal lost; hum depends on termination | Implemented | `withdrawnPlugLosesThroughSignal`, `withdrawnPlugHumDependsOnShielding` |
| R3.4 | Worn region fails repeatably; pristine positions rarely | Implemented | `wornLocationFailsMoreThanPristine` |
| R3.5 | Grounded state quiets instead of maximum hum | Implemented | `shortToSleeveIsQuieterThanOpen` |
| R3.6 | Pop with zero bias → only the interruption click | Implemented | `popNeedsBias` |
| R3.7 | Position-dependent probability and hysteresis | Implemented | `LooseJack::control`, cluster term in ContactModel |
| R3.8 | High Intermittency at a healthy seated spot shows **"Contact stable here"** | Partial | shown only as the unlabelled dot (R1.5) and inside the Why-quiet dialog (`SlotPanel.cpp:32`) |
| R3.9 | Offer a guided **"Move toward worn area"** action | Missing | advice text only. Fix: a button in the Why-quiet dialog / ContextPanel that ramps `jk_insert` toward `jk_wearpos` with a host gesture (~25 lines) |
| R3.10 | Sustained rhythmic gating needs real movement through the unstable region | Implemented | hazard needs motion or the partial zone |
| R3.11 | Hum on a disconnected plug is not mandatory | Implemented | Shield and Hum controls |
| R4.1 | Touched high-Z tip → 50/60 Hz plus harmonics, not broadband or melody | Implemented | `PlugTouch.touchCouplesMainsNotBroadband` |
| R4.2 | Terminated and shielded → little pickup | Implemented | `shieldedTerminatedPicksUpLittle` |
| R4.3 | Untouched → weaker ambient pickup | Implemented | `untouchedIsWeakerThanTouched` |
| R4.4 | Finger held still → no repeated onset events | Implemented | `stationaryFirmFingerMakesNoOnsetEvents` |
| R4.5 | Press/release → chatter around transitions only | Implemented | `pressAndReleaseChatterAroundTransitions` |
| R4.6 | Mains phase continuous; 50↔60 Hz controlled transition | Implemented | Environment glide; core test |
| R4.7 | Body shapes coupling, not pitch | Implemented | brightness mix only |
| R4.8 | Shield and Circuit Exposure reduce pickup | Implemented | `PlugTouch::control` |
| R4.9 | RF optional, separate, labelled "Interference" | Implemented | `rf` parameter named "Interference" |
| R4.10 | Create excitation labelled plainly | Implemented | "Excitation (studio source)" caption |
| R4.11 | Hum off → only brief contact/bias events | Implemented | `humOffLeavesOnlyBriefContactEvents` |
| R5.1 | Sound cable, Mic 0 → little or no handling sound | Implemented | `soundCableWithoutMicIsSilentAndTransparent` |
| R5.2 | Bend over the damage point → clustered loss tied to location | Implemented | `bendOverWeakSpotFaultsMoreThanElsewhere` |
| R5.3 | Repeated shake → motion-linked clusters, no static when motion stops | Implemented | hazard ∝ motion; `handlingFollowsMotionAndStops` (mechanical path) |
| R5.4 | Rubbing with Mic → texture tied to speed | Implemented | `handlingFollowsMotionAndStops` |
| R5.5 | Tap with Mic → short decaying response | Implemented | `tapIsShortAndDecays` |
| R5.6 | Bend position, strain, persistent weak-spot map | Implemented | `CableHandling.h` |
| R5.7 | Shield and conductor are separate fault paths | Implemented | `shieldFaultAdmitsHumWhileConductorLive` |
| R5.8 | Friction stops when motion stops (plausible tail only) | Implemented | test above |
| R5.9 | Mechanical sound requires Mic | Implemented | `micOffMeansNoHandlingSound` |
| R5.10 | Shake schedulable, events vary with velocity | Implemented | lane Shake gesture |
| R6.1 | Clean pot → taper curve, no crackle | Implemented | `cleanPotFollowsTaperWithoutScratch` |
| R6.2 | Dirty pot through the worn arc → localized clustered scratch | Implemented | `turningThroughWornArcScratchesThere` |
| R6.3 | Dirty pot held still → no new scratch | Implemented | `heldStillStopsScratch` |
| R6.4 | Rapid turning → finite speed and arc | Implemented | MotionDriver caps; `Core.motionSpeedAndAccelerationAreCapped` |
| R6.5 | Silent input, zero bias → no scratch | Implemented | `silentInputZeroBiasIsSilentBiasIsNot` |
| R6.6 | Taper vs Dirt/Wear Position; Scratch scales valid events only | Implemented | `DirtyPot::control` |
| R6.7 | Finite arc; automatic gestures reverse, never wrap | Implemented | `MotionDriver::step` clamps; Sweep returns |
| R6.8 | Tone topology identified; the filter really moves | Implemented | `toneCircuitFiltersNotJustLevel` |
| R6.9 | Scratch on silence only with a labelled bias source | Implemented | `pt_bias` tooltip "labelled powered circuit" |
| R6.10 | Stable wear map with some event variation | Implemented | seed-only map |

### §7 Two sources

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| R7.1 | Defined series path; a downstream open contact interrupts upstream audio and artifacts | Implemented | `Engine.downstreamOpenContactInterruptsUpstreamArtifacts` | |
| R7.2 | A linked gesture keeps separate maps and random streams | Implemented | `Engine.h:384-396` | |
| R7.3 | Shared mains/environment; global noise budget; no double-counted hum | Implemented | `Engine.h:428-441`; `twoExposedPathsRespectTheNoiseBudget` | |
| R7.4 | Explicit headroom protection that keeps quiet realism | Implemented | Ceiling (explicit) plus a numeric-only failsafe | |
| R7.5 | Pops from one linked gesture are correlated, not layered copies | Partial | independent contacts driven by the same motion; not tested | Add a test (P3) |
| R7.6 | GUI reasons: "No input reaches this open contact", "Cable mic is off", "Pot is stationary", "Circuit is shielded", "Excitation is off" | Implemented | `SlotPanel.cpp:11-61` `explainQuiet` | |
| R7.7 | Never add audio in secret | Implemented | | |

### §8 Musical leeway

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| R8.1 | Expressive: faster gestures, more wear, denser bursts, louder explicit excitation | Implemented | limits table | |
| R8.2 | Rhythm sync never retunes mains or quantizes crackles | Implemented | | |
| R8.3 | Post effects are separate, labelled stages | Implemented | Studio chain | |
| R8.4 | Create: Noise, Buzz, Test Tone, Charge excitation | Implemented | `exc_type` | |
| R8.5 | **UI visually distinguishes the excitation signal from the fault response** | Partial | only the Create-tab text and the Monitor "Injected Artifacts" tap. The engine publishes `excitationActive` (`Engine.h:635`) but no UI reads it; there is no excitation meter or badge outside the Create tab | Add an "EXCITATION ON" badge in the mode row and an excitation level meter in Routing/Create (~30 lines) |
| R8.6 | Natural exports carry only permitted artifacts; expressive or post use disclosed in metadata | Implemented | preset `policyLabel`, `studioProcessed`, `internalExcitation`; render popup; sidecar | |
| R8.7 | Exact timing and seed reproducibility | Implemented | | |

### §9 UX requirements

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| R9.1 | Illustrations match the real source state | Implemented | telemetry driven | |
| R9.2 | Grey out irrelevant controls with a concise reason | Partial | done for Cable Load, Input Drive/Headroom, Body, Vibration, Ceiling (`SlotPanel.cpp:330-341`). Not done for e.g. Echo Time when Sync is on, Res Freq with Follow MIDI, Studio stage controls while the stage or Studio is off, Mic-dependent controls | Extend the `dormant(...)` calls (P3) |
| R9.3 | Effective-value indicators when clamps apply | Partial | see R1.5 | see R1.5 |
| R9.4 | Dormant parameters kept and restored | Implemented | values untouched | |
| R9.5 | "Why is it quiet?" | Implemented | SlotCard "Why quiet?" | |
| R9.6 | No abrupt pops on source switching, order, policy or bypass; transport/seed state preserved | Partial | slot, order and bypass are declicked; a policy switch can pop (R1.6) | Fix M4.22 |
| R9.7 | The central illustration has an alternative slider, label, keyboard use and tooltip | Partial | slider, label and keyboard present; no tooltip | see M8.7 |
| R9.8 | Status animation never fabricated | Partial | Touch field animation (M8.10) | see M8.10 |
| R9.9 | Natural/Expressive, Excitation and Mic labels prominent | Partial | policy in the mode row; Mic knob and illustration label; Excitation only in the Create tab (R8.5) | see R8.5 |
| R9.10 | Details: separate meters for through signal, electrical and mechanical injection | Partial | three meters exist (`SlotPanel.cpp:357-359`), but "through" shows **R** (the healthy reference, not the contact-affected path), and for Cable "electrical" includes the mechanical signal (`CableHandling.h:164` sums them; `tele.activity`) | Publish C0 peak and a mechanical-free electrical peak per slot |
| R9.11 | Presets and exports save policy, physical inputs, seed/maps, stage labels | Implemented | StateIO, sidecar | |

### §10 Verification

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| R10.1 | Deterministic test of every table row at representative sample rates **and block sizes** | Partial | every row has a test, but all run at 48 kHz, block 256 (`TestSources.cpp:10-14`) | Run the `Single` harness over {44.1 k, 96 k} × {64, 1024} (~20 lines) |
| R10.2 | Assertions on audible output and telemetry | Implemented | tests check both | |
| R10.3 | All six pairs in both orders | Partial | M6.9 | |
| R10.4 | Property tests sweep ranges and modes | Implemented | `extremeParameterFuzzIsBoundedAndFinite` | |
| R10.5 | **Test UI disabled states and "Why is it quiet?" against real engine dependencies** | Missing | no test calls `explainQuiet` or `setDormant` | GUI test: drive states (Mic 0, pot still, shielded), assert the strings and dormant flags (~50 lines) |
| R10.6 | Research-informed label when no references | Implemented | Research.md, Issues O2 | |
| R10.7 | Research traceability | Implemented | Research.md | |

---

## 3. Effects chain and routing (`INTERFERENCE_VST_EFFECTS_CHAIN_AND_ROUTING.md`)

### §1 External I/O

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| C1.1 | Main in/out mono or stereo; channel count kept; surround rejected; silence and zero-length | Implemented | `isBusesLayoutSupported`; Host test | |
| C1.2 | Optional sidechain for detector/key only; absent → visible status; never replaced by the main input | Implemented | `PluginProcessor.cpp:288-299`; RoutingView status; `sidechainNeverLeaksIntoOutput` | |
| C1.3 | Sidechain documented and validated in a real host | Partial | documented (Routing.md); host validation not possible | Release gate |
| C1.4 | No core feature requires the sidechain | Implemented | | |
| C1.5 | MIDI: gestures, pattern start/Fill, pressure/CC, resonator note; GUI/automation equivalents; safe handoff | Implemented | MidiMap.md | |
| C1.6 | Output layout equals input; no-input generator only if supported | Implemented | `in != out` rejected; Diagnostics warns when there is no input | |
| C1.7 | Off-thread WAV export, explicit tap and tail policy, no automatic files, user path, no overwrite | Implemented | RenderOverlay, LiveRecorder | |

### §2 Canonical graph, buses, mix

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| C2.1 | Bus names used in code, docs, UI and tests | Implemented | `Engine.h:478-540`; Routing.md; RoutingView | |
| C2.2 | R, C0 and C counterfactuals; `C = R + D + A` exact | Implemented | three paths per sample | |
| C2.3 | Nonlinear preamp handled with matched counterfactual paths | Implemented | `preSat[3][2]` | |
| C2.4 | A never built from pre-slot noises | Implemented | A = C − C0 after both slots | |
| C2.5 | F = A / D / A+D; U; Studio bypass P = F so W = C | Implemented | `studioBypassReturnsExactlyC` | |
| C2.6 | Named taps, reproducible, never x in place of R | Implemented | `Engine.h:545` | |
| C2.7 | Preview and export use the same tap semantics | Implemented | the renderer sets `monitor` | |
| C2.8 | Linear default, switchable constant power; 0 % = aligned dry; 100 % = exactly W | Implemented | `zeroWetIsTimeAlignedDry` | |
| C2.9 | Input Trim on the wet path; separate dry compensation | Implemented | `in_trim`, `dry_trim` | |
| C2.10 | Output trim after the mix; labelled, testable protection | Implemented | Ceiling | |
| C2.11 | True bypass is time-aligned dry | Implemented | `pluginBypassIsDry` | |
| C2.12 | Sidechain never in dry/wet | Implemented | test | |

### §3 Stage order and algorithms

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| C3.0 | Fixed order; independent bypass and wet with declicking | Implemented | `StudioChain::process` stageMix ramps; `eachStageBypassIdentityAfterToggle` | |
| C3.1 | EQ: HP, LP, tilt, broad peak; stereo-linked; no sound from zero; **coefficients smooth** | Partial | coefficients jump each block (`StudioChain.h:115-124`) | Interpolate or smooth EQ parameters (P3) |
| C3.2 | Contour: transient/sustain, clamped gain | Implemented | | |
| C3.3 | Drive: Soft/Hard/Asym, pre/post, wet, tone, DC blocker, ADAA, bypassed by default | Implemented | `driveAdaaAliasesLessThanNaive`, `asymmetricDriveDcIsBlocked` | |
| C3.4 | Crush: bits and hold rate, seeded dither, phase continuity, "Digital Crush" label | Implemented | crush tests | |
| C3.5 | Resonator: two modes, freq or MIDI, decay, blend, bounded feedback, identity bypass, no self-oscillation, **meter stage input/output** | Partial | all but the meter. `stageIn/stageOut` are declared (`Telemetry.h:28`) and never written or shown | see C3.13 |
| C3.6 | Rhythm Gate: Through/Pulse/Choke; keys; Studio only; choke kills feedback / clears echo; **tempo fallback displays status** | Partial | all but the status: with no clock, Steps-key Pulse silently stays open (`StudioChain.h:237`) | Show "Gate: no clock (open)" in the Studio note (P3) |
| C3.7 | Echo: filtered, sync/free, safe feedback, finite tail | Implemented | `echoTailIsFinite` | |
| C3.8 | Local feedback: after the Resonator, before the Gate, into Drive input; HP/LP, DC, saturator, delay; 0 by default; Kill Feedback, MIDI-mappable | Implemented | `StudioChain.h:285-346` | |
| C3.9 | Exact tap and destination visible in the advanced routing panel | Partial | the RoutingView flow text omits the loop (`StudioPanel.cpp:181-190`); only Routing.md and the tooltip show it | Add a line "Feedback: Resonator out → Drive in (x %)" |
| C3.10 | Stable loop gain guaranteed, or the combination clamped with a reason | Partial | clamp and reason exist, but the loop-gain estimate **ignores Auto Match gain (up to ×4) applied inside the loop** (`StudioChain.h:176-181` vs `315-325`), so a sustained limit cycle is possible (tanh-bounded, not finite) | Multiply `loopGain` by 4 when Auto Match is on, or apply Auto Match after the feedback tap (~3 lines + test) |
| C3.11 | No runaway hidden behind a brickwall | Implemented | | |
| C3.12 | Maximum tail length limited; freeze behaviour documented separately | Partial | tail capped at 30 s in reporting; freeze behaviour not documented | Add a Routing.md note on host freeze and tails |
| C3.13 | **Per-stage peak/RMS meters** plus one master peak meter | Partial | master and bus meters present; per-stage meters missing | Write per-stage in/out peaks into `telemetry.stageIn/stageOut` in `StudioChain::process` (pass a pointer) and draw 7 small meters under the stage buttons in `StudioPanel` (~50 lines) |
| C3.14 | Default patches keep headroom when everything coincides | Implemented | `factoryPresetsPlayOnMusicWithoutPeaksOrLevelJumps` | |
| C3.15 | Auto Match: measured, bounded, no pumping | Implemented | `autoMatchIsBounded` | |
| C3.16 | Optional Ceiling with stated lookahead/latency; numeric-only failsafe | Implemented | "zero-latency" tooltip; `Engine.h:564-566` | |
| C3.17 | Test overs and intersample peaks | Missing | no Ceiling test | see H19 |
| C3.18 | Silence stays silent except for tails or enabled sources | Implemented | `noStageAddsSoundFromSilenceWithoutFeedbackState` | |

### §4 Routing UI

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| C4.1 | Input meter with mono/stereo indicator | Implemented | OutputStrip "in" meter + "stereo in/out" | |
| C4.2 | Source A/B shown in user order | Partial | see M6.5 | |
| C4.3 | Studio Source: Artifacts / Breaks / Both | Implemented | StudioPanel | |
| C4.4 | Compact fixed chain of seven labelled on/off blocks | Implemented | StudioPanel stage row | |
| C4.5 | Feedback send | Implemented | 8th stage "Feedback" | |
| C4.6 | Raw Hardware | Implemented | | |
| C4.7 | Dry/Wet | Implemented | | |
| C4.8 | Output meter | Implemented | | |
| C4.9 | Capture Tap | Implemented | Routing tab | |
| C4.10 | Routing view: taps R, D, A, P, W, x, y; sidechain state; latency and tail | Implemented | `StudioPanel.cpp:152-167` | |
| C4.11 | One source loaded → empty slot bypassed without relabelling | Implemented | | |
| C4.12 | "No artifact to process" | Implemented | `StudioPanel.cpp:76` | |
| C4.13 | Raw Hardware disables Studio, feedback and excitation (Keep Excitation), restores C, crossfaded | Partial | correct, except that the hidden Ghost Carrier is applied **after** the Raw Hardware blend (`Engine.h:534-537`), so Raw Hardware ≠ C while it is on (Routing.md claims W = C) | Skip the ghost modulation when `rawHw` is on (P3) |
| C4.14 | Core-only distinct from plugin bypass | Implemented | Monitor "Full Core" vs Bypass | |
| C4.15 | Monitor choice never changes RNG, contacts or order | Implemented | | |

### §5 Capture, offline render, latency

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| C5.1 | Live capture: default y; taps C, A, D, P, W, x | Implemented | `captureTap` combo | |
| C5.2 | Tapping never changes the output or RNG | Implemented | `Engine.h:569-573` | |
| C5.3 | Offline render: cloned engine, x = 0, s = 0; sample import deferred | Deviation | host audio / sample import deferred (Manual "Export limitations", Issues O5) | |
| C5.4 | Tail include/trim choice; duration and echo exclusion reported | Implemented | render popup "(including tail)/(no tail)" | |
| C5.5 | Mono/stereo, correct headers, naming; D signed | Implemented | `faultDifferenceTapKeepsSignAndMonoWorks` | |
| C5.6 | Constant latency | Implemented | 0 | |
| C5.7 | Parallel paths aligned | Implemented | zero-latency design | |
| C5.8 | Tail covers Echo, feedback, resonator; Kill Feedback ≠ Kill Tail | Implemented | `StudioChain::tailSeconds`; separate kills | |
| C5.9 | Host freeze/offline deterministic | Implemented | lock and bar keying | |

### §6 Interaction with realism

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| C6.1 | Constraints apply before Studio; no post effect writes fault state | Implemented | | |
| C6.2 | Local feedback starts silent; tails finite and killable | Implemented | `localFeedbackTailIsFiniteAndKillable` (without Auto Match; see C3.10) | |
| C6.3 | Studio bypass returns exact C | Implemented | | |

### §7 Validation matrix

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| C7.1 | Impulse/silence/music through every I/O layout and monitoring tap | Partial | mono/stereo and most taps tested separately, not the full matrix | Loop layouts × taps in one test (P3) |
| C7.2 | Algebraic nulls C−(R+D+A), W−C; 0 % wet = dry | Implemented | Engine tests | |
| C7.3 | Nonlinear preamp tested with counterfactual buses | Missing | `pre_on` only appears in TestPerf | Test with Input Drive on: Studio-off W == C and D/A bounded (~20 lines) |
| C7.4 | Both orders, source bypass, single/both, excitation, sidechain absent/present | Implemented | Engine tests | |
| C7.5 | Neither sidechain nor feedback leaks to the dry branch | Implemented | `sidechainNeverLeaksIntoOutput`, `chokeClosesGateOnlyInStudioBranch` | |
| C7.6 | Each chain block's bypass identity | Implemented | Studio tests | |
| C7.7 | All-stage peak and CPU | Implemented | `Perf.cpuRealtimeFactorWorstCase` (slow; not yet run) | |
| C7.8 | Abrupt parameter changes | Partial | fuzz uses static parameters per run | Add per-block random parameter jumps to the fuzz (P3) |
| C7.9 | Stereo phase | Missing | no test | Check L/R correlation with Stereo Difference 0 (P3) |
| C7.10 | Drive aliasing, Crush, resonator and feedback stability, Gate never mutes dry, Echo finite | Implemented | Studio tests | |
| C7.11 | Plugin bypass, capture, export | Implemented | | |
| C7.12 | Host latency and buffer-size changes | Implemented | block-size and variable-block fuzz | |
| C7.13 | Project reopen with a locked seed | Partial | see T13 | |
| C7.14 | Listening tests: snare, bass, guitar, silence | Not verifiable | | |
| C7.15 | An unstable or unverified stage is listed as a release blocker | Implemented | Issues.md | |

---

## 4. Industrial performance (`INTERFERENCE_VST_INDUSTRIAL_PERFORMANCE.md`)

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| I1.1 | Machine Pattern extends the Gesture Lane, not a second sequencer | Implemented | DECISIONS.md; one `Pattern` | |
| I1.2 | Never inserts unmotivated crackle samples | Implemented | only triggers gestures | |
| I1.3 | Does not change existing preset timing or turn Studio on | Implemented | lane/Studio default off | |
| I2.1 | One lane per source (A, B), shared 1–64 step timeline; 1/8, 1/16, 1/32 and triplets | Implemented | `Pattern.h` | |
| I2.2 | Step fields: Gesture, Intensity, Duration, Position Target, Studio Send, Probability, Accent | Implemented | `LaneStep`; step menu `LaneEditor.cpp:194-256` | |
| I2.3 | Source-aware events (Jack Wiggle/Seat/Pull; Touch Tap/Hold/Release; Cable Bend/Shake/Tap; Pot Turn-to-Angle/Sweep/Hold) | Implemented | `Source.h` `patternGesture` | |
| I2.4 | Mute and solo per lane | Implemented | | |
| I2.5 | Link Motion: one gesture in both lanes with independent fault state | Implemented | `link` | |
| I2.6 | Keyboard editing: copy/paste, shift, rotate, clear, undo | Implemented | `StepGrid::keyPressed` | |
| I2.7 | Step probability is a seeded per-loop decision | Implemented | hash(seed, lane, absolute step) | |
| I2.8 | Accent scales force and a separate post-chain Studio send, bounded, no prerequisite bypass | Implemented | `Pattern.h` e.intensity×1.35, send×1.25 | |
| I2.9 | Duration sustains; source-specific release | Implemented | `GestureGenerator` | |
| I2.10 | Position Target can aim at the worn region | Implemented | "Aim at the worn region" menu | |
| I2.11 | Ratchet 2–4; too fast → **"Motion limit reached"** and coalesce | Partial | coalescing works (`naturalCoalescesFastRatchetsExpressiveAllows`); the message is drawn only on the Loose Jack illustration (`Illustrations.cpp:290-291`) | Draw it in `Illustration::paint` for every source, or in the lane clock text |
| I2.12 | Bounded, seeded microtiming | Implemented | offset ±0.5 step; humanize seeded | |
| I2.13 | Swing on offbeats | Implemented | `swingDelaysOnlyOffbeats` | |
| I2.14 | Density = probability of eligible gestures | Implemented | | |
| I2.15 | Evolve: seeded per-bar change, positions and map kept | Implemented | `evolveKeepsPositions` | |
| I2.16 | Lock reproducible across bounce, loop, render, reopen | Implemented | tests (reopen partial, T13) | |
| I2.17 | Fill momentary; no permanent overwrite | Implemented | `fillAddsEventsWithoutChangingThePattern` | |
| I2.18 | MIDI one-shot trigger and note-off policy | Implemented | note 62; `midiOneShotPatternAndNoteOffPolicy` | |
| I2.19 | Host automation of Fill, Pattern Level, Studio Send | Implemented | parameters | |
| I2.20 | No host tempo: internal BPM, stateful start/stop, visible fallback indicator | Implemented | Run + LaneEditor clock text (`LaneEditor.cpp:383-391`) | |
| I2.21 | Phase restored from musical position after discontinuities | Implemented | `scheduleLane` pure in ppq; bar re-keying | |
| I3.1 | Different lane lengths on a common clock | Implemented | `crossRhythm16Against12` | |
| I3.2 | Full combined cycle limited to a **documented maximum** | Missing | the cycle can reach 64 × 63 = 4032 steps; no documented maximum | State the maximum (4032 steps) in Manual and DECISIONS |
| I3.3 | Deterministic phase after a seek | Implemented | pure scheduling | |
| I3.4 | UI shows "A: 16", "B: 12" and a **loop-phase indicator** | Partial | lengths and cycle length shown; no position within the combined cycle | Add "cycle step n / N" to the clock text |
| I3.5 | Choke stops the Studio tail with a click-free envelope; never the hardware or clean path | Implemented | `chokeClosesGateOnlyInStudioBranch` | |
| I3.6 | A "Gap" step holds a jack open or pot at minimum with a valid gesture | Partial | possible via Pull/Turn-to with a 0 % target and duration; no labelled Gap | Add a "Gap" item to the step menu that sets gesture = Pull/Turn-to, target 0, duration 1 |
| I3.7 | A hard rhythmic gate uses the Studio Rhythm Gate | Implemented | | |
| I4.1 | Four factory presets: broken snare, machine percussion, breakdown, controlled chaos | Implemented | "Machine Pattern" category | |
| I4.2 | No artist names | Implemented | | |
| I4.3 | A/B, dry mix, source bypass, Raw Hardware and MIDI stay usable while the pattern runs | Implemented | | |
| I5.1 | Controls only gestures and Studio sends/chokes | Implemented | | |
| I5.2 | Bounded, versioned pattern data embedded in state | Implemented | StateIO | |
| I5.3 | **Never allocate per step or sort the event list on the audio callback; compile an immutable event table on another thread** | Partial | no allocation, and the pattern arrives by seqlock. But events are scheduled per block **on the audio thread** and insertion-sorted there (`Pattern.h:169-272`, sort at 263; the processor also sorts MIDI/UI events at `PluginProcessor.cpp:253-260`). Bounded, not recorded as a decision | Record the interpretation in DECISIONS.md (bounded n ≤ 512), or precompile a sorted per-cycle table on the message thread |
| I5.4 | Probability and timing seeded by absolute cycle + lane + step | Implemented | | |
| I5.5 | Tail/choke affect Studio only, never host dry | Implemented | | |
| I5.6a | Test odd time signatures | Missing | `sigNum` is never set in tests | Run the scheduler with 7/8 and 5/4 (~20 lines) |
| I5.6b | Test host loops | Implemented | `loopJumpRepeatsHardwareEvents` | |
| I5.6c | Test tempo automation | Missing | | see T11 |
| I5.6d | Test mono/stereo, silence, 12 vs 16, ratchet limits | Implemented | Pattern and Engine tests | |
| I5.6e | Test Fill release | Partial | Fill on only; release not asserted | Check events return to the base pattern after release |
| I5.6f | Test project reopen | Partial | T13 | |
| I6.1 | Provenance statement; keep research traceability (SOS NIN articles, NIME 2021, DAFx feedback paper) | Partial | Research.md has no entry for the industrial/feedback sources in this spec or the routing spec's second DAFx link | Add a "Machine Pattern and Studio" section with the four URLs, labelled as design inspiration |

---

## 5. General plugin requirements (`include.md`)

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| G1 | In-plugin help explains each feature | Partial | 18 help sections; the Studio stages' individual controls (Drive curves, Crush, Gate keys/modes, Echo, Auto Match, Ceiling) are only listed, not explained | Add a "Studio stages" help section (text only) |
| G2 | Help: how to use it, workflow | Implemented | "Quick start" | |
| G3 | Help: GUI description and controls | Implemented | "The window" | |
| G4 | Help: version number | Implemented | Overview, About | |
| G5 | Help: licence | Implemented | About (`Branding::licenseSummary`) | |
| G6 | Help: GitHub link if open source | N/A | closed source (`sourceRepository = ""`) | |
| G7 | Help: homepage link | Partial | placeholder `https://interference-audio.example` (`HelpContent.h:13`; also in CMakeLists) | Publisher must supply the real URL |
| G8 | Help: support email | Partial | placeholder `support@interference-audio.example` (`HelpContent.h:14`) | Publisher must supply it |
| G9 | Help: manual install/uninstall troubleshooting | Implemented | "Install, uninstall, troubleshooting" | |
| G10 | Help: where to put preset files | Implemented | "Presets" | |
| G11 | Help has a Debug button | Implemented | `HelpOverlay` "Debug..." | |
| G12 | Unique custom icon for the exe | Implemented | `Resources/icon.png` (`tools/make_icon.py`); ICON_BIG/ICON_SMALL | |
| G13 | Bank of presets with descriptive names | Implemented | 22 presets | |
| G14 | Reset button restores defaults | Implemented | header Reset (keeps MIDI mappings, documented) | |
| G15 | Save / Save As / Open in a dropdown menu | Implemented | `MainView::showMainMenu` | |
| G16 | Options page: tooltips on/off, MIDI and audio device choice, "open location in Explorer" | Implemented | `OptionsOverlay`; devices in stand-alone only (host owns them, explained on the page) | |
| G17 | Export audio with basic quality options and a success popup (location, length, name, quality) | Implemented | Render popup (`Overlays.cpp:571-580`), Record popup (`MainView.cpp:355-367`) | |
| G18 | MIDI import/export only when it makes sense | N/A | effect plugin; not offered. Not recorded as a decision although the plugin has a lane sequencer | One DECISIONS.md line |
| G19 | Right-click on any control: MIDI map, reset, set a value | Partial | knobs, toggles and choices: yes. Fill has one too. Illustrations and non-parameter buttons (slot Play, mode, Lock, New Take) have no menu | Add `showParamMenu(primaryParam())` on right-click in `Illustration` |
| G20 | Drag and drop when samples can be imported | N/A | no sample import | |
| G21 | Hover tooltips | Implemented | illustrations excepted (M8.7) | |
| G22 | Randomize: new settings every press; each later press resets first | Implemented | `PluginProcessor::randomize` resets to init every press | |
| G23 | Progress file kept up to date | Partial | PROGRESS.md stops at session 1; TODO.md is current | Update PROGRESS.md or point it at TODO.md |
| G24 | Push to a private GitHub at milestones | Deviation | DECISIONS.md "No automatic GitHub push" | |
| G25 | VST and stand-alone versions | Implemented | FORMATS | |
| G26 | Debug window: raw data live as features change; misconfiguration signals | Implemented | `DebugOverlay`, `Diagnostics::liveData`, `findMisconfigurations` | |
| G27 | "Create log file on crash" checkbox; date/time-stamped; off on every load | Implemented | `CrashLog`; not persisted | |
| G28 | User told where the log is and how to send it | Implemented | Help, Install.md, checkbox tooltip | |
| G29 | Hard reset to defaults removing cache files | Implemented | `PluginProcessor::hardReset` deletes the options file | |
| G30 | Troubleshooting file: light self-diagnosis, all settings, audio and MIDI settings, licence, version, DAW info | Implemented | `Diagnostics::troubleshootingReport` | |
| G31 | Crash log starts with a copy of the troubleshooting file | Implemented | `startCrashLog` header; GUI test | |
| G32 | Explain both files and to send both when hard-crashing | Implemented | Help "Install, uninstall, troubleshooting" | |
| G33 | Easter egg: one pixel reveals a hidden tab with controls, a close button, tooltips say it is secret | Implemented | `MainView.cpp:420-425`, `GhostOverlay`; tooltips start "Secret hidden effect" | |
| G34 | Implement theme.md | Implemented | see §6 | |

## 6. Visual identity (`theme.md`)

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| V1 | Colour palette tokens | Implemented | `Theme.h:12-25` | |
| V2 | Fonts: Inter → Space Grotesk → system; mono JetBrains/IBM Plex; weights 500/600 | Partial | families and fallback correct; weights are plain/bold (JUCE has no 500) | none needed |
| V3 | Labels 11 px, uppercase, +0.08 em; values 13–14 px | Implemented | `Theme::drawLabel` | |
| V4 | Knob: 270° arc outside the body with a 4 px gap, 2 px indicator, gradient body, 4 px centre dot muted at default | Implemented | `Theme.cpp:120-163` | |
| V5 | Knob sizes 48 / 36 / 64 px | Partial | knobs scale with their cells | P3 |
| V6 | Double-click resets; right-click opens value entry | Implemented | double-click default; right-click menu with "Set value…" | |
| V7 | Value above (hover/drag), label below | Implemented | `ParamKnob::paint` | |
| V8 | Slider track, fill, 16×24 thumb, **ticks** | Partial | no ticks on linear sliders | P3 |
| V9 | Buttons 28 px, 4 px radius, on = 15 % accent fill, accent text and border; 100 ms flash | Implemented | `drawButtonBackground`; `flash()` (header buttons 26 px) | |
| V10 | Meters: smooth gradient teal→orange→yellow→red; peak hold 1.5 s then −20 dB/s; mono peak readout top right | Implemented | `LevelMeter` | |
| V11 | 8 px grid, 1 px separators, section header accent bar, radii, 16 px edge padding | Implemented | `MainView::resized` (`r.reduce(16,0)`), `drawSectionHeader` | |
| V12 | Never boxes inside boxes | Partial | slot cards and illustrations are bordered panels inside columns | P3 |
| V13 | **All value changes animate over 80 ms ease-out** | Missing | no value animation | P3: animate the knob arc toward the target in `ParamKnob` |
| V14 | Hover brightens ~8 %; **cursor becomes vertical resize on knobs/sliders** | Partial | brighten yes; no `setMouseCursor` anywhere | `slider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor)` in ParamKnob |
| V15 | Drag: vertical fine, Shift coarse, Ctrl ultra-fine | Implemented | `FineSlider::mouseDown` | |
| V16 | Tooltips: dark pill, muted text, 400 ms | Implemented | `Theme.cpp:263-289`; `TooltipWindow(this, 400)` | |
| V17 | Header: 32 px, name 14 px semibold left; gear, preset selector, A/B right; separator | Implemented | `MainView::paint/resized` | |
| V18 | Signature notch 2 px, 12 px, 45°, top left | Implemented | `MainView.cpp:489` | |
| V19 | Footer version, 9 px muted mono, bottom right | Implemented | `MainView.cpp:502-504` | |
| V20 | LED top left: gray → white with level, red above 0 dB | Implemented | `OutputLed` | |
| V21 | Data stream in empty areas: ~10 lines, faded top/bottom two, light green, real data, stops when idle | Implemented | `DataStream`, `layoutStream` | |

## 7. JUCE guidelines (`JUCE_CLAUDE_GUIDELINES.md`, checkable items)

| # | Requirement | Status | Evidence | Suggested fix |
|---|---|---|---|---|
| J1 | Nothing on the audio thread allocates, locks or blocks | Implemented | review of `processBlock` / `Engine::process` | |
| J2 | `ScopedNoDenormals` in `processBlock` | Implemented | `PluginProcessor.cpp:242` | |
| J3 | DSP objects prepared and reset in `prepareToPlay` | Implemented | `Engine::prepare` | |
| J4 | APVTS with cached raw pointers and GUI attachments | Implemented | | |
| J5 | Smoothing on every gain or continuous parameter | Partial | see M4.2 | see M4.2 |
| J6 | Bus layouts validated | Implemented | | |
| J7 | Unused output channels cleared | Implemented | `PluginProcessor.cpp:319-320` | |
| J8 | Versioned state that round-trips | Implemented | `version`, `pluginVersion` | |
| J9 | Warning-free build at /W4; warnings as errors in CI | Partial | recommended warning flags; no /WX; no CI | add with CI (T32) |
| J10 | pluginval strictness 10 passes | Partial | 5 passed; 10 pending | |
| J11 | Static MSVC runtime; stable plugin codes | Implemented | CMakeLists | |

---

## 8. Docs and README claims the code contradicts

| # | Claim | Where | Reality |
|---|---|---|---|
| X1 | "Continuous parameters are smoothed" / "the engine smooths it" | Docs/MidiMap.md "Host automation"; Help "Gestures, MIDI and automation" | Dry/Wet, Output, Dry Trim, Studio Level, Hum, Ceiling, Studio stage gains and EQ step per block (M4.2) |
| X2 | "Switching never rewrites your knob values; effective limits are shown on the controls" | Help "Natural and Expressive" | effective limits are not shown; only an unlabelled dot on three controls (R1.5) |
| X3 | "Tail … With Studio off it is 0" | Docs/Routing.md last paragraph | tail follows the stage toggles even with Studio off (H13) |
| X4 | "Raw Hardware: W = C" | Docs/Routing.md diagram | not true while the Ghost Carrier is on (C4.13) |
| X5 | "Each bound is exercised by Tests/TestLimits.cpp" | `Source/Core/PhysicalLimits.h:6` | no such file; six bounds have no test (R2.12) |
| X6 | Ceiling "an explicit, audible soft ceiling" | Docs/Manual.md "Level guidance" | instant-attack gain rider plus hard clamp (D16) |
| X7 | README, CHANGELOG, Issues and Manual cite `Docs/QA.md` for results, CPU and unverified gates | README table, CHANGELOG release notes | QA.md is untracked and has only reproduction commands so far (M1.5h) |
| X8 | Test name `Engine.pairMatrixBothOrdersFiniteAndDistinct` | Tests/TestEngine.cpp | never asserts distinctness (M6.9) |
| X9 | Preset "Jack Pull Pop": "pulling the plug discharges it … Trigger A to play" | `FactoryPresets.cpp:163-168` | Slot A Gesture is Wiggle (0), not a pull; set a Pull lane step or describe it as a wiggle |
| X10 | "Details … calibration" | Help "The window", Manual | no calibration controls beyond trims and Chaos (M8.5) |

---

## 9. Status counts

Every requirement row in sections 1–7 is counted once (the section 8 contradictions are not counted separately):

| Status | Count |
|---|---|
| Implemented | 394 |
| Partial | 81 |
| Missing | 13 |
| Deviation | 4 |
| Not verifiable | 3 |
| N/A | 3 |
| **Total** | **498** |

---

## 10. Prioritized gaps

### P1: spec violations a user would notice, or missing required features

1. **Pops without a contact transition** (M4.22, R1.6, R2.8, R9.6). Changing Pop or Circuit Bias (knob or automation), or switching Natural↔Expressive, makes an audible pop on a closed, still contact. *Fix:* in `Source/DSP/ChargeCircuit.h:23-35`, high-pass the conductance only and multiply by a bias ramped over ~50 ms (`y = smooth(HP(g)) * biasRamp`). Callers are `LooseJack.h:112`, `PlugTouch.h:95` and `DirtyPot.h:138`. Add a test that steps Pop and policy on a seated jack and expects no transient. About 15 lines plus a test.
2. **Zipper noise on automated gains** (M4.2, H6, J5, X1). Dry/Wet, Dry Trim, Output, Studio Level, Ceiling and Hum/Buzz/RF in `Engine.h:234-269`, and Drive pre/post, stage wets and EQ coefficients in `StudioChain.h:102-202`, change in a step at each block boundary. *Fix:* per-sample one-pole or `SmoothedValue` ramps of 10–20 ms, using the existing `smoothCoeff`. Leave `mixTo`'s exact end points intact so the identity tests still hold once a ramp settles. About 40 lines.
3. **No effective values or visible "Limited by …" text** (R1.5, R3.8, R9.3, X2). `ParamKnob::setLimited` only draws a 5 px dot, and the reason never appears. `SourceTelemetry::effective/limited` is never shown. Excitation Level is silently capped at about −20 dB in Natural while the knob goes to −6 dB. *Fix:* in `Widgets.cpp`, draw the reason under the knob, add it to the tooltip, and draw an effective-value tick. In `Engine.h`/`SlotPanel.cpp`, flag the excitation cap ("Limited by Natural policy: −20 dB") and "Contact stable here". About 60 lines.
4. **Guided "Move toward worn area" action missing** (R3.9). *Fix:* add a button in the Why-quiet dialog or ContextPanel, shown when the jack is "Contact stable here", that ramps `jk_insert` toward `jk_wearpos` inside a begin/end host gesture (`SlotPanel.cpp`, about 25 lines).
5. **Per-stage Studio meters missing** (C3.5, C3.13). `EngineTelemetry::stageIn/stageOut` are declared (`Telemetry.h:28`) but never written or drawn. *Fix:* record per-stage input/output peaks in `StudioChain::process` (pass a telemetry pointer or local peak array that Engine stores after the block). Draw 7 small meters under the stage buttons in `StudioPanel::resized/paint`. About 50 lines.
6. **QA evidence is incomplete** (M1.5h, E2, D9, X7). `Docs/QA.md` is untracked and has commands only. *Fix:* add per-suite results, pluginval 10 output, CPU and latency figures, the ASAN run if done, the release ZIP checksum and the list of unverified gates (DAW load, calibration, sidechain in host). Commit it.
7. **Homepage and support email are placeholders** (G7, G8). `HelpContent.h:13-14` and `CMakeLists.txt` use `*.example`. *Fix:* the publisher supplies the real values. Until then, record this as an open release gate in Issues.md.

### P2: partial implementations

- Swap Order hidden in the Details drawer; the main view does not show the processing order; no per-slot audition (M6.5, M6.6, C4.2, W5). Add an order toggle and "A→B / B→A" label between the cards, and a Solo on each card.
- Excitation not visually distinguished outside the Create tab (R8.5, R9.9). Show an "EXCITATION ON" badge from `telemetry.excitationActive` and an excitation meter.
- Fresh one-shot workflow: no way to record a performed gesture for Render (W4). Add "record gestures to lane", or document the limitation.
- Feedback loop-gain estimate ignores the Auto Match gain inside the loop (C3.10). Include ×4 in `loopGain`, or move Auto Match after the tap.
- Audio-thread scheduling and sorting of lane events differs from the spec's "compile an immutable event table" wording (I5.3). Record it as a decision or precompile the table.
- Missing tests: UI dormant states and Why-quiet vs the engine (R10.5); six untested PhysicalLimits bounds (R2.12); source table rows only at 48 kHz/256 (R10.1); pair-matrix distinctness (M6.9); forward seek, stop/start, sample-rate switch, tempo change and odd time signatures (M7.17, T11, I5.6a/c); offline vs real-time agreement (T15); reopen audio identity (T13); DC input and sweeps (T1, T3); nonlinear-preamp counterfactuals (C7.3); Ceiling (H19, C3.17); Fill release (I5.6e).
- No CI workflow (T32, J9).
- Verification gates still open: pluginval strictness 10, the `--slow` CPU and soak run, the release ZIP, the ASAN run (M1.5c/i, T18, T21, T23).
- Details "advanced calibration" is thin (M8.5).
- Details meters: "through" shows R rather than the contact-affected path, and the Cable "electrical" meter includes the mechanical signal (R9.10).
- Tail reported with Studio off (H13, X3).

### P3: polish and documentation

- Touch illustration field uses the Hum parameter and animates constantly (M8.10, R9.8).
- Illustrations have no tooltip and no right-click menu (M8.7, R9.7, G19); focus order is implicit.
- "Motion limit reached" is drawn for Loose Jack only (I2.11).
- No documented maximum combined cycle (I3.2); no position-in-cycle indicator (I3.4); no labelled Gap step (I3.6).
- More greyed-out controls with reasons (R9.2); Gate no-clock status (C3.6); the feedback path is missing from the RoutingView text (C3.9); EQ coefficient smoothing (C3.1).
- Ghost Carrier applied after Raw Hardware (C4.13, X4).
- theme.md: no 80 ms value animation (V13), no resize cursor (V14), no slider ticks (V8), boxes inside boxes (V12).
- Docs: the stale `Tests/TestLimits.cpp` comment (X5); "soft ceiling" wording (X6); "Jack Pull Pop" preset uses Wiggle (X9); industrial/feedback research provenance missing from Research.md (I6.1); freeze behaviour of tails undocumented (C3.12); licence-activation extension point (D19); MIDI import/export N/A decision (G18); PROGRESS.md stale (G23); Studio stage help section (G1).
- Test extras: Passive-vs-Line response (W1); silence invariant over Tone circuit and Expressive (R2.2); linked-pop correlation (R7.5); stereo phase (C7.9); abrupt mid-run parameter changes (C7.8); full layout × tap matrix (C7.1).

---

## 11. Round 2 UI and doc fixes (2026-09-23; written, not yet compiled)

| Item | Change | Files |
|---|---|---|
| P1.3 / R1.5 / R3.8 / R9.3 / X2 | `ParamKnob::setLimited` now draws the reason in the readout row (yellow, whenever the knob is not being touched) and adds "Limited: …" to the tooltip. New `ParamKnob::setEffective` draws a yellow tick on the arc at the effective value (in `paintOverChildren`). Wired: Excitation Level (reads `telemetry.excitationLimited` / `excitationEffectiveDb`, "Limited by Natural/Expressive cap"), Feedback ("Limited by loop stability", tick at `fbEffective`), jack Wiggle and Intermittency ("Contact stable here"; Intermittency sits in Details, so the old check never reached it), pot Scratch, Cable Shake and Mic | Widgets.h/.cpp, SlotPanel.cpp, StudioPanel.cpp |
| P1.4 / R3.9 | "Move toward worn area" button in the Loose Jack context panel (highlighted when the engine reports a stable healthy contact), and as a second button in that slot's Why-quiet dialog. It ramps `jk_insert` to `jk_wearpos` over about 0.6 s inside one begin/end host gesture | SlotPanel.h/.cpp, MainView.cpp |
| P1.5 / C3.5 / C3.13 | `StageMeter` under each of the seven stage buttons: upper bar input peak, lower bar output peak, read from `telemetry.stageIn/stageOut[0..6]` | StudioPanel.h/.cpp |
| P2 M6.5 / C4.2 | Order button between the slot cards bound to `swap` ("A → B" / "B → A", with an "order" caption); RoutingView shows the live order | MainView.h/.cpp, StudioPanel.cpp |
| P2 R8.5 / R9.9 | "Excitation on: <type> <level> dB (capped)" badge next to the bottom tabs, driven by `telemetry.excitationActive`; clicking it opens Create | MainView.h/.cpp |
| P3 M8.7 / R9.7 / G19 | Illustrations: tooltip per source; right-click opens the primary parameter's menu (drags and mouse-up of that click are ignored) | Illustrations.h/.cpp |
| P3 I2.11 | "motion limit reached" drawn for every source, top left of the illustration | Illustrations.cpp |
| P3 M8.10 / R9.8 | Touch field intensity now comes from the measured injected level only; the rings stand still when nothing is picked up | Illustrations.cpp |
| P3 C3.9 | RoutingView flow text now lists the feedback path and its amount (and the clamp) | StudioPanel.cpp |
| P3 C3.6 | Studio note: "Gate: no clock, stays open" when a steps-keyed Pulse gate has no clock | StudioPanel.cpp |
| P3 I3.2 / I3.4 / I3.6 | Lane clock line: "cycle n / N" while running; step-menu "Gap" item for jack and pot lanes (Pull or Turn to Angle, 0 % target, 1 step); maximum combined cycle documented | LaneEditor.cpp, Manual.md, HelpContent.cpp |
| G1 | Help "Studio chain and routing" now explains every stage, the stage meters, Raw Hardware, and the Ceiling as an instant-attack linked rider with ~45 ms release plus a final clamp | HelpContent.cpp |
| X1 | MidiMap.md and Help describe the real automation handling (gains smoothed ~10 ms, positions moved through the mechanics, switches at the block edge) | MidiMap.md, HelpContent.cpp |
| X3 / C3.12 | Routing.md: tail is 0 with Studio off or Raw Hardware on; host freeze/bounce behaviour added | Routing.md |
| X4 / C4.13 | Routing.md and Help: Raw Hardware also bypasses the Ghost Carrier (W = C) | Routing.md, HelpContent.cpp |
| X5 | PhysicalLimits.h comment points at Tests/TestLimits.cpp plus the existing TestCore/TestSources/TestEngine bound tests | PhysicalLimits.h |
| X6 | Manual and Routing.md describe the Ceiling as a peak ceiling (instant attack, ~45 ms release, final clamp), no longer "soft" | Manual.md, Routing.md |
| X9 | "Jack Pull Pop" now has a lane step with the Pull gesture (played by the one-shot or note 38); the description matches | FactoryPresets.cpp |
| X10 | "calibration" removed from the Details descriptions (Help, Manual, Details tooltip) | HelpContent.cpp, Manual.md, SlotPanel.cpp |
| P1.7 | Placeholder homepage/e-mail recorded as release gate O8 | Issues.md |
| W4 | Render does not capture live gestures: documented (Manual, Issues O9) | Manual.md, Issues.md |
| Crash log | Help and Issues O7: fast-fail terminations and host-caught crashes cannot be logged | HelpContent.cpp, Issues.md |
| Params | Parameters.md regenerated with `tools/gen_param_doc.py` (150 parameters) | Parameters.md |

X8 (notes only, test files not edited): `Engine.pairMatrixBothOrdersFiniteAndDistinct` should either assert, for each pair, that
swapping the order changes the output (or the per-slot event telemetry), or be renamed `…FiniteAndBounded`.

Still open from this round: Details "through"/"electrical" meters need per-slot C0 and electrical-only peaks from
the engine (Issues O10). Per-slot Solo was skipped: it cannot be done without a DSP change or without rewriting the
other slot's enable parameter behind the host's back (Issues O11). Knobs now show the vertical-resize cursor; the rest of the theme.md polish (80 ms value animation,
slider ticks) is not done.

Addendum (same round): the 10 new environment parameters (commit 3c9d96a) get a fifth bottom tab, **Environment**
(`EnvironmentPanel` in StudioPanel.h/.cpp, wired in MainView). It has two rows. Row 1: Mains, Hum, Buzz Band,
Buzz Source, Dimmer, Dimmer Kind, Interference, 2G Phone, Distance. Row 2: Other Gear, Outlets, Cable Run, Cable
Shield, Interface, and the measured "Ground loop: n dBFS" readout from `telemetry.groundLoopDb`. Controls are greyed
with a reason: Dimmer and Dimmer Kind when Buzz Band is 0 or the source is Supply; Buzz Source when Buzz Band is 0;
the loop controls when Other Gear is Off; Outlets when the gear is 2-prong (the engine uses Outlets only for Grounded
gear); Distance when the phone is Away. `ParamChoice::setDormant` was added for this. Docs: Manual "Environment: hum,
ground loop, dimmer, phone", a matching Help section, a Routing.md note on where the loop and phone enter, and
Parameters.md regenerated. I used a tab rather than the Details drawer, which was already full.

---

## 12. Final status (2026-09-24, HEAD 8687cb1 plus the reviewer's uncommitted UI changes)

Every Partial and Missing row above was re-checked against the committed code and the working tree.
Key: **(a)** done; **(b)** documented deviation or decision; **(c)** not verifiable here, or a release action
or gate in `Docs/QA.md`; **(d)** still missing. Row M1.5a (a working VST3) now builds (QA.md) and counts as
Implemented. The other Deviation, Not verifiable and N/A rows are unchanged.

| Status | Round 1 | Final |
|---|---|---|
| Implemented | 394 | 456 |
| Partial | 81 | 16 |
| Missing | 13 | 2 |
| Deviation | 4 | 9 |
| Not verifiable | 3 | 12 |
| N/A | 3 | 3 |
| **Total** | **498** | **498** |

Of the 94 rows that were Partial or Missing: (a) 61 done, (b) 5 documented, (c) 10 not verifiable here or a pending release action, (d) 18 still missing.

| # | Round-1 status | Final | Evidence / reason |
|---|---|---|---|
| M1.5c | Partial | (c) | pluginval strictness 10 is a local run; QA.md "Validation" still says "recorded below" with no result yet |
| M1.5h | Partial | (a) | QA.md now has environment, results, CPU, soak and unverified gates (bce7516) |
| M1.5i | Partial | (c) | tools/make_release.ps1 runs in CI (ZIP + .sha256 uploaded); the local checksum is not yet recorded in QA.md |
| M4.2 | Partial | (a) | Engine.h:431-442 per-sample smoothing of dry/wet, output, Studio level, ceiling, pre-drive, hum/buzz/RF; StudioChain.h:238-242 stage wets and drive gains; Scenarios.abruptGainAndRoutingChangesAreDeclicked |
| M4.22 | Partial | (a) | ChargeCircuit.h: the high-pass acts on g only, bias smoothed over 50 ms; Scenarios.policySwitchOnStillHardwareIsSmoothAndEventFree |
| M6.5 | Partial | (a) | order button between the slot cards (MainView), live order in RoutingView |
| M6.6 | Partial | (b) | Issues.md O11 (no per-slot Solo; use A/B compare or the slot On buttons) |
| M6.9 | Partial | (a) | Scenarios.everyPairSoundsDifferentInEachOrder |
| M7.17 | Partial | (a) | Scenarios.forwardSeekRestartsExactlyLikeAFreshStart, stopThenStartReplaysLockedTake..., sampleRateSwitchLeavesNoTraceOfTheOldRate |
| M7.18 | Partial | (a) | MidiMap.md and Help describe the per-parameter automation handling; matches M4.2 |
| M8.5 | Partial | (d) | no calibration view and no DECISIONS entry (see the (d) list) |
| M8.7 | Partial | (d) | focus order still implicit: no setExplicitFocusOrder in Source/UI (see the (d) list) |
| M8.10 | Partial | (a) | Touch field follows the measured pickup; the rings stand still at zero pickup (Illustrations.cpp) |
| W4 | Partial | (b) | Issues.md O9 + Manual "Render and record": Render replays lane and gestures; Record captures live gestures |
| W5 | Partial | (b) | order button done; the audition part is Issues.md O11 |
| H6 | Partial | (a) | see M4.2 |
| H13 | Partial | (a) | Engine.h:777: tail is 0 with Studio off or Raw Hardware on |
| H19 | Partial | (a) | Scenarios.ceilingHoldsOversLinkedAndReleases |
| T1 | Partial | (a) | Scenarios.dcInputPassesHealthyHardwareSteadily, logSweepThroughHealthyAndFaultyRigs |
| T3 | Missing | (a) | Scenarios.injectedArtifactsCarryNoDc, dcInputPassesHealthyHardwareSteadily |
| T7 | Partial | (a) | Scenarios.forwardSeekRestartsExactlyLikeAFreshStart |
| T10 | Partial | (a) | Scenarios.everyPairSoundsDifferentInEachOrder |
| T11 | Partial | (a) | Scenarios.tempoChangesKeepLaneGesturesOnTheGrid |
| T13 | Partial | (a) | by composition: Host.stateRoundTripThroughHostBlob (exact state) + locked replay tests (stopThenStart..., lockedSeedBounceIsNullIdentical); DAW reopen is QA gate 1 |
| T15 | Missing | (a) | Scenarios.offlineRenderMatchesTheRealTimeEngine |
| T18 | Partial | (c) | pluginval 10: a local run whose result is not yet in QA.md |
| T21 | Partial | (c) | QA.md unverified gate 6 (sanitizer build not run on the 4 GB machine) |
| T22 | Partial | (a) | Scenarios.randomParameterJumpsEveryBlockStayFiniteAndBounded + Gui.concurrentAudioAndGuiActions |
| T23 | Partial | (a) | QA.md "Soak (offline)": 60 min in 437 s, no drift, PASS; the real-time host soak is gate 2 |
| T24 | Partial | (c) | plugin scan = the pluginval run (T18) and the DAW scan (gate 1) |
| T29 | Partial | (d) | not listed as an unverified gate in QA.md (see the (d) list) |
| T31 | Missing | (d) | no accessibility test (see the (d) list) |
| T32 | Partial | (a) | .github/workflows/ci.yml |
| D9 | Partial | (a) | QA.md CPU table (44.1-192 kHz) and the Manual compatibility row |
| D19 | Partial | (d) | no extension-point note in DECISIONS.md or the docs (see the (d) list) |
| E2 | Partial | (c) | QA.md is complete except the pluginval 10 and release-ZIP records (see M1.5c / M1.5i) |
| R1.5 | Partial | (a) | ParamKnob Limited text, tooltip and effective tick; excitation, feedback, contact-stable and line-source hints |
| R1.6 | Missing | (a) | Scenarios.policySwitchOnStillHardwareIsSmoothAndEventFree |
| R2.8 | Partial | (a) | ChargeCircuit change, see M4.22 |
| R2.12 | Partial | (a) | Tests/TestLimits.cpp covers the six previously untested bounds |
| R3.8 | Partial | (a) | "Contact stable here" drawn as text on Wiggle and Intermittency |
| R3.9 | Missing | (a) | "Move toward worn area" button and Why-quiet action (SlotPanel.cpp) |
| R7.5 | Partial | (d) | no test that linked-gesture pops are correlated (see the (d) list) |
| R8.5 | Partial | (a) | "Excitation on" badge (MainView) |
| R9.2 | Partial | (d) | only environment and circuit controls are greyed; dependent Studio stage controls are not (see the (d) list) |
| R9.3 | Partial | (a) | see R1.5 |
| R9.6 | Partial | (a) | see R1.6 |
| R9.7 | Partial | (a) | illustration tooltips (Illustrations.cpp) |
| R9.8 | Partial | (a) | see M8.10 |
| R9.9 | Partial | (a) | policy in the mode row, Mic knob, Excitation badge |
| R9.10 | Partial | (b) | Issues.md O10 (through meter shows R; the cable electrical meter includes mechanical) |
| R10.1 | Partial | (d) | source-table rows are still tested only at 48 kHz / 256 (see the (d) list) |
| R10.3 | Partial | (a) | see M6.9 |
| R10.5 | Missing | (d) | no GUI test calls explainQuiet or checks dormant states (see the (d) list) |
| C1.3 | Partial | (c) | QA.md unverified gate 3 |
| C3.1 | Partial | (d) | EQ coefficients are still recomputed per block without smoothing (StudioChain.h setParams) (see the (d) list) |
| C3.5 | Partial | (a) | stage meters (StudioPanel) from telemetry.stageIn/stageOut (Engine.h:779) |
| C3.6 | Partial | (a) | Studio note "Gate: no clock, stays open" |
| C3.9 | Partial | (a) | RoutingView lists the feedback path and amount |
| C3.10 | Partial | (a) | StudioChain.h:191: loop gain x4 with Auto Match |
| C3.12 | Partial | (a) | Routing.md "Host freeze and offline bounce" |
| C3.13 | Partial | (a) | see C3.5 |
| C3.17 | Missing | (a) | Scenarios.ceilingHoldsOversLinkedAndReleases |
| C4.2 | Partial | (a) | see M6.5 |
| C4.13 | Partial | (a) | Engine.h:660-662: Ghost is applied before the Raw Hardware blend |
| C7.1 | Partial | (d) | no single layout x tap matrix test (see the (d) list) |
| C7.3 | Missing | (a) | Scenarios.inputDriveKeepsCounterfactualBusesHonest |
| C7.8 | Partial | (a) | Scenarios.randomParameterJumpsEveryBlockStayFiniteAndBounded |
| C7.9 | Missing | (a) | Scenarios.stereoDifferenceZeroKeepsChannelsIdentical |
| C7.13 | Partial | (a) | see T13 |
| I2.11 | Partial | (a) | "motion limit reached" drawn for every source (Illustrations.cpp) |
| I3.2 | Missing | (a) | Manual and Help: maximum combined cycle 64 x 63 = 4032 steps |
| I3.4 | Partial | (a) | lane clock line "cycle n / N" (LaneEditor.cpp) |
| I3.6 | Partial | (a) | step-menu "Gap" item for jack and pot lanes (LaneEditor.cpp) |
| I5.3 | Partial | (d) | audio-thread scheduling and sort not recorded in DECISIONS.md (see the (d) list) |
| I5.6a | Missing | (a) | Scenarios.oddTimeSignaturesPutBarsOnTheirOwnGrid |
| I5.6c | Missing | (a) | Scenarios.tempoChangesKeepLaneGesturesOnTheGrid |
| I5.6e | Partial | (a) | Scenarios.fillReturnsToTheBasePatternOnRelease |
| I5.6f | Partial | (a) | see T13 |
| I6.1 | Partial | (a) | Research.md: SOS NIN articles, NIME 2021, DAFx feedback paper |
| G1 | Partial | (a) | Help "Studio chain and routing" explains every stage |
| G7 | Partial | (c) | QA.md unverified gate 5, Issues.md O8 (the publisher must supply it) |
| G8 | Partial | (c) | QA.md unverified gate 5, Issues.md O8 |
| G19 | Partial | (d) | mode buttons, slot Play (trigA/B parameters), Lock and New Take have no right-click menu (see the (d) list) |
| G23 | Partial | (d) | PROGRESS.md still stops at session 1 (see the (d) list) |
| V2 | Partial | (d) | JUCE has no weight 500; not recorded as a decision (see the (d) list) |
| V5 | Partial | (d) | knobs scale with their cells; not recorded as a decision (see the (d) list) |
| V8 | Partial | (a) | Theme.cpp drawLinearSlider ticks |
| V12 | Partial | (d) | slot cards and illustrations are bordered panels; not recorded as a decision (see the (d) list) |
| V13 | Missing | (b) | DECISIONS.md: the 80 ms value animation is not implemented for 1.0 |
| V14 | Partial | (a) | ParamKnob up/down resize cursor |
| J5 | Partial | (a) | see M4.2 (EQ coefficients excepted: C3.1) |
| J9 | Partial | (d) | CI does not build with warnings as errors (see the (d) list) |
| J10 | Partial | (c) | see T18 |

### (d) still missing, prioritised

**P2 (behaviour or verification the spec asks for)**
1. R10.5 / T31: no GUI test of the Why-quiet texts, the greyed (dormant) states or accessibility (focus order,
   titles, tooltips). Fix: add to `Tests/TestGui.cpp`:
   - set up a still pot, Mic 0, a shielded touch and a line-source cable, then assert `explainQuiet` strings and
     `setDormant` flags;
   - walk the focus chain and assert that each focusable component has a title and a tooltip.
   About 60 lines.
2. M8.7: the keyboard focus order is implicit. Fix: `setExplicitFocusOrder` in `MainView` (header → mode row → slot
   cards → illustrations → context panel → tabs → output strip) and in `ContextPanel`. About 20 lines.
3. C3.1: Studio EQ coefficients are recomputed once per block, so automating EQ can zipper. Fix in
   `StudioChain.h`: smooth HP/LP/tilt/peak frequency and gain per sample (or recompute coefficients every 32
   samples from smoothed values). About 30 lines, plus a test in `TestScenarios`.
4. R10.1: the per-source realism-table tests run only at 48 kHz / 256. Fix: parametrise the `Single` harness in
   `Tests/TestSources.cpp` over {44.1 k, 96 k} × {64, 1024}. About 20 lines.

**P3 (polish)**
5. R9.2: dependent Studio controls are not greyed. Echo Time (Sync on), Echo Division (Sync off), Res Freq (Follow
   MIDI on), Gate Division (Key not Steps) and the stage parameters while their stage or Studio is off should be
   greyed with a reason. Fix: `StudioPanel.cpp` `setDormant` calls in `refresh()`.
6. G19: no right-click menu on the mode buttons (parameter `mode`) or on the slot Play buttons (`trigA`/`trigB`).
   Fix: call `showParamMenu` on popup-menu clicks in `MainView` and `SlotCard`.
7. R7.5: no test that one linked gesture gives correlated contact transitions in both slots. Fix: add a test in
   `TestScenarios` with `link` on.
8. C7.1: no single test runs every monitor tap in both mono and stereo. Fix: a loop over layouts and taps in
   `TestEngine`.
9. J9: CI does not treat warnings as errors. Fix: add `-DCMAKE_COMPILE_WARNING_AS_ERROR=ON` to the configure step
   in `.github/workflows/ci.yml`, once the build is warning-free.

**Records only (no code)**
10. I5.3, D19, M8.5, V2, V5, V12: add one line each to `DECISIONS.md`:
    - lane events are scheduled per block on the audio thread (bounded, allocation-free) instead of a precompiled
      table;
    - no licence activation, with the extension point named;
    - "advanced calibration" is fixed in `PhysicalLimits.h`, with no UI;
    - JUCE fonts have no weight 500;
    - knobs scale with their cells;
    - slot cards are bordered panels.
11. T29: add "low disk space: error path only, not tested" to the QA.md unverified gates.
12. G23: bring `PROGRESS.md` up to date or point it at `TODO.md`.

Not in this list, because they are pending release actions rather than gaps: recording the pluginval
strictness-10 result and the release ZIP SHA-256 in `Docs/QA.md` (M1.5c, M1.5i, T18, T24, J10, E2).
