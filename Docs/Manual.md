# Interference 1.0 — User manual

Interference turns faulty analog signal paths into an audio effect and a playable sound source. It models four
pieces of misbehaving hardware — **Loose Jack**, **Exposed Plug Touch**, **Cable Handling** and **Dirty Pot** — and
lets you load any one, or any two different ones, in slots A and B.

Every audible fault has a cause: movement, wear, pressure, touch, or an environment (mains hum) or excitation you
switched on. A healthy, still, shielded circuit with no hum is silent. Controls describe what you do to the hardware
(Move Plug, Touch, Shake Cable, Turn Knob), not abstract noise amounts.

The models are **research-informed**, not measurements of particular devices (see `Research.md`).

Contents: [Modes](#modes) · [Quick start](#quick-start) · [The window](#the-window) · [Sources](#the-four-sources) ·
[Two slots](#two-slots-order-and-linking) · [Gesture lane](#gesture-lane) · [Seed and repeatability](#seed-lock-and-new-take) ·
[Studio chain](#studio-chain-and-routing) · [Render and record](#render-and-record) · [Presets](#presets) ·
[Levels](#level-guidance) · [Help and debug](#help-and-debug-tools)

Reference: [Parameters](Parameters.md) · [MIDI map](MidiMap.md) · [Routing](Routing.md) · [Install](Install.md) ·
[Privacy](Privacy.md)

## Modes

Interference is both an **effect** (it processes the track it is inserted on) and a **generator** (with internal
excitation it makes sound on a silent track). The same engine runs in all three modes:

| Mode | Use | What drives the hardware |
|---|---|---|
| Hardware | Insert effect: subtle hum, resistive loss, scratch, an occasional fault | Knob positions, automation |
| Play | Rhythmic, performed faults on a track | MIDI notes, Play buttons, illustrations, the Gesture Lane |
| Create | One-shots and loops from nothing | As Play, plus an internal excitation signal (Noise, Buzz, Bias Impulse, Test Tone) |

## Quick start

**Hardware — subtle coloration**
1. Insert Interference on a guitar track and click **Loose Jack** in the source row.
2. Details > Source circuit: **Passive Pickup** for guitar, **Buffered/Line** for everything else.
3. Keep Move Plug at 90–100 %, add a little Wiggle and Wear. Seat the plug fully (100 %) and the faults stop.
   Bypass gives the dry signal back, sample-aligned.

**Play — rhythmic faults**
1. Load Loose Jack in A and Cable Handling in B; choose **Play**.
2. In the Gesture Lane click steps to add gestures (drag up/down for intensity); turn **Lane On**.
3. Start the host transport. Gestures start on the grid; the crackles inside them stay physical and vary.
4. Keep **Lock** on (default) and bounce twice: the files are identical.

**Create — one-shots and loops**
1. Choose **Create**, pick an Excitation and level.
2. Play the slots (buttons, MIDI, lane). Set Monitor to *Injected Artifacts* to hear only the faults.
3. **Render WAV…** chooses bars or seconds, format and file. **New Take** keeps the timing but changes micro-events.

## The window

- **Header** — output LED (dark = silence, white = 0 dB, red = over 0 dBFS), menu (Save, Save As, Open, Options,
  Open preset folder), preset browser with modified marker `*`, A/B compare, Reset, Randomize, Help (`?`, F1), Options.
- **Mode row** — Hardware / Play / Create; the four sources (each shows its slot; the row shows the two-source limit);
  Seed, Lock, New Take; Natural / Expressive.
- **Slot cards A and B** — source, one-line physical cause, activity meter, gesture, enable, disturbance, Play and
  *Why quiet?*. Under each card is its illustration, which draws the engine's real state and is itself a control:
  - Loose Jack: drag the plug left/right (Move Plug); up/down wiggles it.
  - Exposed Plug Touch: press and hold the finger; contact grows while held; drag down to press harder.
  - Cable Handling: drag along the cable to set the bend point; fast drags shake it.
  - Dirty Pot: drag to turn; double-click to show the interior (track, wiper, the seeded worn arc).
- **Context panel** — labelled knobs for the selected slot (redundant with the illustration). **Details** opens circuit
  mode, input stage, stereo, calibration and diagnostic meters.
- **Bottom tabs** — Gesture Lane, Studio, Create, Routing. **Output strip** — Dry/Wet, Output, Monitor, Mix law,
  Dry trim, Ceiling, Bypass, meters.

Keyboard: Tab / Shift+Tab move focus, arrows adjust the focused control, Space plays a focused slot's gesture,
F1 opens help, Esc closes overlays. Right-click any control for MIDI learn, clear mapping, reset or typing a value.
Colour is never the only cue: states are also labelled in text.

## The four sources

### Loose Jack
A TS plug in a worn jack (one tip/sleeve pair; switching, TRS and powered jacks behave differently).
Move Plug (insertion; 100 % seated, the unstable zone is part way in, 0 % withdrawn = open input), Wiggle (lateral
movement), Pressure (spring force), Wear and Wear Position, Intermittency (clusters eligible faults — cannot create
faults on a healthy still contact), Recovery, Short to Sleeve (interruptions ground instead of floating), Pop
(bias/charge discharged by transitions), Shield. An open contact removes the through signal.

### Exposed Plug Touch
A simplified, artistic input-coupling model: a finger on a floating high-impedance tip couples the local mains field
in. Touch (area), Pressure (steadiness), Body (hum colour, never a pitch), Circuit Exposure, Shield, Pop. With the
global Hum at zero, touching only clicks.

### Cable Handling
Electrical faults (conductor and shield) driven by flexing over a weak spot, plus optional mechanical rub/taps heard
only through an enabled microphonic path. Bend, Shake Cable, Damage (broken strands interrupt signal), Shield Damage
(admits hum while the conductor stays live), Mic (0 = a sealed cable is silent), Weak Spot.

### Dirty Pot
A rotary pot on a finite track used as a Level or Tone control. Turn Knob (level/tone follow the taper even with no
dirt), Taper (Linear, Log A, Reverse Log C), Circuit (Level, or Tone: 250 k + 22 nF treble cut), Dirt, Wear Position
and Worn Arc Width (drawn on the illustration), Scratch, Circuit Bias (makes scratch audible on silence), Vibration
(only a pot with Dirt above 70 % stays intermittent while held still). Holding the knob still stops the scratch.

## Two slots, order and linking

Slots are processed in series: A then B, or B then A with **Swap Order** (Details). A downstream open contact
interrupts the upstream source's artifacts; a downstream pot filters them. This is a plausible series interaction,
not an exact model of one combined device.

Picking a third source asks which slot to replace; nothing is dropped silently. Choosing the other slot's source
moves it (they swap). Bypassing a slot keeps its settings and is an exact identity; its physical time keeps running
silently, so un-bypassing does not replay old events. **Linked Gesture** sends one movement to both slots; each keeps
its own contacts, wear and random stream. Both slots share one mains phase and a global noise budget.

## Gesture lane

Two lanes (A drives slot A, B drives slot B), 1–64 steps each; lengths may differ for polymeter. Each step: gesture,
intensity, accent, ratchet (2–4 retriggers; too-fast retriggers are coalesced and logged as *motion limit*),
probability, duration, position target, microtiming, Studio send and choke. The lane schedules gesture **starts**;
the micro-events inside stay physical. Swing, Humanize, Density, Evolve (seeded per-bar variation) and Fill (or MIDI
note 63) shape it.

Keys: arrows move, Space toggles, +/− intensity, A accent, R ratchet, G gesture, P probability, C choke,
Ctrl+C/V copy/paste, Shift+arrows shift, Ctrl+Shift+arrows rotate, Delete clear, Ctrl+Z/Y undo/redo.

Timing follows the host position, so bars, loops and seeks are deterministic. Without a running host transport
press **Run**: the internal clock uses Internal BPM and says so.

## Seed, Lock and New Take

Every random decision is derived from Seed, Take, source, slot and the absolute musical position (bar). With
**Lock** on (default) the same host position and settings give identical audio for any buffer size, bounce or
loop pass. Unlocked, each transport start draws a new take. **New Take** keeps positions, timing and fault maps but
changes the micro-events. Worn arcs and weak spots depend on the Seed only.

Reproducibility holds for the same sample rate and plug-in version; a different sample rate gives the same gestures
and timing but not a bit-identical waveform.

## Studio chain and routing

Studio processing happens **after** the modelled hardware and is labelled Studio (never a property of the cable).
Studio source: Artifacts (injected hum/pops/handling), Breaks (the fault's change to the signal) or Both. Fixed
order: EQ → Contour → Drive → Crush → Resonator → Rhythm Gate → Echo, each with on/off and wet. Feedback is a bounded
local loop from after the Resonator back into Drive (zero by default; Kill Feedback clears it). **Raw Hardware**
bypasses Studio, feedback and excitation. Monitor chooses what leaves the plug-in; it never changes the model.
Latency is always 0 samples. See `Routing.md` for the bus diagram.

## Render and record

**Render WAV…** renders the gesture/excitation path with a cloned engine at a fixed tempo (it does not include host
audio): 1–16 bars or 0.1–30 s; 44.1/48/96 kHz; 16/24-bit PCM or 32-bit float; mono/stereo; optional tail and peak
normalisation. A silent result is explained before anything is written; existing files are never overwritten
without confirmation. Names follow `Interference_<preset>_<seed>_<length>_<bpm>bpm.wav`; an optional sidecar `.txt`
stores seed and tempo.

**Record** captures the selected Capture Tap while you play, through a lock-free buffer written by a background
thread. To process track audio, use your host's bounce or record.

Export limitations: maximum 30 s / 16 bars per render; no MP3/FLAC; the render uses the plug-in's own tempo field,
not the host tempo map; host audio is not rendered.

## Presets

Factory categories: Repair Faults, Rhythmic, One-Shots, Subtle Texture, Wild (Expressive), Machine Pattern.
User presets are `.ifxpreset` files in `Documents\Interference\Presets` (macOS: `~/Documents/Interference/Presets`).
Copy files there to see them in the browser, or use Open from anywhere. Presets store every parameter, seed, lock,
take, sources, lane and the Natural/Expressive label — never paths or audio. Older presets load safely: missing
values take defaults, unknown values are ignored, bad values are repaired.

**Reset** returns every sound setting to default (MIDI mappings are kept). **Randomize** resets, then picks new safe
Natural settings. **Natural** (default) applies strict physical caps; **Expressive** allows faster gestures, denser
bursts and louder excitation — still causal, never presented as measured hardware.

## Level guidance

Factory presets are level-matched so switching them does not jump in loudness; the header LED and output meters show
peaks. Create-mode excitation and Studio Drive/Resonator/Feedback can add level — watch the meter and use the Output
Ceiling (an explicit, audible soft ceiling; off means no limiting). Bypass and one-slot bypass are exact.

## Compatibility and host notes

| | Status |
|---|---|
| Windows 10/11 x64, VST3 | Built and validated (pluginval, host-contract tests). DAW load still unverified; see `QA.md`. |
| Windows x64 stand-alone | Built. |
| macOS (VST3/AU), Linux, CLAP | Not built for 1.0. The DSP core is portable. |
| Sample rates | 44.1, 48, 88.2, 96, 192 kHz (tested offline). |
| Channels | Mono → mono, stereo → stereo, optional mono/stereo sidechain. |
| CPU | x64 with SSE2. Two slots plus the full Studio chain run in real time on a typical desktop CPU (see `QA.md`). |

Known host limitations:
- Some hosts do not send MIDI to audio effects. Use the Play buttons, the Trigger A/B automation or the Gesture Lane.
- A sidechain needs a host that exposes VST3 sidechain buses. Without one, Sidechain React shows *no sidechain*.
- Offline bounce is deterministic when Lock is on: the same start position gives the same audio.

## Help and debug tools

Help (`?` or F1) explains every feature, shows the version, licence, homepage and support address, and has
install/uninstall and preset-folder troubleshooting. **Help > Debug** opens a live data window, a
*create log file on crash* option (off every time the plug-in loads), a troubleshooting-file export and a hard reset.
See `Install.md` for where these files are written and what to send to support.
