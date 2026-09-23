# Decisions (autonomous run) — Interference

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
