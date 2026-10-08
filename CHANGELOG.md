# Changelog

Interference uses semantic versioning: MAJOR.MINOR.PATCH.

- **PATCH**: bug fixes; sound of saved projects and presets unchanged except where the old behaviour was a bug
  (listed here).
- **MINOR**: new features/parameters. Parameter IDs are appended, never renamed or renumbered; older projects and
  presets load with new parameters at their defaults, which are chosen to reproduce the old sound.
- **MAJOR**: reserved for a change that cannot load old state identically; a migration step converts old state and
  the change is documented here.

## Unreleased

- Added a CLAP build (clap-juce-extensions, pinned) and a Linux x64 build (VST3, CLAP, stand-alone). CI builds,
  tests and packages both platforms. Linux QA: 171 DSP + 17 GUI tests pass (GCC 13, Release). Known Linux limit: the
  crash-log hook is Windows-only, so no crash log is written on Linux. DAW load and pluginval on Linux/CLAP are not
  yet verified.

## Backward-compatibility policy

- Host parameter IDs (`Source/Core/Params.h`) are stable forever once shipped.
- Plug-in state and `.ifxpreset` files carry a format version. Older versions are migrated on load; unknown
  optional fields are ignored; malformed or out-of-range data is repaired to bounded defaults and reported in the
  data stream (never a crash, never a silent jump to a loud setting).
- Seeded randomness (seed, take, bar keying) is part of the sound: a PATCH release never changes the random
  stream of an existing seed.

## 1.0.0 — 2026-09-23

First release.

- Four physically modelled fault sources: Loose Jack, Exposed Plug Touch, Cable Handling, Dirty Pot.
- Two slots (A, B) with any pair in either order, swap, linked gestures, per-slot disturbance and exact bypass.
- Hardware / Play / Create modes; gestures from buttons, illustrations, MIDI notes and a 1–64 step gesture lane.
- Seed, Lock and New Take for repeatable bounces; bar-keyed random streams, block-size independent.
- Studio chain (EQ, Contour, Drive, Crush, Resonator, Rhythm Gate, Echo, bounded feedback) with Raw Hardware switch.
- Named buses and monitor taps (Full Output, Full Core, Injected Artifacts, Fault Difference, Studio Only, Full Wet,
  Dry Input), linear / constant-power mix, optional output ceiling, meters.
- Offline WAV render (bars or seconds; 16/24/32-bit float; 44.1/48/96 kHz; mono/stereo; tail; normalize) and
  live record.
- 27 factory presets in 7 categories; user presets with modified marker and A/B compare.
- MIDI map and MIDI learn; in-plugin help, tooltips, keyboard navigation.
- Zero latency; mono and stereo; 44.1–192 kHz.
- Help > Debug: live data window with misconfiguration checks, opt-in crash log, troubleshooting-file export and hard
  reset.
- Environment tab: coupling-aware mains hum, ground loop (other gear, outlets, cable run, shield, interface), supply or
  light-dimmer buzz, 2G phone bursts, RF transmitters (AM station, two-way radio, motor sparks, square-law detected);
  researched in `Docs/research/ElectricalInterference.md`.
- One hidden extra for the curious.

### Release notes 1.0.0

- **Formats:** VST3 (Windows x64) and a stand-alone Windows app. No installer: copy the bundle (see `Docs/Install.md`).
- **Verified:** 171 offline DSP/state/render/environment tests, 17 headless host and GUI tests, a CPU benchmark
  (about 12 % of one 2011 laptop core at 48 kHz, worst case), a 1-hour offline soak with automation, pluginval at strictness 10
  (see `Docs/QA.md` for the exact commands and results). CI workflow in `.github/workflows/ci.yml`.
- **Not yet verified:** loading in a real DAW, a real-time host soak, sidechain in a host, empirical calibration
  against recordings, and the publisher items (homepage, support e-mail, JUCE/VST3 licences, signing). These are
  open release gates listed in `Docs/QA.md` and `Docs/Issues.md`.
- **Known limitations:** some hosts do not send MIDI to effects (use the GUI, automation or the lane); Render WAV uses
  its own tempo and does not include host audio; bit-identical repeats need the same sample rate and version.
