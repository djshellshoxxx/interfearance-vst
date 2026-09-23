# Interference VST: effects chain, buses, input and output routing

**Read with all companion Interference VST specifications.** This document is the canonical signal graph. It supersedes shorter or ambiguous routing language in the master and creative features specs. The realism constraints govern hardware models; every effect in this chain is a clearly identified **Studio** processor after the modeled contact. Do not describe Drive, Crush, Gate or Feedback as properties of a faulty cable or jack. Use the user's visual style specifications without creating new brand styling.

## 1. Supported external I/O

- **Main audio input:** mono-in/mono-out or stereo-in/stereo-out effect. Preserve channel count; no implicit mono sum, no stereo duplication, no channel swap. Reject unsupported surround layouts clearly. Accept host silence and zero-length blocks.
- **Optional sidechain audio input:** mono or stereo when host bus configuration allows. Used for React detector and optional Studio ducking/gate key only. No direct audible sidechain feed. If absent, disable routing to it with visible status; never quietly replace it with the main input. Document VST3 sidechain bus behavior and validate it in an actual host. Do not require sidechain for any core feature.
- **MIDI/control input:** gesture triggers, pattern start/Fill, pressure/CC and optional post-resonator note pitch. MIDI itself never carries audio. If the host denies MIDI for effects, GUI/host automation supplies equivalent controls. Stable parameter IDs and safe message-to-audio handoff.
- **Audio output:** mono or stereo equal to main input layout; a host with no main audio may use Create internal excitation only if the plugin instance has a supported mono/stereo output bus. Do not pretend the effect is a dedicated synthesizer in hosts that reject a no-input effect.
- **Export output:** off-thread WAV file created from monitored/captured audio or cloned offline render, with explicit tap choice and tail policy. No automatic files on disk during ordinary plugin processing. User chooses path; no overwrite without confirmation.

## 2. Canonical graph and named buses

Use these names in code, docs, UI meters and tests:

```
HOST MAIN IN x --------------------------> [dry compensation] -------------------------\
      |                                                                            [dry/wet + output trim] -> HOST MAIN OUT y
      +-> [Input Trim + source impedance/circuit mode] -> p0                             /
                          |                                                            /
INTERNAL EXCITATION e ----+-> p = conditioned input + explicitly enabled excitation  /
                               |                                                      /
                               +-> [Hardware Slot A -> Slot B, in user order] -> C  /
                               |    (physical inline response + injections)         /
                               +-> [no-injection counterfactual] -> C0             /
                               +-> [healthy-contact reference] -> R                /
                               D = C0 - R; A = C - C0; C = R + D + A               /
                               |                                                      /
                               +-> [Studio selected F = A or D or A+D]              /
                                   -> EQ -> Contour -> Drive -> Crush                 /
                                   -> Resonator -> Rhythm Gate -> Echo                /
                                   -> STUDIO output P                                /
                               [wet reconstruction W = R + U + P,
                                   with U = unselected component(s)] --------------/

HOST SIDECHAIN s -> [detector only] -> gestures and optionally Studio dynamics key
MIDI / GUI / host automation / Machine Pattern -> gesture scheduler -> hardware and Studio controls
```

The above ASCII graph is implementation notation, not a prescribed UI diagram. Every host-borne audible audio sample enters only through main input. Internal excitation is a distinctly labeled studio source. Sidechain affects **control** only. No system microphone, loopback, external feedback port, or hidden hardware dependency is required.

`R` is the same input/excitation passed through the selected healthy source impedance and nominal, undamaged component transfer, including intended static level/tone effects, then the same optional preamp behavior. `C0` uses the *same actual contact states* and signal path as `C`, with electrical/mechanical injected signals set to zero. `C` is the complete physical core result. If nonlinear preamp/saturation prevents a cheap algebraic split, evaluate matched counterfactual paths and measure CPU; do not claim simple addition of source terms gives an exact split across a nonlinear system. This gives the **exact** samplewise identity `C = R + D + A` by definition, subject to floating-point precision. It costs processing but avoids misleading 'noise only' meters. Optimize safely when monitors/Studio are bypassed; the audible default `C` must remain unchanged.

`D = C0 - R` is contact-induced alteration of the main path: attenuation, tone change and signal gating. It may be signed and phase-sensitive. `A = C - C0` is the sum of injected electrical and optional mechanical energy *after* downstream hardware transformation; it includes charge pops where generated. In a perfectly healthy and silent setup these components vanish. Provide optional finer UI meters for electrical versus mechanical by separate diagnostic runs or lineage tracking if exactly implementable, but the two canonical buses above take precedence for mixing. Do not make the `A` bus by simply adding pre-slot noises, as Slot B can attenuate or distort Slot A's contribution.

Choose Studio input `F = A`, `D`, or `A + D` (`Artifacts`, `Breaks`, `Both`). `Both` is calculated `C - R`. Define the unselected component `U`: if F=A, U=D; if F=D, U=A; if F=A+D, U=0. If Studio is bypassed, P=F samplewise, so `W=R+U+P=C` at unity. If enabled, P is the processed selected branch with explicit output level. The separately exposed `Fault Difference` audition selects D; `Injected Artifacts` selects A; `Full Core` selects C; `Studio Only` selects P; `Full Wet` selects W; `Full Output` selects y. These taps must give reproducible results and never silently use the original dry `x` in place of R. Monitoring one tap changes what leaves the plugin; preview and export must use the same semantics.

**Dry/wet:** `y = dryGain * delayedOriginal(x) + wetGain * W` with explicit `Mix` and documented law. Use linear gains for predictable null tests or an explicitly switchable constant-power blend; default linear recommended. At 0% Wet, align original dry sample for reported latency and avoid unexpected tone loss or added excitation. At 100% Wet, output exactly W before output trim and protection. `Input Trim` acts on wet core/excitation path only; expose any dry compensation separately. Output trim applies after mix; optional final protective stage is labeled and testable. True host/plugin bypass must be time-aligned dry audio with well-defined latency/tail policy. Never include sidechain audio in dry/wet.

## 3. Stage order and effect algorithms

Fixed order in v1 (no drag-and-drop reordering); each block has independent bypass and wet amount with declicking. Existing creative features EQ, Contour, Resonator, Echo retain the same function and add three industrial processors `Drive`, `Crush`, `Rhythm Gate`. Signal flow:

1. **Artifact EQ:** high-pass, low-pass, tilt/broad peak. Stereo-linked by default. EQ cannot add sound from zero input. Coefficients smooth with host sample-rate changes.
2. **Transient Contour:** short attack/decay envelope on selected F; preserve body of slow hum when desired, distinguish transient versus sustained material. Clamp gain and avoid reset artifacts on tempo seek.
3. **Drive:** optional saturator with Soft/Hard/Asymmetric curves, pre/post gain, wet, tone, DC blocker, oversampling or validated antialiasing. Asymmetric modes can create DC internally so verify blocker and low-frequency response. Default bypass; level-match preset output. A high Drive setting is a Studio alteration, never an alleged cable property.
4. **Crush:** optional sample-rate reduction with independent bit depth and hold rate controls, distinct from Drive. Bit quantization shall be defined in linear PCM amplitude with optional controllable dither and deterministic stochastic rounding seed; disable dither for null tests. Rate reduction uses sample-and-hold with phase continuity across blocks and defined behavior on BPM changes. Audible aliasing is intentional here, unlike unintended aliasing from the hardware contact renderer or Drive. Label `Digital Crush`; Natural policy can still use it because Natural applies to physical sources, but mark processed result `Studio`.
5. **Resonator:** two damped tuned modes excited by F after prior stages, with frequency or MIDI note, decay, blend and bounded feedback. In bypass, identity. No self-oscillation from zero input unless an explicit Studio-only sustained feedback control is chosen and bounded. Meter stage input/output.
6. **Rhythm Gate:** on the Studio branch only; step envelope, MIDI/sidechain key, or audio envelope key. Choices `Through`, `Pulse`, `Choke`, with attack/release and depth. Closing it must not gate the host dry input or the unaffected hardware wet component U/R. Pulse requires source or tail to be audible; it cannot invent fault events. A Choke step may additionally command a smooth `Kill Feedback`, and may mute or clear Echo tail according to an explicit separate option. Because the feedback tap occurs before the Gate, merely closing the Gate cannot stop that internal loop. Host tempo fallback uses internal BPM and displays status.
7. **Echo:** short filtered, tempo-syncable or free delay with safe feedback, wet, and finite tail. Must accept silence without creating audio (unless prior saved delay energy remains). Echo repeats are **Studio** artifacts after physical events; stopping movement does not imply immediate tail cutoff.

### 3.1 Local Industrial Feedback

An optional Studio-only local feedback send creates sustained/repeating harsh textures from **actual** artifact or fault-difference events. Tap output **after Resonator but before Rhythm Gate**. Feed through a high-pass/low-pass filter, DC blocker, bounded saturator and a minimum one-sample delay (preferably an adjustable rhythmic delay) back into the **input of Drive**, after Contour. `Feedback Amount` is zero by default. The exact tap/destination must be visible in the advanced routing panel. This is an internal feedback loop; do not ask the host to route output to input or use system audio. Solve it with a bounded delay line and a tested gain strategy; all filters and saturation remain stable at legal values, including zero signal, high sample rates, and sudden automation. If stable loop gain cannot be guaranteed in a mode, clamp that combination and show why. An explicit `Kill Feedback` button ramps the return to zero, flushes or fades loop state according to policy, and is MIDI-mappable. Never hide runaway behavior behind a final brickwall limiter. Limit maximum tail length and document freeze behavior separately; no unbounded infinite tail in a normal preset.

### 3.2 Output protection and gain

Use per-stage peak/RMS meters and one master peak meter. Default patches should maintain headroom when two source injections, Drive, Resonator, Feedback and Echo coincide. Offer `Auto Match` for Drive/Crush as a measured gain-compensation utility with bounded gain; compute statistics off-thread/with a safe sliding estimator and do not pump unexpectedly. Optional output limiter is a user-visible `Ceiling` stage with stated lookahead/latency; bypassable except any non-bypassable *numeric* failsafe that only handles invalid internal values. The failsafe must never routinely color normal audio. Test overs and intersample peaks where relevant. Silence stays silent except decaying delay/feedback state or explicitly enabled excitation/hum conditions.

## 4. Routing UI

Main view: `Input` meter with mono/stereo indicators; source A/B in user order; `Studio Source: Artifacts / Breaks / Both`; compact fixed chain of seven labeled on/off blocks; `Feedback` send; `Raw Hardware`; `Dry/Wet`; output meter; `Capture Tap`. A small `Routing` details view shows the named taps R, D, A, P, W, x and y, sidechain detector state, and latency/tail. This screen should reflect live engine route choices and meter truthfully. If only one source is loaded, bypass the empty slot without relabeling the downstream path. Controls that have no eligible sound should display `No artifact to process` instead of pretending to add a hidden source. The plugin remains useful with all Studio effects off.

`Raw Hardware` disables Studio chain, local feedback and internal excitation (unless a separate `Keep Excitation` explicit option is set in Create mode), restores core C, and retains settings for comparison. Enable/disable via short click-free crossfade with delay alignment. Distinguish `Core-only` from full plugin bypass (which emits dry host input, not modeled hardware). Monitor choices do not change physical model RNG, contact state or source order; capture respects explicit chosen tap and the monitor selection shown.

## 5. Capturing, offline rendering and latency

For a live capture, default tap is Full Output y; selectable taps `Full Core C`, `A`, `D`, `P`, `W` and dry x. Buffer tapping must not change audible output or random sequence. Offline self-excitation render uses cloned engine and the same route/timing/seed, with x=0 and s=0 unless the user supplies a properly loaded local sample; do not silently use live host audio or stale sidechain. If render needs a supplied input sample, expose an explicit import path, license/privacy handling and resampling, or defer that feature to a later release. Support render including tails or trimming with user choice; compute recorded duration and report whether echoes were excluded. Export mono/stereo to match selected tap with correct headers, headroom and file naming. `Fault Difference` D may contain negative sample values; export as ordinary signed PCM/float without rectifying it.

Use a constant reported host latency across stage bypasses and routing options, equal to worst enabled topology if necessary, or implement correct dynamic latency updates supported and tested in target DAWs. Parallel paths R/C0/C, P and dry x must be phase/time aligned at reconstruction and mix. Oversampling and lookahead must be compensated before calculating D/A if latency differs. Tail reporting takes Echo, local feedback and resonator decay into account; `Kill Feedback` does not automatically erase Echo tail unless an independent `Kill Tail` control is chosen. Host freeze/offline playback must be deterministic from serialized state and explicit start position.

## 6. Interaction with the realism constraints

Physical source constraints apply **before** the Studio chain. Drive/Crush/Gate/Feedback can make an eligible quiet fault violent, robotic, harsh or sustained; their status is visible and their output must be labeled processed. They cannot cause an impossible source event in the core or justify misleading claims of raw analog realism. With healthy hardware, no input and no internal excitation/hum, local feedback starts silent. With a previously excited local loop, an effect tail may persist, but must be finite by default and killable. Bypassing Studio returns the exact C branch at matched latency and documented gain. No post effect may write to the source fault state or contaminate the reference buses unless an explicit control mapping schedules a **valid physical gesture**.

## 7. Validation matrix

Run impulse/silence and musical material through every I/O layout and monitoring tap. Test algebraic nulls at Studio bypass: `C-(R+D+A)` and `W-C`, relative to floating-point tolerance after alignment; at 0% Wet, `y` equals delayed host dry x after output gain. Test nonlinear preamp with counterfactual buses, two source orders, source bypass, single/both A/B, internal excitation, and sidechain absent/present. Verify neither sidechain nor Studio-only feedback leaks to the host dry branch. Automated QA: each chain block bypass identity; all-stage enabled peak/CPU; abrupt parameter changes; stereo phase; Drive aliasing; Crush expected quantization/hold behavior; resonator and local feedback stability; Rhythm Gate never mutes clean dry; Echo finite tail; plugin bypass/capture/export; host latency and buffer-size changes; project reopen with a locked seed. Do listening tests for snare, bass, guitar and silence. If a stage is not stable or the required host routing is unverified, list it as a release blocker rather than silently disabling it in a commercial preset.

## 8. Design evidence

The genre use case comes from artist/engineer production accounts of deliberate drum distortion, ring modulation and heavier guitar/drum processing. Feedback is a known musical practice, but this local DSP loop is a designed Studio effect; it is not measured imitation of a specific machine. DAFx research on nonlinear processing and stable feedback networks informs numerical implementation. See https://www.soundonsound.com/techniques/classic-tracks-nine-inch-nails-closer ; https://www.soundonsound.com/people/4ad-nine-inch-nails ; https://www.dafx.de/paper-archive/details/vem_XXF5qBbfiWOH2RVVAA ; https://www.dafx.de/paper-archive/details/tLDBzbMbZN8aoERSiwvSpg .
