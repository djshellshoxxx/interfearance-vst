# Routing and buses

Every audible host sample enters through the **main input**. The sidechain is used **for control only**. Internal
excitation is a clearly labelled Studio source (Create mode). No microphone, loopback or external feedback is ever
used. Latency is **0 samples** in every configuration, so every parallel bus below is sample-aligned.

## Signal graph

```
HOST MAIN IN  x ─────────────────────────────── [Dry Trim] ──────────────┐
      │                                                                  │  y = dryGain·x + wetGain·W
      └─► [Input Trim] ─► [Source Circuit: Line / Passive Pickup] ─┐     ├──► [Output] ─► [Ceiling*] ─► [Bypass] ─► HOST MAIN OUT
                                                                   │     │      (* optional, audible, off by default)
INTERNAL EXCITATION e (Create; off by default) ────────────────────┤     │
                                                                   ▼     │
                     [optional Input Stage Drive (ADAA)] ─► p            │
                                                                   │     │
            ┌──────────────────────────────────────────────────────┤     │
            ▼                         ▼                            ▼     │
   R: healthy reference     C0: same contact states,      C: full physical core
   (nominal pot/tone,        injections = 0                (slot A ─► slot B, or B ─► A
    no faults)                                              with Swap Order)
            │                         │                            │     │
            └────────── D = C0 − R ───┴────────── A = C − C0 ──────┘     │
                                                                          │
   Studio Source F = A (Artifacts) | D (Breaks) | A + D (Both);  U = the unselected part
            │                                                             │
            ▼                                                             │
   EQ ─► Contour ─► Drive ◄─┐ ─► Crush ─► Resonator ─┬─► Rhythm Gate ─► Echo ─► P (Studio out)
                             │                        │
                             └─ Feedback (HP/LP, delay, bounded, zero by default) ◄┘
                                                                          │
   W = R + U + P   (Studio off: W = C exactly;  Raw Hardware: W = C, no Studio, no feedback, no excitation*)
            │                                                             │
            └────────────────────────────────── Ghost Carrier** ──────────┘
                                                (** hidden effect, off by default; acts on W only)

HOST SIDECHAIN s ─► [detector] ─► Sidechain React gestures, Rhythm Gate key   (never audible)
MIDI / GUI / automation / Gesture Lane ─► gesture scheduler ─► slot gestures + Studio sends / chokes
```

`C = R + D + A` holds exactly by construction: each slot's inline circuit is evaluated three times per sample
(R, C0, C) using one shared contact state. \*Keep Excitation keeps the excitation with Raw Hardware on.

## Monitor taps (what leaves the plug-in)

| Monitor | Bus | Meaning |
|---|---|---|
| Full Output | y | Normal output: dry/wet mix after Output. |
| Full Core | C | The complete modelled hardware, without Studio. |
| Injected Artifacts | A | Only what the faults add (hum, pops, scratch, handling), after downstream hardware. |
| Fault Difference | D | Only what the faults change in the signal (loss, tone change, gating). Signed. |
| Studio Only | P | The Studio branch output. |
| Full Wet | W | The wet signal before the dry/wet mix. |
| Dry Input | x | The host input, time-aligned. |

Monitor changes only what leaves the plug-in. It never changes contact states, random streams or source order.
**Capture Tap** (Record) and the render dialog's **Tap** use the same definitions.

## Mix laws and identities

- Dry/Wet: linear (default) or constant power. 0 % Wet gives the time-aligned dry input exactly. 100 % Wet gives
  exactly W before Output.
- A bypassed slot is an exact identity for that slot. Two bypassed slots, or two empty slots with no hum or
  excitation, give exactly the input.
- Plug-in Bypass gives the time-aligned dry input after a 10 ms linear declick.
- Studio off gives W = C exactly. Every Studio stage has its own on/off and wet amount.

## Buses and channel layouts

| Bus | Layouts | Notes |
|---|---|---|
| Main in / out | mono → mono, stereo → stereo | No implicit mono sum, stereo duplication or channel swap. Other layouts are rejected. |
| Sidechain in | mono or stereo, optional | Detector only. When it is absent, React shows "no sidechain" and does nothing. |
| MIDI in | yes | Gestures, fill, one-shot, CC/pressure, MIDI learn. See `MidiMap.md`. |

The tail length reported to the host is computed from the enabled Echo, Resonator and feedback settings (60 dB
decay, capped at 30 s). With Studio off it is 0. Kill Feedback
and Kill Tail fade those tails.
