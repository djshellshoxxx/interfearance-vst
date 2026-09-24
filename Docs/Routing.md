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
                              (** hidden effect, off by default; acts on W only; bypassed by Raw Hardware)

HOST SIDECHAIN s ─► [detector] ─► Sidechain React gestures, Rhythm Gate key   (never audible)
MIDI / GUI / automation / Gesture Lane ─► gesture scheduler ─► slot gestures + Studio sends / chokes
```

`C = R + D + A` holds exactly by construction: each slot's inline circuit is evaluated three times per sample
(R, C0, C) using one shared contact state. \*Keep Excitation keeps the excitation with Raw Hardware on.

**Environment injections** (Environment tab). Ground-loop hum (after the receiving interface's rejection) and 2G
phone bursts are added to the complete path C *before* the slots. They belong to bus A, not to R or C0, and a
downstream open contact interrupts them like any other signal. A Loose Jack sleeve or a damaged Cable Handling
shield is part of the loop's resistance, so contact state modulates the loop hum. The mains field, Buzz Band and
Interference are not added here: they enter only through each source's own exposed paths.

In the plug-in window the processing order is the button between the slot cards (`A → B` or `B → A`), and the
Routing tab draws the same graph with the live order and feedback amount. Each Studio stage has a small meter under
its button: the upper bar is the stage's input peak, the lower bar its output peak.

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
- Raw Hardware gives W = C exactly once its 10 ms crossfade settles: it bypasses Studio, feedback, excitation (unless
  Keep Excitation is on) and the hidden Ghost Carrier.
- Gains are smoothed per sample over about 10 ms (Dry/Wet, Dry Trim, Output, Studio Level, Ceiling, Studio stage
  amounts, Drive gains), so the identities above hold exactly once a change has settled.

## Output ceiling

The optional **Ceiling** is an explicit, audible peak ceiling, off by default: a stereo-linked gain rider with
instant attack and about 45 ms release, followed by a final clamp at the ceiling level. It adds no latency. A
separate numeric failsafe replaces only invalid values (NaN/Inf) and extreme overflows. It never colours normal
audio.

## Buses and channel layouts

| Bus | Layouts | Notes |
|---|---|---|
| Main in / out | mono → mono, stereo → stereo | No implicit mono sum, stereo duplication or channel swap. Other layouts are rejected. |
| Sidechain in | mono or stereo, optional | Detector only. When it is absent, React shows "no sidechain" and does nothing. |
| MIDI in | yes | Gestures, fill, one-shot, CC/pressure, MIDI learn. See `MidiMap.md`. |

The tail length reported to the host is computed from the enabled Echo, Resonator and feedback settings (60 dB
decay, capped at 30 s). With Studio off or Raw Hardware on it is 0. Kill Feedback and Kill Tail fade those tails.

**Host freeze and offline bounce.** The tail length above is what a host adds after the last block when it freezes or
bounces the track. A frozen or bounced file is identical to the real-time output from the same start position when
Lock is on. Echo, Resonator and feedback tails are always finite in normal use (feedback loop gain is clamped below 1);
a tail that is longer than the reported 30 s cap is cut by the host's tail policy, not by the plug-in.
