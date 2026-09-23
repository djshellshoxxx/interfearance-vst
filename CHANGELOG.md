# Changelog

Interference uses semantic versioning: MAJOR.MINOR.PATCH.

- **PATCH**: bug fixes; sound of saved projects and presets unchanged except where the old behaviour was a bug
  (listed here).
- **MINOR**: new features/parameters. Parameter IDs are appended, never renamed or renumbered; older projects and
  presets load with new parameters at their defaults, which are chosen to reproduce the old sound.
- **MAJOR**: reserved for a change that cannot load old state identically; a migration step converts old state and
  the change is documented here.

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
- Routing / monitor taps (Full, Artifacts, Direct, Circuit), mix laws, output ceiling, meters.
- Offline WAV render (bars or seconds; 16/24/32-bit float; 44.1/48/96 kHz; mono/stereo; tail; normalize) and
  live record.
- 22 factory presets in 6 categories; user presets with modified marker and A/B compare.
- MIDI map and MIDI learn; in-plugin help, tooltips, keyboard navigation.
- Zero latency; mono and stereo; 44.1–192 kHz.
