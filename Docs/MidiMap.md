# MIDI and automation map

Send MIDI to the Interference track or instance. Some hosts do not pass MIDI to audio effects. In that case every
MIDI function has a GUI and host-automation equivalent (Play buttons, Trigger A/B parameters, lane, Fill).

## Notes

| Note number (60 = middle C) | Action | Note-off |
|---|---|---|
| 36, 37, 38 | Slot A: the loaded source's three pattern gestures (Jack: Wiggle, Seat, Pull; Touch: Tap, Hold, Release; Cable: Bend, Shake, Tap; Pot: Turn to Angle, Sweep, Hold Position) | releases slot A |
| 39 | Slot A: its selected Gesture (Slot A Gesture parameter) | releases slot A |
| 48, 49, 50 | Slot B: pattern gestures, as for 36–38 | releases slot B |
| 51 | Slot B: its selected Gesture | releases slot B |
| 62 | One-shot: plays one cycle of the longest Gesture Lane from this exact sample (same as *Play once*) | stops at once |
| 63 | Fill (held): temporary denser lane variation | ends the fill |
| any other note | Both slots play their selected gestures. The note also sets the Resonator pitch when *Follow MIDI* is on. | releases both slots |

Velocity sets gesture intensity. Gestures stay physical: a gesture on a healthy, still, shielded source with no hum
makes no sound. Too-fast retriggers are coalesced and logged as *motion limit*.

## Controllers

| Message | Action |
|---|---|
| CC 1 (mod wheel) | Continuous random hand motion added to both slots' gestures |
| Channel pressure / poly aftertouch | Extra contact pressure / touch |
| CC 120, CC 123 | All sound off / all notes off: releases every gesture |
| Any learned CC | Drives the mapped parameter (host-visible, automatable) |

## MIDI learn

Right-click any control → **MIDI learn** → move a controller. The mapping stores the CC and its channel.
**Clear MIDI mapping** removes it, and *Options → Clear all MIDI mappings* removes all of them. Mappings are saved
with the project. **Reset** keeps them; **Hard reset** (Help → Debug) clears them. A learned CC controls only its
parameter and is not also used as a gesture controller.

## Host automation

All 140 parameters are automatable, with stable IDs (see `Parameters.md`). Continuous parameters are smoothed.
Fault events are not smeared: automation moves the hardware, and the physics decides what happens.

Momentary parameters: `trigA`, `trigB` (play slot A or B's gesture while on), `fill`, `kill_fb` and `kill_tail`.
Non-parameter state (seed, lock, take, slot sources, lane pattern, MIDI map) is saved with the project and presets
but is not automatable.
