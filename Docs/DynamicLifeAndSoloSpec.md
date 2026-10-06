# Dynamic Life and Slot Solo specification

Status: implementation target for `codex/dynamic-life-and-solo`.

## 1. Purpose

This addition has two goals:

1. Make two-source sound design easier by allowing either slot to be auditioned by itself without deleting, bypassing, or changing the other slot.
2. Add a simple performance macro that makes any loaded source evolve continuously instead of sounding static when the user wants motion.

Both additions must preserve Interference's physical-cause model. They must not create arbitrary broadband noise, must remain deterministic for a fixed seed/take, and must be exact no-ops at their default values.

## 2. Slot Solo

### 2.1 Parameters

Append two stable host parameters. Existing parameter IDs and ordering must not change.

- `slotA_solo` / **Slot A Solo**: boolean, default Off.
- `slotB_solo` / **Slot B Solo**: boolean, default Off.

### 2.2 Behaviour

- Neither Solo enabled: existing routing is unchanged.
- Only A Solo enabled: Slot A is audible; Slot B continues advancing its physical/control state but contributes no change to the three hardware buses.
- Only B Solo enabled: mirror of the above.
- Both Solo enabled: both slots are audible. This avoids a confusing "everything muted" state and matches common audio-tool solo behaviour.
- Solo must use the existing short slot-mix ramp so engaging/disengaging it does not click.
- A soloed-out slot must not contribute environmental pickup coupling or ground-loop openness while inaudible.
- Solo must not delete slot contents, reset wear/contact state, alter presets, or change slot enable parameters.

### 2.3 UI

Each slot card gets a compact **Solo** toggle beside its existing On control.

Tooltip: "Hear this slot without the other slot. If both Solo buttons are on, both slots play."

## 3. Life macro

### 3.1 Parameters

Append two stable host parameters:

- `slotA_life` / **Slot A Life**: continuous 0..100%, default 0%.
- `slotB_life` / **Slot B Life**: continuous 0..100%, default 0%.

### 3.2 Behaviour

Life adds low-frequency physical micro-motion to the slot's existing gesture signal.

At 0%:
- Output and state evolution must remain compatible with previous behaviour.
- No extra gesture motion is added.

Above 0%:
- Each slot receives a deterministic, smooth combination of slow displacement and pressure motion.
- Motion rate increases gradually with the knob.
- Motion depth increases gradually with the knob.
- The modulation is continuous across audio blocks and independent of host block size.
- The two slots use different phase/frequency relationships so they do not move in lockstep.
- The macro acts through the existing `GestureOut` path. Each source therefore interprets it using its own physical model:
  - Loose Jack: subtle lateral movement can work a worn/partly seated contact.
  - Exposed Plug Touch: touch pressure/contact can breathe.
  - Cable Handling: cable flex can keep moving through a weak spot.
  - Dirty Pot: the wiper can move through dirt/wear and produce evolving scratch when the physical prerequisites are present.
- Life does not bypass source prerequisites. A healthy cable with Mic off still cannot emit handling noise solely because Life is raised.
- Existing Natural/Expressive limits continue to apply.

### 3.3 UI

Each slot card gets a small **Life** knob near Disturbance.

Tooltip: "Adds smooth automatic micro-motion to this source. Higher values move it more and faster while keeping the source's normal physical rules."

## 4. Determinism and safety

- No allocation, locking, file I/O, host calls, or random-device access in the audio callback.
- Life phase state is reset with the engine's normal reset path.
- For fixed seed/take, transport and parameters, renders are repeatable.
- Values remain finite and bounded by existing source and output safety limits.
- Slot Solo and Life must work in mono and stereo.

## 5. Tests

Add engine tests before implementation:

1. **soloAExcludesSlotBButKeepsSlotA**: compare a two-slot render with A solo against a corresponding A-only render after ramp settling.
2. **bothSoloMatchesNormalTwoSlotRouting**: both solo buttons on must match neither solo on after settling.
3. **lifeZeroIsExactLegacyBehaviour**: explicit Life=0 produces the same render as defaults.
4. **lifeMakesEverySourceMove**: for every source type, a physically eligible setup with Life>0 increases source telemetry movement or changes the rendered result relative to Life=0.
5. **lifeIsBlockSizeIndependent**: same fixed setup at two block sizes nulls within existing deterministic tolerance.
6. GUI test: slot cards expose Solo and Life controls and remain inside their bounds at minimum supported editor size.

## 6. Documentation

Update Parameters and Manual after implementation. Record the implementation and test result in PROGRESS/CHANGELOG.
