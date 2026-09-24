# Sound design recipes

Thirteen practical recipes for Interference, from invisible realism to rhythmic abuse. Each one lists the goal, the
settings (by the names you see in the plug-in), how to play or automate it, the one-line physics behind it, and a
level note where it matters.

Control names and ranges are listed in [Parameters.md](Parameters.md). Where each control lives is described in the
[Manual](Manual.md). MIDI notes are in [MidiMap.md](MidiMap.md). The research behind the models is summarised in
[Research.md](Research.md).

## Before you start: four habits

- **Natural first.** *Range Policy* (mode row) starts at **Natural**. It keeps every fault within strict physical
  caps. For one exposed path, hum peaks stay around −22 dBFS, pops around −12 dBFS and handling noise around
  −14 dBFS. The room background (ground loop, phone) stays under about −20 dBFS. **Expressive** roughly doubles
  those caps and allows faster gestures: use it on purpose, then check your meters.
- **Put a Ceiling on loud experiments.** In the output strip, turn **Ceiling** on and set **Ceiling Level** to
  −1 dB. It is a zero-latency peak rider (instant attack, about 45 ms release), so a stray pop or feedback swell
  can't clip your track.
- **Hear only the fault.** Set **Monitor** to *Injected Artifacts* to hear just what the hardware adds (hum, pops,
  handling, radio), or to *Fault Difference* to hear just what it takes away. Monitor never changes the sound, only
  what leaves the plug-in.
- **Why is it quiet?** Nothing in Interference makes sound without a cause. If a recipe stays silent, press
  **Why quiet?** on the slot card: it names the missing cause (a still knob, Mic at 0, a shielded input, a
  buffered source, no hum in the room…).

Mains: set **Mains** (Environment tab) to your region's mains frequency. It is 50 Hz in most of Europe, Asia,
Africa and Australia, and 60 Hz in most of the Americas. Hum and buzz are always locked to it and never follow the
song key.

---

## 1. Guitar DI with a breathing ground loop

**Goal:** a clean DI guitar that sounds like it was recorded through a real, slightly faulty cable: low hum that
swells and flutters when the cable moves.

**Set up**
- Slot A: **Cable Handling**. *Bend* 50 %, *Weak Spot* 50 %, *Damage* 0 %, *Shield Damage* 40 %, *Mic* 0 %,
  *Shake Cable* 15 %.
- Details > **Source circuit**: *Passive Pickup*; *Cable Load* 700 pF.
- Environment tab: *Hum* 20 %, *Other Gear*: *Grounded*, *Outlets*: *Same Room*, *Cable Run* 6 m,
  *Cable Shield*: *Foil + Drain*, *Interface*: *Unbalanced*.

**Play it:** automate *Shake Cable* between 0 % and 30 % on phrase endings, or drag along the cable illustration.
When the cable is still, the hum settles. Watch the *Ground loop* readout on the Environment tab: somewhere around
−60 to −50 dBFS sounds like a real, slightly careless studio.

**Why:** the ground current flows through the cable shield, and current × shield resistance becomes hum in series
with the signal. Flexing the broken braid changes that resistance, so the hum breathes.

**Levels:** Natural caps the loop at about −20 dBFS. For a more obvious effect, use a longer *Cable Run* rather
than switching to Expressive.

## 2. Fix it live: switch the Interface during a take

**Goal:** a "fixing the hum" moment you can automate. Loud hum, then a thin buzz, then silence, exactly as real
isolation gear behaves.

**Set up:** start from recipe 1, or load the factory preset *Breathing Ground Loop*. Change *Other Gear* to
*2-Prong High Power*: its leakage current gives a buzzier, odd-harmonic hum, which makes the stages easier to hear.
Raise *Cable Run* to about 20 m so the loop is clearly audible before you start fixing it.

**Play it:** automate *Interface* one step per bar:

| Bar | Interface | What you hear |
|---|---|---|
| 1 | Unbalanced | the full loop |
| 2 | Balanced | about 30 dB less hum |
| 3 | Balanced + Isolator | the hum is gone; a thin, high buzz remains |
| 4 | Balanced + Input Transformer | essentially silence |

The factory pair *Breathing Ground Loop* / *Loop Fixed: Isolator* does the same with the **A/B** buttons.

**Why:** a balanced input or transformer cancels noise that appears equally on both wires. A simple isolator's
rejection falls at higher frequencies, so the upper harmonics (the "buzz") survive longest.

## 3. Mains-locked dimmer wah on a pad

**Goal:** a rasping, vowel-like buzz that you sweep like a wah, but that never changes pitch.

**Set up**
- Slot A: **Exposed Plug Touch**. *Touch* 70 %, *Pressure* 90 %, *Body* 60 %, *Circuit Exposure* 80 %,
  *Shield* 0 %, *Pop* 0 %.
- Environment tab: *Buzz Band* 60 %, *Buzz Source*: *Dimmer*, *Dimmer Kind*: *Cheap Wall*, *Hum* 5 %.

**Play it:** automate *Dimmer* from 10 % to 90 % and back over two bars, or right-click *Dimmer* → **MIDI learn**
and ride a fader. The buzz is loudest and widest near 50 % and disappears at fully on or fully off. Switch
*Dimmer Kind* to *Stage* or *TV Studio* for a softer, darker version.

**Why:** a phase-control dimmer switches the lamp current on part-way through every half-cycle. The fast switching
edge induces a spike twice per mains cycle, which gives odd harmonics locked to the mains.

**Levels:** *Cheap Wall* is bright and edgy. Keep the Ceiling on. The finger also loads the pad slightly (a gentle
treble loss); lower *Touch* or *Circuit Exposure* if you want the pad untouched.

## 4. Phone in the studio: 2G bursts as a ghost rhythm

**Goal:** the famous "dit-dit-dit" of a 2G phone near a speaker, used as an off-grid ghost layer.

**Set up**
- Any source (the default seated Loose Jack is fine). The phone reaches the circuit on its own.
- Environment tab: *2G Phone*: *In Call*, *Phone Distance* 0.4 m.
- Studio tab: *Studio* on, *Studio Source*: *Artifacts*, *EQ* on with *High-pass* 300 Hz, *Echo* on with *Echo Sync*
  on, *Echo Division* 1/8D and *Echo Wet* 25 %.

**Play it:** automate *2G Phone* per bar: *Incoming* on the first bar of a section (irregular handshake clusters),
*Away* in the verse, *In Call* for the breakdown. Automate *Phone Distance* from 2 m down to 0.3 m for a phone that
"walks over" to the desk.

**Why:** a 2G phone transmits in 577 µs bursts, 217 times a second, with a regular gap every 26 frames. Any
semiconductor junction in the audio path detects them as a buzzing rhythm. The rhythm is set by the network timing,
never by your tempo.

**Levels:** the level rises steeply as the phone gets closer. At about 0.15 m it reaches the Natural background cap
(about −20 dBFS peaks). This is the sound of 2G networks; modern phones don't make it.

## 5. Two-way radio breakup on vocals

**Goal:** a vocal that drops out on a bad connection while radio chatter leaks in and echoes away.

**Set up**
- Slot A: **Loose Jack**. *Move Plug* 75 %, *Wiggle* 25 %, *Wear* 50 %, *Wear Position* 75 %, *Shield* 20 %,
  *Short to Sleeve* 0 %.
- Environment tab: *Interference* 70 %, *Transmitter*: *Two-Way Radio*, *Hum* 5 %.
- Studio tab: *Studio* on, *Studio Source*: *Artifacts*, *EQ* on with *High-pass* 400 Hz and *Low-pass* 3500 Hz,
  *Echo* on with *Echo Division* 1/8D, *Echo Feedback* 40 % and *Echo Wet* 35 %.

**Play it:** automate *Wiggle* up on the lines you want to break up. Every time the tip lifts, the vocal drops out
and the exposed tip lets the radio chatter in. The keyed transmissions come and go on their own, with a thump at
each key-up and key-down. Push *Move Plug* to 100 % to "fix" it.

**Why:** a junction in the audio circuit detects radio as the square of its envelope. That is why the effect is so
sensitive: halving *Interference* makes the chatter about 12 dB quieter.

**Levels:** the Echo only repeats the injected artifacts; the vocal itself stays dry. Use *Studio Level* to balance
the echo tail.

## 6. Motor-spark crackle tuned to the song

**Goal:** electrical crackle from a brush motor, ringing at the pitch of the notes you play.

**Set up**
- Slot A: **Exposed Plug Touch**. *Touch* 60 %, *Pressure* 100 %, *Circuit Exposure* 100 %, *Shield* 0 %, *Pop* 0 %.
- Environment tab: *Hum* 0 %, *Interference* 80 %, *Transmitter*: *Motor Sparks*.
- Studio tab: *Studio* on, *Studio Source*: *Artifacts*, *Resonator* on, *Follow MIDI* on, *Mode Ratio* 2.00 x,
  *Res Decay* 0.6 s, *Res Feedback* 20 %, *Res Blend* 70 %.

**Play it:** send MIDI notes in the song's key to the plug-in, avoiding notes 36–39, 48–51, 62 and 63, which play
gestures. Each note retunes the Resonator, so the crackle rings on that pitch. Notes also play the slots' gestures;
set *Slot A Disturbance* to about 20 % if you don't want the finger to move with every note.

**Why:** a brush motor's commutator sparks 60–250 times a second. Each spark is a burst of radio noise that the
audio circuit detects as a click. The pitch comes from the Resonator, a Studio effect, not from the motor.

**Levels:** Resonator feedback is bounded and can't self-oscillate from silence, but a long *Res Decay* adds
level. Keep the Ceiling on, and **Kill Tail** stops the ringing at once.

## 7. Broken-shield cable shaken in the Gesture Lane

**Goal:** a rhythmic, physical cable part: hum swells, crackle and handling rustle, all on the grid.

**Set up**
- Slot A: **Cable Handling**. *Bend* 50 %, *Weak Spot* 50 %, *Damage* 30 %, *Shield Damage* 70 %, *Mic* 40 %,
  *Intermittency* 60 %.
- Details > **Source circuit**: *Passive Pickup* (a buffered source would hold the broken shield down, about 34 dB
  less hum).
- Environment tab: *Hum* 40 %.
- Mode: **Play**. Gesture Lane tab: *Step Division* 1/16, *Swing* 15 %. Put *Shake* steps on 1, 5, 9 and 13 (accent
  on 13), a *Bend* on 11, and a *Tap* with ratchet 2 on 15. Turn *Gesture Lane* on and start the host.

**Play it:** raise *Density* for busier bars, and hold **Fill** (or MIDI note 63) for a denser variation that
returns when you let go. **New Take** keeps the timing but gives you fresh crackles.

**Why:** broken braid exposes the conductor to the mains field, and every movement opens and closes the damaged
shield and strands. With *Mic* on, friction inside the cable also separates charge. With a passive source that
charge is audible as rustle, and more *Cable Load* makes the rustle darker.

**Levels:** in Natural, hum from the exposed conductor peaks around −22 dBFS and handling noise around −14 dBFS.
The grid schedules gesture starts only; the crackles inside each gesture are never quantized.

## 8. Dirty-pot scratch groove

**Goal:** a groovy, scratchy volume pot that grinds against a drum loop, and scratches even in the gaps.

**Set up**
- Slot A: **Dirty Pot**. *Circuit*: *Level*, *Taper*: *Log (A)*, *Dirt* 80 %, *Scratch* 70 %, *Wear Position* 50 %,
  *Worn Arc Width* 0.20, *Turn Knob* 55 %.
- Details: *Circuit Bias* 30 % (a powered circuit, so the scratch is audible even on silence).
- Gesture Lane: *Step Division* 1/8, *Sweep* steps on the off-beats, a *Turn to Angle* step aimed at 50 % with the
  step's position target on the downbeat of bar 2.

**Play it:** or skip the lane and automate *Turn Knob* with a slow triangle between 40 % and 70 %. Stop moving and
the scratch stops; that stillness is part of the groove.

**Why:** scratch is the current through a dirty wiper jumping as it crosses grime on the worn arc. No motion means
no scratch, and a DC bias across the pot turns each jump into a click.

**Levels:** the scratch's depth is capped relative to the signal (Natural). With the Log (A) taper, the level
itself also follows the knob. Use *Output* to make up for it.

## 9. One-shot pops for percussion (Create + Render)

**Goal:** a folder of plug-pop and contact-click one-shots for a drum sampler.

**Set up**
- Mode: **Create**.
- Slot A: **Loose Jack**. *Move Plug* 80 %, *Wear* 70 %, *Wear Position* 80 %, *Wiggle* 0 %, *Pop* 100 %;
  *Recovery* 30 % (Details). *Slot A Gesture*: *Wiggle*.
- Environment tab: *Hum* 0 %.
- Output strip: *Monitor*: *Injected Artifacts*.

**Play it:** press **Play** on slot A to audition. Then use **Render WAV…** with a length of 1 s, 48 kHz, 24-bit,
normalisation on. Press **New Take** and render again for each variation: same timing, different micro-events. For
a longer "pull and reseat" pop, program a *Pull* step followed by a *Seat* step in the lane before rendering.

**Why:** a bias voltage behind a contact only discharges when the contact's conductance changes. A still contact
never pops, and a contact that makes and breaks does.

**Levels:** pops peak around −12 dBFS in Natural and −6 dBFS in Expressive before normalisation. Render never
overwrites a file without asking.

## 10. Lo-fi sampler grit on the breaks

**Goal:** crunchy, bit-reduced glitches that only happen where the signal breaks up.

**Set up**
- Slot A: **Loose Jack**. *Move Plug* 80 %, *Wiggle* 40 %, *Wear* 60 %, *Wear Position* 80 %, *Intermittency* 50 %.
  *Hum* 0 % (Environment tab).
- Studio tab: *Studio* on, *Studio Source*: *Breaks*.
  - *Digital Crush* on: *Bits* 6, *Hold Rate* 6000 Hz, *Crush Wet* 100 %.
  - *Drive* on: *Drive Curve* *Asymmetric*, *Drive* 10 dB, *Drive Output* −6 dB.
  - *Auto Match* on.

**Play it:** use on a drum loop. Automate *Wiggle* for more or fewer breaks. Switch *Studio Source* to *Both* to
crush the pops and hum too.

**Why:** *Breaks* sends the fault difference (what the faulty contact removes from the signal) into the Studio
chain. So the grit appears exactly where, and only when, the contact misbehaves. Crush and Drive are Studio
effects, not cable physics.

**Levels:** *Auto Match* keeps Drive and Crush roughly level-matched within bounded limits. **Raw Hardware** is a
quick A/B against the unprocessed fault.

## 11. A subtle "vintage room" bed

**Goal:** a barely-there sense of old electronics under a whole mix: a faint, steady hum you feel more than hear.

**Set up**
- Slot A: **Loose Jack**, fully seated and healthy: *Move Plug* 97 %, *Wiggle* 5 %, *Wear* 20 %, *Shield* 85 %.
- Environment tab: *Hum* 10 %, *Buzz Band* 5 %, *Buzz Source*: *Supply*, *Other Gear*: *Grounded*, *Outlets*:
  *Same Room*, *Cable Run* 3 m, *Cable Shield*: *Braid*, *Interface*: *Unbalanced*.
- Range Policy: *Natural*. *Dry/Wet* 100 %.

**Play it:** leave it running. The *Ground loop* readout should sit around −60 dBFS. For more, go *Outlets* →
*Across Building*. For less, *Cable Shield* → *Heavy Braid*.

**Why:** two grounded devices on the same room's wiring sit a few millivolts apart. That difference drives a small
current through the cable shield, and it becomes a low hum in series with your signal.

**Levels:** this is meant to be quiet. A seated, healthy jack passes the music unchanged, so the bed is the only
addition.

## 12. Order matters: finger hum through a tone pot

**Goal:** a hum you can "play" with a tone knob, and an ear-training demo of signal order.

**Set up**
- Slot A: **Exposed Plug Touch**. *Touch* 60 %, *Pressure* 80 %, *Body* 70 %, *Circuit Exposure* 90 %, *Shield* 0 %.
- Slot B: **Dirty Pot**. *Circuit*: *Tone*, *Dirt* 10 %, *Turn Knob* 50 %.
- Environment tab: *Hum* 40 %. The order button between the cards shows *A → B*.

**Play it:** sweep *Turn Knob* slowly. The hum darkens and brightens because the tone network sits after the
finger. Now click the order button (*B → A*): the knob only colours the dry signal, and the hum stays bright.

**Why:** a downstream circuit acts on everything upstream, including injected noise, so the order of two faults
changes the sound.

## 13. Buzzing finger taps as a lead line

**Goal:** an aggressive, rhythmic "touch the live plug" buzz, played from the lane.

**Set up**
- Slot A: **Exposed Plug Touch**. *Touch* 0 %, *Pressure* 60 %, *Body* 80 %, *Circuit Exposure* 100 %,
  *Shield* 0 %, *Pop* 60 %.
- Environment tab: *Hum* 60 %, *Buzz Band* 20 %, *Buzz Source*: *Supply*.
- Mode: **Play**. Gesture Lane: *Tap* and *Hold* steps in a 12-step pattern for lane A (*Hold* steps two steps
  long), with *Swing* 20 %.
- Studio tab: *Studio* on, *Studio Source*: *Artifacts*, *Drive* on (*Drive Curve* *Hard*, *Drive* 18 dB, *Drive
  Output* −9 dB), *Rhythm Gate* on (*Gate Mode* *Pulse*, *Gate Division* 1/16).

**Play it:** use *Pattern Level* and accents for dynamics. Let the 12-step lane run against a 16-step lane B
driving a second source for a polymeter that repeats every 48 steps.

**Why:** a finger on a floating, high-impedance tip couples the room's mains field into the input. Making and
breaking contact clicks through the contact's charge. The drive, gate and rhythm are Studio.

**Levels:** Drive into a hard curve adds a lot of level. Start *Drive Output* low, keep the Ceiling on, and try
*Expressive* only if you want faster retriggers.

---

**Remember:** Interference models plausible faults; it does not measure any particular device. Every fault still
needs its cause. If a recipe is silent in your session, **Why quiet?** will tell you which cause is missing.
