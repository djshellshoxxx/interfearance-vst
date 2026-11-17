# User Guide

Interference is both an **effect** (it processes the track it sits on) and a **generator** (with an internal excitation it makes sound on a silent track). The same engine runs in all three modes.

| Mode | Use | What drives the hardware |
|---|---|---|
| Hardware | Insert effect: subtle hum, resistive loss, scratch, occasional fault | Knob positions and automation |
| Play | Rhythmic, performed faults | MIDI notes, Play buttons, illustrations, the Gesture Lane |
| Create | One-shots and loops from nothing | As Play, plus internal excitation (Noise, Buzz, Bias Impulse, Test Tone) |

## Quick start recipes

**Hardware, subtle coloration.** Insert on a guitar track, click Loose Jack. In Details > Source circuit pick Passive Pickup for guitar, Buffered/Line for everything else. Keep Move Plug at 90 to 100 percent, add a little Wiggle and Wear. Seat the plug fully and the faults stop. Bypass returns the dry signal, sample aligned.

**Play, rhythmic faults.** Load Loose Jack in A and Cable Handling in B, choose Play. In the Gesture Lane click steps to add gestures (drag up or down for intensity) and turn Lane On. Start the transport. Gestures land on the grid, and the crackles inside them stay physical and vary. Keep Lock on and two bounces are identical.

**Create, one-shots and loops.** Choose Create, pick an Excitation and level. Play the slots with buttons, MIDI or the lane. Set Monitor to Injected Artifacts to hear only the faults. Render WAV chooses bars or seconds, format and file. New Take keeps the timing and changes the micro-events.

## The window

- **Header**: output LED (dark = silence, white = 0 dB, red = over 0 dBFS), menu (Save, Save As, Open, Options, Open preset folder), preset browser (modified marker `*`), A/B compare, Reset, Randomize, Help (`?` or F1), Options.
- **Mode row**: Hardware / Play / Create, the four sources, Seed, Lock, New Take, Natural / Expressive.
- **Slot cards A and B**: source, a one-line physical cause, activity meter, gesture, enable, disturbance, Play and *Why quiet?*. Each card has an illustration that draws the real state and is also a control:
  - Loose Jack: drag left and right for Move Plug, up and down to wiggle.
  - Exposed Plug Touch: press and hold the finger, contact grows while held, drag down to press harder.
  - Cable Handling: drag along the cable to set the bend point, fast drags shake it.
  - Dirty Pot: drag to turn, double-click to show the interior.
  Right-click an illustration for the menu of its main control. *motion limit reached* at the top left means a gesture could not finish in time and was merged with the next one.
- **Order button** (between the cards): `A -> B` or `B -> A`, the processing order. Click to swap.
- **Context panel**: labelled knobs for the selected slot. **Details** opens the source circuit, input stage, trims, stereo, chaos, range policy, gesture linking and diagnostic meters. Where a value is limited, a yellow reason appears above the knob and a yellow tick marks the value actually used. The stored value never changes.
- **Bottom tabs**: Gesture Lane, Studio, Create, Routing, Environment. An **Excitation on** badge appears while an internal excitation feeds the circuit.
- **Output strip**: Dry/Wet, Output, Monitor, Mix law, Dry trim, Ceiling, Bypass, meters.

**Keyboard**: Tab and Shift+Tab move focus in a fixed order. Arrows adjust the focused control, Space plays a focused slot's gesture, F1 opens help, Esc closes overlays. Right-click any control for MIDI learn, clear mapping, reset or typing a value. Colour is never the only cue.

## The four sources

**Loose Jack.** A TS plug in a worn jack. Controls: Move Plug (100 percent is seated, the unstable zone is part way in, 0 percent is withdrawn = open input), Wiggle, Pressure, Wear and Wear Position, Intermittency (clusters faults, cannot create them on a healthy still contact), Recovery, Short to Sleeve, Pop, Shield. An open contact removes the through signal. A fully seated plug on a healthy spot is stable, and the context panel says *Contact stable here*. **Move toward worn area** (context panel) slowly moves the plug to Wear Position, and stops if you or automation move the plug.

**Exposed Plug Touch.** A simplified input-coupling model: a finger on a floating high-impedance tip picks up mains. Controls: Touch (area), Pressure (steadiness), Body (hum colour, never a pitch), Circuit Exposure, Shield, Pop. With the global Hum at zero, touching only clicks.

**Cable Handling.** Electrical faults in conductor and shield, driven by flexing over a weak spot, plus optional mechanical rub and taps heard through an enabled microphonic path. Controls: Bend, Shake Cable, Damage (broken strands interrupt signal), Shield Damage (admits hum while the conductor stays live), Mic (0 means a sealed cable is silent), Weak Spot. A Buffered/Line source holds the conductor at low impedance, about 34 dB less hum than a Passive Pickup.

**Dirty Pot.** A rotary pot on a finite track, used as Level or Tone. Controls: Turn Knob (follows the taper even with no dirt), Taper (Linear, Log A, Reverse Log C), Circuit (Level, or Tone with 250k + 22 nF treble cut), Dirt, Wear Position, Worn Arc Width, Scratch, Circuit Bias (makes scratch audible on silence), Vibration. Holding the knob still stops the scratch.

## Two slots, order and linking

Slots run in series, A then B or B then A with Swap Order. A downstream open contact interrupts the upstream source's artifacts, and a downstream pot filters them. It is a plausible series interaction, not an exact model of one combined device.

Picking a third source asks which slot to replace, nothing is dropped silently. Choosing the other slot's source swaps them. Bypassing a slot keeps its settings and is an exact identity. Its physical time keeps running silently, so un-bypassing does not replay old events. **Linked Gesture** sends one movement to both slots, each keeping its own contacts, wear and random stream. Both slots share one mains phase and a global noise budget.

## Environment tab

Sets the rig's surroundings, shared by both slots. Everything is off or quiet by default. Controls that do nothing in the current setting are greyed out with the reason in the tooltip.

- **Mains** (50/60 Hz) and **Hum**: the field around the rig. It only reaches the sound through an exposed path (open or partly inserted jack, touched tip, broken shield). Resistive pickup keeps the round line shape, capacitive pickup (finger, floating tip) is brighter. Hum is never retuned to the song.
- **Buzz Band** and **Buzz Source**:
  - Supply: rectifier pulses, twice per mains cycle (100/120 Hz buzz).
  - Dimmer: phase-control dimmer, odd harmonics of the mains, loudest near half brightness, silent at fully on or off. **Dimmer Kind**: Cheap Wall (fast, harsh) or Stage / TV Studio (choked, softer).
- **Interference** (RF) and **Transmitter**: AM Station (distant broadcast, synthetic whispered syllables, never real speech), Two-Way Radio (keyed transmissions with thumps, the default), Motor Sparks (commutator crackle). RF noise falls as the square of the field, so -10 dB of RF gives -20 dB of noise.
- **Other Gear** (ground loop): current through the shield times its resistance becomes hum in series with the signal. 2-prong gear leaks a small capacitive current. Grounded gear uses **Outlets** (Same Strip, Same Room, Across Building, Cable-TV Line). **Cable Run** and **Cable Shield** set the loop resistance. **Interface** is the receiving input: Unbalanced, Balanced (about 30 dB rejection at mains), Balanced + Isolator (about 70 dB, thin high buzz left), Balanced + Input Transformer (about 100 dB). The readout shows the measured hum after the interface.
- **2G Phone**: Away (nothing), Check-in (a few bursts), Incoming (handshake clusters before the ring), In Call (continuous). **Phone Distance** makes it fall off steeply.

**Things to try:** Breathing Ground Loop (Cable Handling with Shield Damage, shake the cable), Mains-locked wah (Buzz Source Dimmer, automate Dimmer), Honest noise filter (automate Interface to Balanced + Isolator), Phone rhythm (switch 2G Phone to Incoming on a bar line).

## Gesture lane

Two lanes, A drives slot A, B drives slot B, 1 to 64 steps each. Lengths may differ for polymeter. Each step has gesture, intensity, accent, ratchet (2 to 4 retriggers; too-fast ones are merged and logged as *motion limit*), probability, duration, position target, microtiming, Studio send and choke. The lane schedules gesture **starts**, the micro-events inside stay physical. Swing, Humanize, Density, Evolve (seeded per-bar variation) and Fill (or MIDI note 63) shape it.

**Gap** (step menu, Loose Jack and Dirty Pot lanes) makes one step a real gesture: the jack is pulled open, or the pot turned to minimum. The pot Gap needs the Level circuit. For hard gating use the Studio Rhythm Gate.

Both lanes share one clock. They repeat together after the least common multiple of their lengths (A at 16 and B at 12 repeat every 48 steps). The maximum combined cycle is 64 x 63 = 4032 steps. The clock line shows the position, e.g. `cycle 17 / 48`.

**Keys**: arrows move, Space toggles, +/- intensity, A accent, R ratchet, G gesture, P probability, C choke, Ctrl+C/V copy and paste, Shift+arrows shift, Ctrl+Shift+arrows rotate, Delete clears, Ctrl+Z/Y undo and redo.

Timing follows the host position, so bars, loops and seeks are deterministic. Without a running transport press **Run**, which uses Internal BPM.

## Seed, Lock and New Take

Every random decision comes from Seed, Take, source, slot and the absolute bar position. With **Lock** on (default), the same host position and settings give identical audio for any buffer size, bounce or loop pass. Unlocked, each transport start draws a new take. **New Take** keeps positions, timing and fault maps but changes micro-events. Worn arcs and weak spots depend on the Seed only. Reproducible for the same sample rate and version. A different sample rate gives the same gestures but not a bit-identical waveform.

## Studio chain

Runs **after** the modelled hardware and is always labelled Studio. Studio source: Artifacts, Breaks, or Both. Fixed order: EQ, Contour, Drive, Crush, Resonator, Rhythm Gate, Echo, each with on/off and wet. Feedback is a bounded loop from after the Resonator back into Drive, zero by default. **Kill Feedback** clears it. If the amount would make the loop unstable it is clamped and the knob shows *Limited by loop stability*. **Raw Hardware** bypasses Studio, feedback, excitation (unless Keep Excitation is on) and the hidden extra, so the output is exactly the modelled hardware. Monitor chooses what leaves the plug-in and never changes the model. Latency is 0 samples.

## Render and record

**Render WAV** renders the gesture and excitation path with a cloned engine at a fixed tempo. It does not include host audio. Options: 1 to 16 bars or 0.1 to 30 s, 44.1/48/96 kHz, 16/24-bit PCM or 32-bit float, mono or stereo, optional tail and peak normalisation. Silent results are explained before anything is written. Existing files are never overwritten without confirmation. Names follow `Interference_<preset>_<seed>_<length>_<bpm>bpm.wav`, with an optional sidecar `.txt`.

**Record** captures the selected Capture Tap while you play, through a lock-free buffer written by a background thread.

Render replays the lane and the slots' gestures from current settings. It does not capture live performance (dragging illustrations, turning knobs, automation). To keep a performed gesture, program it in the lane (the *Touch and Turn* preset) and render, or use Record.

## Presets

Factory categories (27 presets in 7 categories): Repair Faults, Rhythmic, One-Shots, Subtle Texture, Wild (Expressive), Machine Pattern, Environment. Environment examples: *Breathing Ground Loop*, *Loop Fixed: Isolator* (an A/B pair), *Dimmer Wah*, *Phone Rings Nearby*. *Cable-TV Hum Monster* is in Wild.

User presets are `.ifxpreset` files in `Documents\Interference\Presets` (macOS: `~/Documents/Interference/Presets`). They store every parameter, seed, lock, take, sources, lane and the Natural/Expressive label, never paths or audio. Old presets load safely: missing values take defaults, unknown ones are ignored, bad ones are repaired.

**Reset** returns sound settings to default (MIDI mappings are kept). **Randomize** resets, then picks new safe Natural settings. **Natural** (default) applies strict physical caps. **Expressive** allows faster gestures, denser bursts and louder excitation, and is still causal.

## Levels

Factory presets are level matched. Create-mode excitation and Studio Drive, Resonator and Feedback can add level, so watch the meter and use **Ceiling**. Ceiling is an explicit, audible peak ceiling: stereo-linked gain rider with instant attack and about 45 ms release, then a final clamp, no latency. Off means no limiting. Level changes are smoothed over about 10 ms. Bypass is exact.

## MIDI and automation

See `Docs/MidiMap.md` for notes, controllers and MIDI learn. Right-click any control to learn it. Some hosts do not send MIDI to audio effects. Use the Play buttons, Trigger A/B automation or the Gesture Lane instead. A sidechain needs a host that exposes VST3 sidechain buses. Without one, Sidechain React shows *no sidechain*.

## Help and debug

Help (`?` or F1) covers every feature, version, licence, homepage, support address, install and uninstall, and preset folder troubleshooting. **Help > Debug** opens a live data window, a *create log file on crash* switch (off every load), a troubleshooting export and a hard reset. Files land in `Documents\Interference\Support\`.

## Compatibility

| Platform | Status |
|---|---|
| Windows 10/11 x64, VST3 | Built and validated (pluginval, host contract tests). DAW load not yet verified, see `Docs/QA.md` |
| Windows x64 stand-alone | Built |
| macOS (VST3/AU), Linux, CLAP | Not built for 1.0. The DSP core is portable |

Sample rates tested offline: 44.1, 48, 88.2, 96 and 192 kHz. Mono and stereo, optional mono or stereo sidechain. Latency 0 samples.
