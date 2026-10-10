# Spec C — Sound engine features (V4, D3, D4)

Engine facts (from code): sources implement `Source` (`prepare/reset/seed/setParams/control/process`, `tele`), are held in
`Engine::sources[kNumSourceTypes+1]`, and `process()` runs **three counterfactual paths per sample** — R (healthy reference),
C0 (actual contacts, no injection), C (complete) — so `C = R + D + A` exactly (Decisions.md). `SourceType` is an int stored in
state (`slotA/slotB`), values 1–4 in use. Latency is 0 by design (ADAA, no lookahead).

---
## V4 — New fault types

### Prerequisite: F0.4 `SourceRegistry`
No new source starts before the registry exists. A source = one descriptor entry:
```cpp
struct SourceDesc { SourceType type; const char* name; const char* cause;      // one-line physical cause (UI, help)
                    GestureKind gestures[3];                                    // Machine Pattern gestures
                    std::unique_ptr<Source> (*make)();                          // factory
                    WearMap::Settings (*wear)(uint64_t mapSeed, const ParamSnapshot&);
                    std::initializer_list<int> primaryParams;                   // the 4 primary controls for ContextPanel
                    const char* helpAnchor; };
```
New `SourceType` values are **appended** (5, 6, 7, 8); `PluginState` already clamps slot values via `clampInt` — extend the clamp
to `kNumSourceTypes` from the registry. Old builds loading a state with an unknown type fall back to `None` (documented).

### Research gate (applies to each source)
Before DSP code: a 1–2 page note appended to `Docs/Research.md` ("Cause → mechanism → audible result → limits") with ≥ 3
citations (abstract level is acceptable, state which), the **quiet conditions** (what makes it silent), and the **realism
rows** as a table. Realism rows become `Tests/Sources<Name>.cpp` tests exactly like `TestSources.cpp` (healthy+still+shielded
= silent; no sound without a physical cause; gestures bounded by `PhysicalLimits`; stereo shares the mechanical state).

### Common deliverables per source
Params block `ParamsExt/<src>.def` (7–10 params, defaults = harmless), DSP `DSP/<Name>.h`, illustration in `UI/Illustrations.cpp`
(new class, registry-mounted), `QuietReason` mappings, Help text, 4 factory presets, Manual section, tests, TestPerf row (each
source ≤ 6 % of one core worst case at 48 kHz), `changelog.d/v4-<name>.md`. Each source is one PR (≤ 800 lines + tests).

### V4.1 Cold Solder Joint (`SourceType::SolderJoint = 5`)
Cause: a cracked joint on a PCB or jack lug; contact is mechanical-vibration- and temperature-sensitive.
* State: `crack` (0–1 static, from Wear map), `thermal` T (slow, τ ≈ 3 s, driven by program level RMS and the Warm gesture),
  `vib` (from gestures Tap/Shake and cabinet-vibration input).
* Model: reuse `ContactModel` with hazard `h = base(crack) · (0.2 + vib²) · (1 + k·(T−T0))`; in `Resistive` state series
  resistance `Rc = Rmin · (1 + 40·crack·noise1/f)` gives crackle (1/f contact noise via `Filters.h` pink filter);
  `Open` interrupts signal and discharges bias (pop, same as Loose Jack).
* Params: `sj_crack`, `sj_vibration`, `sj_thermal`, `sj_recovery`, `sj_pop`, `sj_shield`, `sj_pos` (where in the chain: input / output).
* Gestures: Tap (knock), Warm (raise T over 2 s), Cool.
* Quiet: crack = 0, vib = 0 (and no warm) = silent ("Joint is sound / nothing is shaking it").
* Tests: hazard monotone in vib; thermal hysteresis (heat 5 s then cool: events persist ≥ 2 s); open → pop only with bias.

### V4.2 Failing Coupling Capacitor (`SourceType::BadCap = 6`)
Cause: electrolytic coupling cap with dried electrolyte: lower capacitance, higher ESR, leakage.
* Equivalent circuit: series C (blocking), ESR in series, leakage R_leak in parallel with C; optional DC bias from previous stage.
* Audible: rising high-pass corner `fc = 1/(2π·(R_in+ESR)·C_eff)` (bass loss), slight level loss from ESR/R_in divider, leakage
  → DC offset that *moves* with temperature → slow thump on level changes, microphonic crackle when physically knocked (Tap),
  reforming effect (after a long silence the cap "recovers" over ~10 s → warm-up swell).
* Params: `bc_age` (0–1 → C_eff 100%→25%, ESR ×1→×20, leak ×1→×200), `bc_bias`, `bc_mic` (microphonic), `bc_temp`, `bc_value` (nominal
  0.1–220 µF log), `bc_recover`, `bc_load` (stage input impedance).
* Quiet: age 0 = transparent (exact identity at default); with age > 0 and zero bias/mic it is only a filter (never noise).
* Tests: corner frequency matches formula ±5 %; identity at age 0 (bit-exact); no noise without bias or mic; reforming
  time constant; DC leakage bounded by `PhysicalLimits`.

### V4.3 Corroded Patch-Bay Contact (`SourceType::PatchBay = 7`)
Cause: oxidised normalling contacts in a TT/bantam patch bay; insertion movement scrapes the oxide.
* Model: contact resistance `R = R0 + Rox·(1−s)` where `s` = scrape state raised by insertion movement and decaying slowly
  (oxide regrows), noise = thermal + avalanche-like bursts when current density through a tiny contact exceeds threshold
  (level-dependent: louder signal "burns through" and the contact improves → **level-dependent clearing**, a distinctive behaviour).
* Params: `pb_oxide`, `pb_scrape`, `pb_regrow`, `pb_level_clear`, `pb_normal` (normalled/half-normalled contact on/off), `pb_dc`.
* Gestures: Insert/Pull (moves `s`), Tap, Hold. Quiet: oxide 0 or no movement and signal below burn-through threshold.
* Tests: burn-through clears resistance monotonically; regrow time; no event without movement when level low.

### V4.4 Dirty Rotary Switch (`SourceType::DirtySwitch = 8`)
Cause: multi-position rotary or toggle switch with dirty detents; transitions bounce.
* Model: N detents (2–12), each with contact quality from the Wear map; moving between detents produces bounce bursts
  (2–8 chatter events, 0.2–3 ms apart, exponential decay) via `ContactModel::beginExcursion`; between detents (break-before-make)
  the circuit is open for `gap` ms (signal gap + pop). Selected detent chooses one of two tone networks (Level / Tone circuit
  reusing `DirtyPot`'s RC models).
* Params: `sw_pos` (continuous 0–1 → detent index), `sw_detents`, `sw_dirt`, `sw_bounce`, `sw_gap`, `sw_circuit`, `sw_pop`.
* Gestures: Click (jump one detent), Sweep, Hold. Quiet: still switch = steady tone network, no noise.
* Tests: bounce count within `PhysicalLimits`, gap ≥ 0 produces silence of the right length, steady state silent.

### V4 acceptance
Each source: realism rows pass, 10-minute randomised soak passes, CPU ≤ 6 %, fixtures for old projects unchanged (types 1–4), registry
test iterates over every descriptor automatically (so a missing illustration/help/quiet mapping fails the build).

---
## D3 — Light mode and exact CPU savings

### D3.1 Exact optimisation (default on, bit-identical)
Today all three counterfactual paths run. R and C0 are needed only when a consumer uses them: Monitor taps ≠ Full Output,
Studio source Artifacts/Breaks/Both (A = C−C0, D = C0−R), telemetry-driven UI (quiet reasons, Illustrations), or the live recorder
tap. Add `Engine::NeededPaths needed` computed per block from the snapshot: `{R, C0}` flags. When both false, skip those two
paths **and** keep `C` bit-identical (C is computed from its own state; spec requires verifying the `C` path never reads R/C0
results — see `Engine.h` ~L532–612; if it does, restructure first). The editor's telemetry (quiet reasons, illustrations) reads R/C0 results, so *decision*: the exact skip applies only while the
editor is closed (typical in a finished mix) or Light mode (D3.2) is on; with the editor open all paths run.
* Test `light.exactWhenEditorClosed`: render 10 scenarios with paths on vs skipped, compare output bit-for-bit.

### D3.2 Light mode (user option, in Options, per-install)
* UI: 15 Hz timer, scope/spectrum off, illustrations static-detail, no value animations (all in `MainView::timerCallback`).
* Engine: `NeededPaths` forced to C only; Echo/Resonator interpolation uses linear instead of cubic only if bit-exactness is
  not affected for default settings (otherwise skip that tweak); denormal flush on (already).
* Target: ≥ 35 % lower total CPU for a Loose Jack + Studio-off patch with editor closed vs today; recorded in QA.md.
* Not changed: sound with Monitor = Full Output.

### D3 acceptance
TestPerf rows "light-closed", "light-open" ≤ 65 % of baseline; bit-exact test above passes; `Full Output` identical with Light on/off.

---
## D4 — Optional oversampling ("Quality")

### Decision gate (spike first, 2 days)
Decisions.md chose ADAA over oversampling to keep 0 latency. Only ship oversampling if a measurement shows audible benefit:
sweep 1–19 kHz sines at −6 dBFS through Input Drive, Studio Drive, and Ceiling at 48 kHz; compute aliasing products relative to
fundamental. Ship if worst-case alias level with ADAA > −60 dBc *and* 2× oversampling improves it by ≥ 12 dB. Otherwise close
as "not needed" with the numbers in Research.md.

### Design if shipped
* Parameter `quality` (choice appended: `Standard (zero latency)|High (2x)|Ultra (4x)`), default Standard. Applies to nonlinear
  stages: Input Stage Drive, Studio Drive, Ceiling, Resonator soft clip. Digital Crush is intentionally aliasing — excluded.
* Implementation: `juce::dsp::Oversampling` with `filterHalfBandPolyphaseIIR` (**minimum-phase, zero reported latency**)
  for High; Ultra uses `filterHalfBandFIREquiripple` (linear phase) and reports latency via `setLatencySamples`.
  Latency changes only at `prepareToPlay`/explicit user change, then `updateHostDisplay`. Dry path is delayed by the same
  amount so Dry/Wet and bypass stay time-aligned (the architecture already guarantees alignment for parallel buses).
* Offline render uses Ultra automatically; project default stays Standard (no sound change on old projects).
* Bit-exact identity when stage off; switching Quality uses a 10 ms crossfade between old/new path; CPU: High ≤ 2× baseline.

### Tests
`os.aliasRejectionImproves`, `os.latencyReportedAndCompensated` (impulse test: dry/wet alignment ±0 samples), `os.standardIsBitExactToOldBuild`
(fixtures), `os.switchIsClickFree`, `os.bypassIdentity`.
