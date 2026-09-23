# Interference VST: industrial performance feature

**Read with:** `INTERFERENCE_VST_CLAUDE_CODE_MASTER_PROMPT.md`, `INTERFERENCE_VST_REALISM_CONSTRAINTS.md`, `INTERFERENCE_VST_CREATIVE_FEATURES.md`, and `INTERFERENCE_VST_EFFECTS_CHAIN_AND_ROUTING.md`. The realism constraints still govern the four hardware sources. This document adds a way to arrange those faults into forceful rhythms. Visual style comes from the user's separate style specification.

## 1. The feature: Machine Pattern

Create **Machine Pattern**, a performance layer that turns physically plausible fault gestures into repeatable, aggressively shaped patterns. Industrial musicians can put the plugin on drums, bass, guitar, field recordings, or a silent instrument channel using explicitly enabled internal excitation. The pattern triggers source-specific movements, damage-region traversals, and Studio send changes; it never inserts unmotivated crackle samples into the raw-hardware path. The result can be a violent drum accent, unstable mechanical pulse, or evolving breakdown while maintaining the source models' realism.

Build Machine Pattern as an extension of the existing Gesture Lane rather than a second independent sequencer. Existing Play/Hardware/Create modes stay intact; `Machine Pattern` is a lane view with extra controls. Its introduction cannot silently change an existing preset's gesture timing or turn on the Studio chain.

## 2. Event lane and feel

Support one lane per loaded source, A and B, with a shared 1–64 step timeline (one to four bars at 16th-note base; support 1/8, 1/16, 1/32 and triplet divisions). Each step can contain `Gesture`, `Intensity`, `Duration`, `Position Target`, `Studio Send`, `Probability`, and `Accent`. Event type is source aware: Jack `Wiggle/Seat/Pull`; Touch `Tap/Hold/Release`; Cable `Bend/Shake/Tap` (mechanical tap audible only if Mic enabled); Pot `Turn-to-Angle/Sweep/Hold`. A lane can be muted or soloed, and `Link Motion` may trigger a single gesture in both source lanes while preserving independent fault state. Editing must be keyboard-accessible with copy/paste, shift, rotate, clear, and undo. A step's probability changes whether a *gesture* occurs using a seeded per-loop decision; microscopic event distributions remain the hardware engine's job.

`Accent` scales gesture force and a separate post-chain Studio send within bounded values. It does not circumvent contact prerequisites or make a silent healthy circuit produce a pop. `Duration` sustains position or motion; on ending, obey source-specific release behavior. `Position Target` can aim at a known worn region, making repeated rough accents possible without requiring identical subevent waveforms. `Ratchet` permits 2–4 gesture retriggers inside a step if each can complete under the source's mechanical speed and recovery limits; otherwise show `Motion limit reached` and coalesce events. Positive and negative microtiming offsets are bounded and seeded. Include a simple `Swing` on offbeats that schedules gesture starts without quantizing individual crackles. Expose `Density` as probability/number of eligible gestures, never as unconditional noise generation.

An `Evolve` option changes seeded gesture choice/probability on a per-bar basis while preserving pattern positions and the fault map. `Lock` makes the whole performance reproducible across DAW bounce, transport loop, render and session reopen. `Fill` momentary control temporarily selects a denser variation and returns on release or next-bar boundary; no permanent overwrite of the main pattern. Support MIDI note trigger for one-shot pattern, note-off policy, and host automation of Fill, Pattern Level and Studio Send. With no host tempo or transport, use explicit internal BPM and stateful start/stop, with visible fallback indicator. If a host sends timing discontinuities, restore phase from musical position rather than simply stepping the PRNG.

## 3. Cross-rhythm controls

Two source lanes may play different lengths, e.g. A at 16 steps and B at 12 steps, but maintain an explicit common bar/step clock. Limit a full combined cycle to a documented maximum; phase after transport seek must be deterministic. The UI should show `A: 16`, `B: 12`, and a loop-phase indicator. A `Choke` condition may stop the **Studio artifact tail** at a programmed step with a short click-free envelope; it must not falsify hardware contact behavior or silently interrupt the track's clean path. A `Gap` step can deliberately hold a Jack open or pot at minimum using a valid physical gesture, not a generic gate hidden under a hardware label. If a user wants a hard rhythmic gate, use the explicitly labeled Studio Rhythm Gate in the effects-chain spec.

## 4. Practical patches

1. **Broken snare:** Insert on a snare bus. Jack wiggle on the second and fourth beats, cable bend on sparse fills. Use Studio artifact Drive and a high-pass before Crush; keep clean snare dry path intact. Changing jack position to fully seated must remove source fault events while Studio tails finish according to routing.
2. **Machine percussion:** On a silent channel enable internal filtered buzz or test excitation. Pot sweep over a damaged arc on sixteenth-note pattern, intermittent cable tap with Mic enabled. Resonator tuned to a selected note can make artifact strikes ring. Raw Hardware comparison removes its pitched tone and post processing.
3. **Breakdown:** On a bass track, stagger a Jack pull and Touch hold. Studio local feedback grows only during the pattern's accents, then Rhythm Gate closes it. The original bass dry path remains available and the feedback network is bounded; Touch alone never becomes an invented synth oscillator.
4. **Controlled chaos:** Two 16/12 step lanes, high probability variations and a locked seed. The long-form cycle evolves in an intelligible pattern; repeated project export yields identical audio at the same transport start and state. A new seed creates a new take without moving every programmed step.

Include four factory presets demonstrating these uses, with safe gain staging and generic descriptive names. Do not imitate or advertise a named artist's signature sound. A/B compare, dry mix, source bypass, Raw Hardware bypass and MIDI performance must remain usable while the pattern is on.

## 5. Scope and implementation boundaries

Machine Pattern controls only source gestures and Studio sends/chokes. It does not replace a DAW drum sequencer, create a third source, install a sampler, or simulate a real machine beyond the four source models. Pattern data is bounded, versioned and embedded in preset/project state; never allocate per-step objects or sort the event list on the audio callback. Compile an immutable event table on an appropriate thread and hand it to DSP safely. Probability and timing are seeded by absolute musical cycle + lane + step index; no unstable per-block RNG dependence. Tail/choke commands affect only Studio signal and optional explicit wet core path, never the host dry bus. Test odd time signatures, host loops, tempo automation, mono/stereo, silence, 12-versus-16 lengths, ratchet limits, Fill release and project reopen.

## 6. Research and provenance

This is a design proposal informed by artist production interviews that describe purposeful distortion and rhythmic sound design, plus research on musician control of unpredictable electronic systems. It is not a claim that any particular artist used this effect or that the entire pattern is an analog hardware process. John Fryer's and producer/engineer interviews describe heavy distortion in industrial-related work; the production account of a Nine Inch Nails recording describes distortion and ring modulation applied to a snare. Retain the existing research traceability for the physical source models and DSP.

- https://www.soundonsound.com/people/4ad-nine-inch-nails
- https://www.soundonsound.com/techniques/classic-tracks-nine-inch-nails-closer
- https://nime.org/proc/nime2021_31/index.html
