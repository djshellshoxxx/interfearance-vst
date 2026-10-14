# Interference — feature specs and development plan (post-1.0)

Written 2026-10-10 against the code at commit `8008407` + the CLAP/Linux branch. Every spec names real files, types and
tests. Read in this order:

| # | Document | Purpose |
|---|---|---|
| 1 | [Standards.md](Standards.md) | Rules every change follows: real-time safety, parameter/state compatibility, merge-conflict protocol, definition of done |
| 2 | [DevelopmentPlan.md](DevelopmentPlan.md) | Waves, branch order, shared foundations, hotspot map, estimates, risks |
| 3 | [Spec-A-Essentials.md](Spec-A-Essentials.md) | E5 working CI · E6 crash reporting · E7 UI scale · E8 calibration / "does it sound right" |
| 4 | [Spec-B-Presets-State.md](Spec-B-Presets-State.md) | V1 preset browser · V2 randomise/mutate+locks · V3 morph · U3 undo/redo · U5 preview · N5 preset codes · V5 packs |
| 5 | [Spec-C-Sound.md](Spec-C-Sound.md) | V4 new fault types · D3 light mode · D4 oversampling |
| 6 | [Spec-D-UX.md](Spec-D-UX.md) | U1 tour · U2 macro screen · U4 meters + why-quiet · N3 visual feedback · D6 accessibility · D7 localisation |
| 7 | [Spec-E-Play.md](Spec-E-Play.md) | N1 Gremlin · N2 patterns + lane library · N4 MIDI performance |
| 8 | [Spec-F-Distribution.md](Spec-F-Distribution.md) | D1 landing page + audio examples · D2 demo build · D5 AAX |
| 9 | [Quality.md](Quality.md) | Test strategy, golden audio, fuzzing, perf budgets, review checklist |

Not specced by request: demo video. Not specced because a person/vendor must act: DAW verification, macOS, code signing,
JUCE/Steinberg licences (tracked in `Docs/QA.md` "Unverified release gates").

## Corrections to the earlier feature list (found while reading the code)

* **UI scaling already half-exists**: the editor is resizable 1000×700…2400×1600 and remembers its size. Missing is a
  user "UI scale" setting, keyboard zoom and verified HiDPI behaviour — E7 is scoped to that.
* **Crash hook is not Windows-only**: `CrashHookWin.cpp` already contains a POSIX fatal-signal branch (it compiled on
  Linux). What is missing is tests, `sigaltstack`, symbol files and a report flow — E6 is scoped to that.
* **Randomize, A/B compare, lane undo/redo and a preset combo box already exist.** V2/V1/U3 extend them.
* The "Why quiet?" dialog and dormant-control explanations already exist (`SlotPanel.cpp`). U4 extends them.
* MIDI already maps notes, velocity, aftertouch, mod wheel and learned CCs (`Docs/MidiMap.md`). N4 adds pitch bend,
  MPE and editable mappings only.

## Feature ID key
E = essential, V = value, U = ease of use, N = fun, D = desirability/distribution.
