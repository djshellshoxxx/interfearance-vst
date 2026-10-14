# Spec E — Playability and fun (N1, N2, N4)

Facts: `Pattern` = 2 lanes × up to 64 `LaneStep`s (`active, gesture 0..3, accent, ratchet 1..4, choke, intensity, duration, target,
send, probability, offset`), scheduled per block by `scheduleLane()` as a pure function of pattern + musical position (Decisions.md);
RNG sub-streams re-keyed from `(seed, source, slot, bar)` while the transport plays; Evolve/Density/Fill/Swing/Humanize exist.
MIDI mapping per `Docs/MidiMap.md`.

---
## N1 — Gremlin mode ("fault roulette")

A bounded, seeded, *physical* meddler: on a schedule it moves hardware controls as if someone were fiddling with the rig.

### Model
* Parameters (`ParamsExt/gremlin.def`): `gr_on` (off), `gr_rate` (0–1 → 0.1–8 events/bar), `gr_depth` (0–1), `gr_scope` (choice:
  `Slot A|Slot B|Both|Environment|Everything`), `gr_style` (choice: `Fidget|Bump|Tug|Storm`), `gr_hold` (0–1: how long a
  disturbance lasts, bars).
* Implemented as a **`GremlinStage` in `ParamPipeline` (F0.3)**: it adds a *position/force offset* to hardware controls only
  (Move Plug, Wiggle, Touch, Bend, Shake, Turn Knob, Hum, RF level), never to circuit/studio/output parameters. So all outputs
  remain physically caused, still limited by `PhysicalLimits` and the Natural/Expressive policy.
* **Determinism**: events derived from `hash(seed, take, bar, eventIndex)` exactly like lane scheduling; same transport position +
  state → same result for any block size (tests compare 64/512/4096 blocks). With transport stopped, events use the internal clock (`runInternal`).
* Event shapes: raised-cosine pulses with attack 20–120 ms and release by `gr_hold`; `Storm` = 4–8 overlapping events;
  `Tug` = slow ramp then fast release (cable pull); `Bump` = single impulse.
* Safety: `gr_depth` maps to ≤ 60 % of each control's travel in Natural; total simultaneous offset limited so mixes never exceed
  `PhysicalLimits` peaks; `Kill` (existing "Kill Tail"/panic) also zeroes Gremlin offsets over 50 ms.
* UI: "GREMLIN" toggle with a small animated eye in the header; effective positions drawn as ghost ticks on knobs (F0.3).
* MIDI: note 64 = trigger one gremlin event now; CC 17 = depth (added to MidiMap.md; not conflicting with learned CCs).
* Tests: `gr.offIsBitExact`, `gr.deterministicAcrossBlockSizes`, `gr.onlyHardwareControlsMove`, `gr.boundedByLimits`,
  `gr.seedChangesEvents`, `gr.stoppedTransportUsesInternalClock`, `gr.killSilencesOffsets`, soak 10 min.
* Acceptance: a 60 s render with Gremlin `Fidget` produces event rate within [0.5, 8]/bar of `gr_rate` and no pop on a healthy shielded
  source (still zero sound if nothing can physically fault).

---
## N2 — Tempo-synced pattern generators and lane library

### Generators — `Core/PatternGen.h` (pure, deterministic)
* `euclid(k, n, rotation)` (Bjorklund), `polyrhythm(a, b)` (two lanes with lengths a and b), `bounce(n)` (accelerating ratchets),
  `stutter(n)`, `dyingContact(n)` (probability ramps 1→0 with growing intensity), `brownian(n, seed)` random walk of intensity,
  `mirror/reverse/rotate/thin(p)/densify(p)` operators, `humanise(seed, amount)` (offset/intensity jitter).
* All take `(PatternGenParams, seed)` and write into a `Lane` using `LaneStep` fields only (no format change).

### Lane library
* 24 factory lane patterns in `Core/LaneLibrary.cpp` (names, tags: `rhythmic`, `chatter`, `decay`, `sparse`). User lanes saved as
  `.ifxlane` (a `Lane` or both via `StateIO::patternToTree`), loaded through the lane editor's new "Patterns ▾" menu (Insert / Replace /
  Save current / Open folder). Drag a `.ifxlane` file onto the editor to load.
* Divisions: append choices to `laneDiv`/`gt_div`/`ec_div` only if absolutely needed (`1/2`, `1/1`, `1/8D`, `1/16D`) — append at the end
  of the choice string; existing indices unchanged; engine's step-length table gets matching rows; tests for musical position math.
* Lane editor additions (`LaneEditor.cpp`): Euclid dialog (k, n, rotation sliders with live preview), "Humanise" slider, "Variation"
  button (applies mutate-style randomness through `PatternGen` using the lane's own seed), per-lane **length different from the
  other lane** already supported (polymeter) — expose with presets.
* Tests: `patgen.euclidKnownVectors` (E(3,8)=x..x..x., E(5,8), E(7,16)), `patgen.deterministic`, `patgen.operatorsPreserveLength`,
  `patgen.stepFieldsInRange`, `lane.libraryAllLoadAndSchedule`, `lane.fileRoundTrip`, `lane.extraDivisionsPositionMath`.
* Acceptance: any library pattern at any division schedules identically for 64/512/4096-sample blocks and across seek/loop (existing
  `TestPattern` determinism rows applied to every entry automatically).

---
## N4 — MIDI performance mode

Existing mapping is fixed (note ranges, velocity, mod wheel, aftertouch, learned CC). Adds an **editable performance map** and
pitch-bend / MPE support.

### Performance map (non-parameter state saved in preset + project)
`PerformanceMap` = list of `{source: MidiSource, target: PerfTarget, amount, curve, slot}` with
`MidiSource ∈ {Velocity, PitchBend, ChannelPressure, PolyPressure, ModWheel, CC(n), NoteNumber, MpeSlide(CC74), MpeLift}` and
`PerfTarget ∈ {GestureIntensity, MovePlug, Bend, TurnKnob, Touch, TouchPressure, Wiggle, Shake, Wear, Hum}`. Default map = today's
behaviour (velocity→intensity, mod wheel→random motion, pressure→pressure) plus **PitchBend → primary position** of the slot's
source (Move Plug / Bend / Turn Knob / Touch). Targets are *hardware-position inputs* (like `GestureOut`), so physics and limits apply.
* New hand-off: translated in `translateMidi()` (message→audio thread already in place); results are written into the existing
  per-block `ControlEvent` stream (`kMaxEvents 1024`, no new FIFO) as `ControlEvent::Continuous{target, value}`.
* **MPE**: zone detection by MCM (RPN 6) or option "Treat channels 2–16 as MPE"; each member channel gets an independent
  *voice* {note, slide, pressure, bend} up to 4 voices mapped round-robin to slot A/B (A,B,A,B) as extra gesture instances; voices
  past 4 steal the oldest. Each voice's continuous controls drive the same targets with per-voice smoothing (5 ms). The slots'
  mechanical state remains shared (physical constraint): voices *add* disturbance, not new hardware.
* UI: Options → "MIDI Performance" overlay `UI/PerformanceMapOverlay.cpp` (table editor, learn buttons, curve preview, reset to default,
  per-row enable); MIDI learn for rows ("move controller" fills `source`).
* Docs: `MidiMap.md` section generated from the default map (single source of truth `Core/PerformanceMap.h`).
* Tests: `perf.defaultMapMatchesLegacyBehaviour` (render comparison against pre-change golden), `perf.pitchBendMovesPosition`,
  `perf.mpeVoicesBounded`, `perf.noAllocationInTranslate`, `perf.mapRoundTrip`, `perf.limitsStillApply`, fuzz random MIDI streams.
* Acceptance: 10 minutes of dense random MIDI (all message types, 16 channels) produce no NaN, no allocation, CPU delta < 1 %.
