# Progress — Interference

A short chronological log. Commit hashes refer to `git log`. The detailed task list is `TODO.md`, decisions are
`DECISIONS.md`, and test and validation evidence is `Docs/QA.md`.

## Session 1 — 2026-09-23 (stopped by the user before the first build)
- Read every spec and replaced the stale Wubforge `DECISIONS.md`.
- `052cb2c`: header-only physical core (`Source/Core`: contact model, wear map, motion driver, gestures, pattern,
  RNG, PhysicalLimits, parameter table), the four sources, the Engine with the R / C0 / C counterfactual buses, the
  Studio chain, and the offline test suites. Not yet compiled.

## Session 2 — 2026-09-23 (first builds)
- `d456172`, `fecf43a`: first compile. Exact declick ramps, Studio mix exactness, resonator gain, pot vibration,
  jack short zone. Map seed separated from the bar-keyed streams, take number, sample-accurate MIDI one-shot. All
  offline tests pass.
- `1b1e0f3`: StateIO, factory presets, offline renderer and live recorder (120 tests passing).
- `bb22105`: plugin processor, editor and the full UI (theme, widgets, illustrations, slots, lane editor, Studio,
  routing, overlays, help), plus the app icon.

## Session 3 — 2026-09-23 (validation and cleanup)
- `780f1f6`: GUI tests fixed (log-skewed parameter tolerance, compact output strip at the minimum size).
- `5cd5da2`: warning-free build; LICENSE, THIRD_PARTY, CHANGELOG and the privacy statement. pluginval strictness 5
  passes.

## Session 4 — 2026-09-23 to 2026-09-24 (this session)
- `e9246ed`, `59ffcd0`: WP13, compiled and verified. Debug window, opt-in crash log, troubleshooting export, hard
  reset, and the hidden Ghost Carrier behind the secret pixel. 121 DSP tests and 13 host/GUI tests pass.
- `744f400`, `ab0d807`: documentation set (README, Manual, Research, Routing, MIDI map, Install, Issues, Decisions
  mirror, Privacy), `tools/make_release.ps1`, and the slow CPU and 1 h soak tests.
- Helper-agent workflow: three agents worked on disjoint files while only the main session built (4 GB RAM).
  - auditor: spec audit, then UI and docs (`Docs/research/SpecAudit.md`);
  - reviewer: code reviews (`CodeReview*.md`, `ReleaseReview.md`);
  - researcher: electrical interference research and tests (`ElectricalInterference.md`).
- `9bbe9e8`: DSP fixes from the review and audit.
  - Per-sample gain smoothing, declicked switches, a linked ceiling, sample-rate-independent Studio timing.
  - Feedback DC blocker, kill switches that clear once, and no pop without a contact change.
  - Studio stage peak meters and excitation-cap telemetry.
- `3c9d96a`: Environment from research: coupling-aware hum spectra, ground loop with interface rejection,
  phase-control dimmer buzz, and 2G phone bursts (10 new parameters).
- `0c81eb3`: integration of agent rounds 2–3.
  - Crash hook rewrite, and test isolation via `IFX_DATA_ROOT`.
  - UI: "Limited by" and effective-value indicators, "Move toward worn area", stage meters, the order button, the
    Excitation badge and the Environment tab.
  - Five environment presets (27 in total), doc fixes, and 27 new tests.
- `73226d3`: researcher rounds 3–4 and auditor round 5.
  - Policy, sample-rate and pot declick fixes; impedance-aware shield pickup.
  - Square-law RF transmitters (AM Station / Two-Way Radio / Motor Sparks) and triboelectric handling noise.
  - GitHub Actions CI (`.github/workflows/ci.yml`: licence and parameter-doc checks, Windows build and tests,
    optional signing, release ZIP, manual slow job) and a doc consistency pass.
- `bce7516`: QA results.
  - 168 DSP tests pass (3 slow tests skipped by default); the slow suite passes.
  - Worst-case CPU is about 12 % of one i5-2520M core at 48 kHz and 39 % at 192 kHz, with zero latency.
  - The offline 1 h soak runs in 437 s with no drift.
  - Unverified release gates are listed.
- `8687cb1`, `00b7635`: RF and triboelectric docs, Issues O3 updated with the soak evidence, and Research.md
  provenance for the Studio chain and Machine Pattern.
- Later rounds (reviewer UI fixes, final re-audit): the Details drawer scrolls, the guided ramp aborts on user input,
  CI builds with warnings as errors, and DECISIONS.md records the remaining deviations. SpecAudit section 12 has the
  final status: 456 of 498 requirement rows implemented; the rest are documented deviations, release gates or
  assigned polish.

## Where to resume
1. `TODO.md`, section "Remaining": integrate any outstanding agent work, full build, all tests. Then pluginval
   strictness 10, `tools/make_release.ps1`, and record both results in `Docs/QA.md`.
2. `Docs/QA.md`, section "Unverified release gates": DAW load in FL Studio, a real-time host soak and memory check,
   sidechain in a host, empirical calibration, publisher items (homepage, support e-mail, licences, signing
   certificate), the sanitizer build, and other platforms.
3. `Docs/research/SpecAudit.md` section 12: the remaining (d) items and who owns them.
