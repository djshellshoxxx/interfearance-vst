# Interference VST: physical realism and musical constraints

**Companion to:** `INTERFERENCE_VST_CLAUDE_CODE_MASTER_PROMPT.md`  
**Instruction to Claude Code:** Read and implement this specification alongside the master prompt. Where this file is more restrictive, this file governs realism, source dependencies, and user-visible constraint behavior. Follow the project's separate visual style specifications for colors, typography, and branding.

## 1. Product rule

Every audible fault must have a plausible cause and a bounded response. A user can play or sequence a cause, but the resulting crackles, interruptions, hum, and tonal changes must still obey the selected physical model. Users may emphasize a plausible effect for musical use. They may not turn a disconnected cable into an unlimited oscillator, a stationary healthy pot into a scratch generator, or any source into undifferentiated broadband static while the GUI continues to call it realistic hardware.

Provide two range policies, sharing the same DSP and controls:

- **Natural** is the default in Hardware, Play, and Create. Dependencies and energy caps are strict. Factory presets marketed as realistic must use it.
- **Expressive** grants a *bounded* extension to gesture speed, fault frequency, event prominence, and internal excitation; the causal dependencies below still hold. Mark Expressive clearly on preset save/export. It is an artistic extension of the model and should not be described as measured hardware behavior. No hidden full-range "anything goes" mode.

Changing policy must not silently rewrite stored knob values. Show the effective value and a small `Limited by ...` explanation when a request exceeds what the current circuit can produce. Smooth transitions between policies without a false contact event. In both policies, fixed hardware condition may be animated by a deliberate gesture; tempo grids schedule gestures, not the microscopic electrical events.

## 2. General invariants the engine must enforce

### Causality and conditional silence

1. A healthy closed contact in a shielded, terminated circuit does not produce audible contact crackle at rest. A source can produce quiet stationary hum only when its exposure/interference conditions permit it.
2. With silence at the effect input, internal Excitation Off, Bias/Charge Off, Touch Off, Mic Off, and Hum/Interference Off, output is silence. Test this for every source, pair, circuit mode, and wet mix.
3. Events follow movement, contact condition, wear position, pressure, or a specifically enabled environmental excitation. No source emits evenly distributed unrelated clicks merely because Density is high.
4. Contact attenuation acts on the through signal. Pops arise from signal interruption or a modeled bias/charge state. Handling audio comes from its separate enabled mechanical pickup path. Those buses stay distinguishable for testing and UI monitoring.
5. An open signal path does not preserve full-strength through audio; a totally shielded path does not produce strong mains hum. Do not bypass those dependencies to reach requested output levels.
6. DC offsets from user input and internal charge are bounded and removed where appropriate after the modeled event. Stereo shares physical movement unless a stereo-specific circuit is explicitly modeled; avoid independent random full-channel dropouts on an ordinary single mono TS contact.

### The meaning of each control

- **Movement/gesture** determines when the hardware is disturbed.
- **Wear/Dirt/Damage** determines where and how susceptible it is.
- **Pressure/Insert/Touch** changes contact or coupling state.
- **Intermittency/Density** adjusts *eligible* event probability and clustering. It cannot invent an eligible event when prerequisites are absent.
- **Hum/Interference** adjusts an environmentally coupled source subject to exposure and shielding.
- **Pop** adjusts the bounded bias/charge response where transitions occur; it cannot generate unrelated beat-synced pops without a transition.
- **Mix/Output/Source Level** adjusts audible balance. It does not change whether a physical event is possible.

Never silently implement one control by scaling an unrelated noise file. `100%` means the highest credible value allowed by the selected circuit and range policy, not an unlimited waveform amplitude. Musical intensity should preferentially increase the frequency of **qualified gestures** and the audible mix of qualified events, rather than defeating the physical rules.

### Calibrating numeric limits

Do not invent a universal milliseconds/dB specification and call it measured. Use documented, adjustable per-model *engineering caps*: maximum event rate, maximum continuous open duration, transient peak energy, injected-noise power relative to through-signal, high-frequency bandwidth, motion acceleration, and background-level ceiling. Select initial values by listening and compare against controlled recordings; record device and gain staging. The implementation must nonetheless have finite protective clamps from day one. Define all thresholds in a centralized `PhysicalLimits` table with Natural and Expressive columns, units, empirical status, and a test for each bound. A cap may be revised after measurements, but the causal invariants cannot be disabled. Distinguish digital safety limits from the calibrated perceptual range.

## 3. Loose Jack limits

**Physical scope:** a specific analog tip/sleeve jack and its input circuit. A switching jack, balanced TRS, phantom-powered connection, and powered guitar electronics have different behavior; do not imply this one model exactly covers them.

| User action or condition | Permitted response | Response to suppress or constrain |
| --- | --- | --- |
| Plug fully seated, healthy contacts, little movement | Near-transparent through path, with source-appropriate cable loading | Continuous loud crackling or repeated dropouts |
| Plug moved through unstable engagement region | Clusters of variable contact resistance, short partial attenuation, momentary opens and recovery | Evenly spaced identical clicks unrelated to position or movement |
| Plug fully withdrawn | Loss of through signal; exposed input may pick up interference according to termination/shielding | Full-level original signal persisting through a truly open contact |
| Wiggle over a localized worn region | Repeatable *location* of failure with variable, seeded microevent detail | Failure occurring just as often at pristine positions |
| Grounded contact state | Quieting or changed noise path as supported by circuit mode | Treating every interruption as maximum hum |
| `Pop` raised while bias/charge is zero | Only signal-gating click inherent to an interrupted live signal | Large synthesized charge pop on a silent uncharged circuit |

Enforce a position-dependent contact probability and hysteresis. `Wiggle` can revisit the transition zone, but cannot create an open circuit at arbitrary distance from the jack unless `Damage` specifically represents a damaged internal contact. If `Intermittency` is high at a healthy fully seated position, show `Contact stable here`; offer a guided `Move toward worn area` action. Brief opening/closing may be strong, but sustained rhythmic gating of a healthy seated plug requires actual automated movement through an unstable region. A disconnected plug can admit environmental hum if its destination circuit is left open; do not make hum mandatory.

## 4. Exposed Plug Touch limits

**Physical scope:** high-impedance conductor, body/environment coupling and termination. It does not model the human body as an autonomous musical oscillator.

| User action or condition | Permitted response | Response to suppress or constrain |
| --- | --- | --- |
| Exposed, high-impedance tip touched | 50/60 Hz environmental hum, limited harmonics, optional circuit-specific buzz; modest onset/off contact ticks | Arbitrary pitched melody or broadband white noise driven solely by Touch |
| Properly terminated and shielded conductor | Little pickup; potential mild contact tick if circuit supports it | Hum as loud as an exposed floating input |
| Untouched conductor | Exposure-dependent ambient pickup, generally weaker or otherwise different than a strong finger coupling | Full-strength finger-coupled buzz with Touch at zero |
| Finger held stationary | Stable or gently drifting hum/buzz subject to environment; no repeated touch onset events | Continuous dense scratch from a motionless finger |
| Finger presses/releases or moves | Change in coupling/contact chatter around transitions | Identical high-energy pop at every small pressure adjustment |

Keep mains phase continuous across blocks and held touch; changing frequency 50↔60 Hz needs a short controlled transition. `Body` changes the coupling/filter shape and perhaps harmonic balance within a plausible range, not an independent oscillator pitch. `Shield` and `Circuit Exposure` must actually reduce pickup. RF-like interference is optional and separately sourced; label it `Interference`, not inevitable human touch noise. In Create mode, Excitation can reveal the sound, but label the added source plainly. If Hum is off, any residual touch event should be brief and dependent on contact/bias, not a substitute hum generator.

## 5. Cable Handling limits

**Physical scope:** electrical faults from flex/strain and optional mechanical rubbing/tapping heard through an enabled microphonic/contact-pickup path. A good insulated cable is not automatically a percussion instrument just because it is shaken.

| User action or condition | Permitted response | Response to suppress or constrain |
| --- | --- | --- |
| Sound cable, shield intact, `Mic` zero | Little or no handling sound, even under ordinary movement | Loud rustles and electrical outages |
| Bent across damaged conductor or shield point | Clustered contact loss or increased interference connected to actual fault location | Unrelated clicks distributed equally along cable |
| Repeated shake of damaged cable | Dense but motion-linked clusters with change as motion reverses or traverses the defect | Uniform static at a constant level when motion stops |
| Rubbing cable with `Mic` enabled | Textured friction tied to speed/contact and resonances of the modeled pickup | The same friction volume regardless of movement speed |
| A discrete tap with `Mic` enabled | Short attack and decaying mechanical response | Long unpowered electronic tone without feedback/excitation |

Track bend position, strain and a persistent weak-spot map. Damage of shield and conductor are separate fault paths: shield failure can admit hum while conductor remains live; conductor opening interrupts the input. If both are enabled, combine those effects by signal-path order. Stop friction when motion stops, allowing only an acoustically plausible resonance tail. Mechanical handling sounds require `Mic` or another explicitly modeled transducer; an ordinary sealed cable has no default amplified scrape. `Shake` may be tempo scheduled, but internal contact events vary according to velocity/position rather than trigger grid alone.

## 6. Dirty Pot limits

**Physical scope:** finite rotary track, wiper contact, chosen level or simple tone circuit; no claim that every component's topology and wiper material sound identical.

| User action or condition | Permitted response | Response to suppress or constrain |
| --- | --- | --- |
| Clean pot turning | Level/tone curve changes according to selected taper/circuit | Crackle without contact fault |
| Dirty pot turning through worn arc | Localized, clustered scratch/resistive interruption coupled to audio and possible circuit bias | Scratch of identical severity at all positions |
| Dirty pot held still under normal conditions | Static setting, perhaps a stable resistive error; no new scratch | A perpetual scratch loop |
| Pot rapidly turned | More traversals of faults, within finite mechanical movement speed and arc | Instant unbounded angle oscillations or infinite clicks per sample |
| Silent input and zero circuit bias | Usually no audible scratch | Full-volume scratch independent of signal and excitation |

`Taper` influences nominal wiper law; `Dirt` and `Wear Position` influence failure. `Scratch` scales fault prominence only for valid movement/circuit conditions. `Turn` stays within its finite arc; automatic gestures reverse at boundaries rather than wrap the wiper discontinuously. For Tone mode, identify the topology used and ensure movement changes modeled filter behavior; a level-only pot with a post hoc EQ label is unacceptable. A strong scratch on silence is allowed only with an explicit powered/bias/noise source in the selected circuit, and must be labeled accordingly. Maintain a stable wear map with occasional event variation, so a user can intentionally revisit a scratchy angle.

## 7. Interactions among two sources

Any pair of **different** sound types is permitted in either order, but the result must obey a defined signal path. A downstream open contact interrupts upstream through audio and upstream injected artifacts; downstream injection can still be audible if its own circuit conditions permit it. A downstream dirty pot may attenuate or filter an upstream touch hum according to its circuit setting; the reverse order is different. A healthy cable-handling module adds essentially nothing unless its mic path is enabled. Both sources may receive a linked gesture but retain their own susceptibility map and random substream.

Prevent double-counting a single environmental source: by default the pair shares a mains phase and environment setting. Each module has its own coupling and shielding, so two exposed pathways can both admit hum, but two independent full-scale hum generators should not automatically sum just because two slots are active. A global noise budget caps their combined injection; dynamic headroom protection must be explicit and preserve quiet realism. Similarly, simultaneous pops caused by one linked gesture should be correlated with plausible simultaneous contact transitions, not two identical layered sample impulses.

GUI must show an intuitive reason when a requested combination produces little sound: `No input reaches this open contact`, `Cable mic is off`, `Pot is stationary`, `Circuit is shielded`, or `Excitation is off`. Users can then change the relevant physical cause, change order, enable a transducer, or enable Create-mode excitation. Do not secretly add audio to satisfy a louder output request.

## 8. Musical leeway without breaking the model

Expressive policy may permit faster plausible repeated gestures, more wear, slightly enlarged transient dynamics, denser bursts near a fault, and a louder *explicit* internal excitation. Rhythm sync controls onsets, duration and intensity of gestures. It must never retune mains to musical notes, quantize individual microcrackles, create a scratch without moving a pot, or continue an open-circuit through signal. A `Pitch` control, if added later, must be clearly marked as a post-effect transformation; it is not a property of mains hum or worn copper. Any post-effect reverb, delay, distortion, granular stretch or limiter likewise needs a separate labeled stage and must not be advertised as contact physics.

In Create mode, the user may turn on Noise, Buzz, Test Tone, or Charge excitation and make useful one-shots or loops. The UI must visually distinguish the **excitation signal** from the **fault response**. An exported file or preset marked Natural should carry only physically permitted artifacts under its chosen circuit; if an expressive excitation or post-transform is used, disclose that in preset metadata/manual. Preserve exact timing and seed reproducibility even when the underlying microscopic events are irregular.

## 9. UX implementation requirements

Illustrations and widgets must correspond to actual source state. Make impossible combinations difficult to dial in: gray out irrelevant controls with a concise reason, use ranges appropriate to the selected circuit, and show effective-value indicators when clamps apply. Do not remove a saved parameter when it becomes temporarily irrelevant; keep it dormant and restore it when prerequisites return. An optional `Why is it quiet?` action explains the active limiting dependency in plain language. For source switching, source order, policy switching, and bypass, avoid abrupt unrelated pops and preserve documented transport/seed state.

The central illustrated control should provide an accessible alternative slider, meaningful label, keyboard operation and tooltip. Status animation must reflect measured/actual internal states and must not fabricate wiggles or contact flashes to make a quiet preset appear active. Provide `Natural/Expressive`, `Excitation`, and `Mic` labels prominent enough that users know what generates the audible sound. Keep audio bus meters separate for through signal, electrical injection, and mechanical injection in the Details panel. Save effective policy, physical inputs, maps/seed and any stage labels with presets and exports.

## 10. Verification and release gate

Implement deterministic tests of every table row above at representative sample rates and block sizes. Assertions should examine both **audible output properties** and **state/event telemetry**: e.g. stationary pot has no scratch events, a healthy seated jack has no open events, a fully open path suppresses through audio, exposed touch adds coupling-dependent mains without creating independent pitched tones, a good cable's electrical and mechanical buses stay quiet. Exercise all six distinct pairs in both orders. Property tests sweep normalized parameter ranges and modes to detect a limit that can be bypassed by extreme combinations. Test UI disabled states and `Why is it quiet?` explanations against actual engine dependencies. Verify Natural presets by listening and measurements on real reference captures as available. If references are unavailable, call the result **research-informed** and document the unverified realism claim; do not invent a measured-accuracy claim.

### Research traceability

The connector-fretting papers motivate persistent health plus brief intermittent transitions. The AES guitar cable study motivates source-dependent capacitance response. The DAFx potentiometer paper motivates separate taper and circuit behavior. The NIME/CHI feedback studies motivate repeatable control of gestures without erasing complex response. These papers do not directly validate the particular ranges or amplitudes in this plugin. See source URLs and full citations in the master prompt. Capture and compare actual jack, touch, cable and pot recordings before making stronger authenticity claims.
