# Decisions

Mirror of `DECISIONS.md` in the repository root (the working log). Both are kept identical.

The previous content of this file described a different project (Wubforge) and was replaced; behave.md is also a
Wubforge document and is used only for its general interaction grammar (overlays dismissible, right-click menus).

- Toolchain: JUCE 8.0.8 (local copy `../JUCE_8_0_8`, else `./JUCE`, else FetchContent tag 8.0.8), CMake >= 3.22,
  MSVC 2022 BuildTools x64, C++17, static MSVC runtime: that is what is installed and was proven by the sibling projects.
- JUCE is not vendored into git (100 MB); CMake finds it via `INTERFERENCE_JUCE_DIR`.
- Zero-latency design: Drive/preamp saturation uses first-order ADAA instead of oversampling, the Ceiling has no
  lookahead, so reported latency is always 0 and every parallel bus (x, R, C0, C, P) is trivially aligned.
- Counterfactual buses are computed by running each source's inline circuit three times per sample (R, C0, C) with
  the contact state computed once; so `C = R + D + A` is exact by construction.
- Source settings are stored per source type (not per slot) so moving a source between slots or bypassing it keeps
  its settings; slot contents (types) are non-automatable state, slot enable/level are parameters.
- "INTERFERENCE_VST_CREATIVE_FEATURES.md" is referenced by two specs but is not in the repo; the features it names
  that the routing spec also defines (EQ, Contour, Resonator, Echo, sidechain React) are implemented from the routing spec.
- The Machine Pattern lane (1-64 steps, per-slot A/B lanes) is the single Gesture Lane; the master prompt's 1-16 step
  lane is the same editor with a shorter length.
- Determinism policy: RNG sub-streams are re-keyed from (seed, source, slot, bar index) at every bar while the
  transport plays; a transport start, seek or loop jump resets fast physical state. Same start position + same state
  gives identical output for any block size.
- No automatic GitHub push: publishing to a remote is an outward-facing action the user did not authorise in this run.
- DAW host loading cannot be performed unattended here; it is recorded as an explicit unverified release gate.
- Declick fades for slot enable, Studio, Raw Hardware, plugin bypass and Studio stages are fixed 10 ms linear ramps
  (not exponential) so identities are bit-exact once a switch settles; crossfades use an exact-at-1 lerp.
- Wear/weak-spot maps derive from the user Seed only; the effective random stream is hash(Seed, Take). `New Take`
  increments Take: timing, positions and fault maps stay, micro-events change. Bar re-keying never touches the maps.
- Loose Jack `Short to Sleeve` is also the share of the partly-inserted zone in which the tip rests across the
  sleeve spring (grounded, quiet) instead of floating (hum pickup).
- Resonator: the internal feedback loop runs on 0 dB-peak modes (loop gain = feedback < 1); the output is scaled by a
  modal gain min(Q, 8) through a soft clip, and the local-feedback loop-gain estimate includes that gain.
- Default lane division is 1/16 (index 3); the previous default pointed at 1/16T by mistake.
- A MIDI one-shot (note 62) starts on the exact sample of its note-on and plays one cycle of the longest lane.
- Build order on this machine: RAM is scarce, so documentation and code are written first and compiled in one pass at the
  end (user request 2026-09-23); intermediate commits may be marked "not yet compiled".
- Easter egg = "Ghost Carrier": ring modulation of the wet bus W by a slowly wandering carrier (4 appended parameters,
  off by default, so dry, bypass and every identity are untouched). Opened by the one pixel at the tip of the header
  notch (14, 2), with 1 px slack for high-DPI rounding. Parameters are appended at the end of the table, so IDs stay stable.
- Crash log: opt-in and never persisted (off on every load). On Windows it chains to, and on stop restores, the host's
  unhandled-exception filter instead of JUCE's global crash handler, which cannot be removed and would outlive the DLL.
- Troubleshooting file and crash logs are written straight to Documents/Interference/Support with time-stamped names
  (no file dialog) so they are easy to find and describe; the popup shows the exact location.
- Hard reset = Reset + clear MIDI map, A/B, options, and delete the options file (the only cache). User presets, renders
  and support files are user data and are kept.
- Research notes use abstracts only (no full-text access in an unattended run); stated per paper in Docs/Research.md.
- No installer: the repo has no trustworthy installer pipeline, so the release is a documented ZIP (spec 12).
- Source impedance: only nodes the source drives (a broken cable shield's inner conductor) are held ~34 dB down by a
  Buffered/Line source; the touched tip is a floating node whose impedance is Circuit Exposure, so Touch is unscaled.
- theme.md 80 ms value animation (V13) is not implemented for 1.0: attachments and host automation set values
  directly, and animating the drawn position would misrepresent the automated value; slider ticks (V8) are drawn.
- Agents: three helper agents (research, audit, review) worked on disjoint files; only the main session builds
  (4 GB RAM machine). Their reports live in Docs/research/.
- Machine Pattern scheduling (industrial spec 5): lane events are computed per block on the audio thread by
  `scheduleLane` (a pure function of pattern + musical position) and insertion-sorted there, instead of a precompiled
  event table. Reason: the pattern arrives lock-free (seqlock), the work is bounded (at most 512 events, no
  allocation), and scheduling from the absolute position is what makes seek, loop and tempo changes deterministic.
- Licence activation (master spec 12): none in 1.0; offline users are never blocked. Extension point, documented
  only: a check would run on the message thread in `InterferenceProcessor`'s constructor, behind a `Branding` flag
  next to `Branding::licenseSummary` (Source/UI/HelpContent.h), and report through the troubleshooting file's
  "Licence activation" line. It must never gate the audio thread. Reason: the repo has no activation system, and the
  spec forbids inventing a server dependency.
- "Advanced calibration" in the Details drawer (master spec 8) is not a UI: the engineering caps live in
  `Source/Core/PhysicalLimits.h` (Natural / Expressive columns, status "tunable, unmeasured") and are changed in code
  after measurements. Reason: exposing caps to users would let them defeat the realism limits.
- theme.md font weights 500 / 600: JUCE fonts offer plain and bold only, so labels use plain and headers bold. Reason:
  no weight axis in JUCE 8's font API without embedding variable fonts (none are bundled: THIRD_PARTY.md).
- theme.md knob sizes (48 / 36 / 64 px): knobs scale with their layout cells in the resizable window instead of
  fixed pixel sizes. Reason: the editor resizes from 1000x700 to 2400x1600, and fixed sizes would waste or overflow
  space.
- theme.md "never boxes inside boxes": the slot cards and illustrations keep a 1 px bordered panel. Reason: they are
  the two selectable, clickable units of the window (selection is shown by the accent border); every other grouping
  uses 1 px separator lines.
