# Research basis — Interference 1.0

Interference is **research-informed and empirically calibratable**, not a component-accurate circuit simulation and
not a measurement of any particular jack, cable, finger or potentiometer. The papers below support the *structure* of
the models. None of them establishes universal numbers for contact timing, pop spectra, skin impedance or a
specific connector, so every numerical default in `Source/Core/PhysicalLimits.h` and in the source models is a
**listening-test starting point, documented as tunable**.

Each entry is labelled:

- **Observed**: what the source reports.
- **Inference**: the design decision drawn from it.
- **Artistic**: a deliberate enhancement that goes beyond the source.

## What was available

This was an unattended build without institutional library access. For the papers behind the four source models
(the first seven rows) only the **abstract and bibliographic record** were read (publisher or repository page, or a
search-engine abstract where the page refused automated access); no full text was used for them. The later additions
(environment, RF and handling, and the Studio chain provenance) used the full text wherever it was openly
available. The table says what was available for each source.

| Paper | Available | Used for |
|---|---|---|
| Maul, McBride, Swingler 2001 | Abstract (IEEE T-CPT 24(3):370–377; Southampton ePrints page refused automated access) | Contact model: slow drift + rapid intermittences |
| Maul, McBride 2002 | Abstract / bibliographic record (48th IEEE Holm Conf., pp. 165–174) | Contact model: short-duration discontinuities |
| Paiva, Penttinen 2011 | Abstract (AES 131st Convention; AES page refused automated access, Aalto portal record) | Passive-pickup + cable-capacitance circuit mode |
| Holmes, Van Walstijn 2019 | Abstract (DAFx-19) | Dirty Pot: taper separate from scratch |
| Mudd 2023 | Abstract (CHI '23) | Play/Create: local unpredictability |
| Elia, Overholt 2024 | Abstract (Audio Mostly '24, ACM) | Stored, recalled and sequenced gestures |
| Parker, Zavalishin, Le Bivic 2016 | Abstract (DAFx-16) | Antialiased (ADAA) saturation |
| Whitlock: Jensen AN-003, AN-004, *Understanding, Finding & Eliminating Ground Loops* (2005); Whitlock & Fox, AES 129 (2010) | Full text (Jensen PDFs) | Hum colour, ground loop, interface rejection, dimmer buzz; RF sources and symptoms |
| EN 50160:2007 | Full text (sections 4.1, 4.11) | Mains frequency tolerance, harmonic limits |
| ETC, *Risetime and Lamp Noise* | Full web page | Dimmer rise times |
| Claesson, Rossholm, *Notch Filtering of Humming GSM Mobile Telephone Noise* | Full text | 2G phone burst pattern |
| Brown, *RFI in Audio Systems* (Audio Systems Group) | Full text (slide paper PDF) | Square-law detection, cell-phone and RF coupling |
| Hall, Kuehl, *EMI Rejection Ratio of Operational Amplifiers* (TI SBOA128A) | Full text | Quadratic RF-to-offset relation (square law) |
| Keithley, *Low Level Measurements Handbook*, section 2 | Full text (chapter PDF) | Triboelectric and piezoelectric charge in cables |
| FEMTO, *Low-noise cables*; IEEE femtocoulomb triboelectric noise meter | Search-engine summary / abstract only | Triboelectric noise bandwidth and low-noise cable construction |
| Doyle, *From 4AD To Nine Inch Nails* (Sound On Sound, January 2006) | Full web article (automated page fetch) | Studio chain: Drive, heavy compression / Contour, "anything is an instrument" |
| Buskin, *Classic Tracks: Nine Inch Nails 'Closer'* (Sound On Sound, September 2012) | Full web article (automated page fetch) | Studio chain: Drive, Contour, Crush (grit), Rhythm Gate / Machine Pattern |
| West, Caramiaux, Huot, Wanderley 2021 | Abstract (NIME 2021 proceedings page; the full text on nime.pubpub.org refused automated access, HTTP 403) | Machine Pattern: balance of control, comprehensible mappings |
| Dal Santo, Pi, Prawda, Schlecht, Välimäki 2026 | Full text (DAFx-26 PDF) | Local feedback: nonlinearity inside a bounded, energy-controlled loop |

Full references and URLs for the environment, RF and handling sources are in `Docs/research/ElectricalInterference.md`.

## Sources

### Connector intermittency

**Maul, C., McBride, J. W., Swingler, J. (2001).** *Intermittency Phenomena in Electrical Connectors.* IEEE
Transactions on Components and Packaging Technologies 24(3), 370–377. https://eprints.soton.ac.uk/22114/

**Maul, C., McBride, J. W. (2002).** *A Model to Describe Intermittency Phenomena in Electrical Connectors.* Proc.
48th IEEE Holm Conference on Electrical Contacts, 165–174. https://ieeexplore.ieee.org/document/1040838/

- *Observed:* under fretting, contact resistance usually rises **slowly** over time. On top of that slow rise come
  **rapid changes within fractions of a second** (intermittences, or short-duration discontinuities). Tin-plated
  contacts are especially affected. The 2002 paper evaluates high-speed voltage-drop and current measurements with
  general contact theory.
- *Inference:* each contact has a **persistent condition** (wear map, pressure, position in the worn zone) plus a
  small state machine of **short events**: Good → Resistive → Open → Recover (`Source/Core/ContactModel.h`).
  Events need an eligible condition *and* movement. A healthy, still contact never fails on its own.
- *Artistic:* the *Intermittency* control clusters eligible events (burst chatter), and *Recovery* sets how fast a
  contact snaps back. Their ranges are tuned by ear, not measured.

### Cable capacitance and passive pickups

**Paiva, R. C. D., Penttinen, H. (2011).** *Cable Matters: Instrument Cables Affect the Frequency Response of Electric
Guitars.* AES 131st Convention, New York. https://aes2.org/publications/elibrary-page/?id=15992

- *Observed:* in well-buffered equipment the cable is insignificant at audio frequencies. A magnetic pickup, however,
  is a resonant low-pass filter, and **cable capacitance** shifts its resonance: up to 1.4 kHz across the cables
  measured. Inductance and series or parallel resistance were negligible.
- *Inference:* the **Source Circuit** switch. *Buffered/Line* (default) applies no cable response. *Passive Pickup*
  applies a simplified pickup + cable RLC resonance whose peak falls as *Cable Load* (pF) rises. It is normalised to
  unity DC gain so switching modes does not jump in level. The cable response is never applied to line sources.
- *Artistic:* the pickup's inductance, resistance and load are generic values, not a specific pickup.

### Potentiometer laws

**Holmes, B., Van Walstijn, M. (2019).** *Potentiometer Law Modelling and Identification for Application in
Physics-Based Virtual Analogue Circuits.* DAFx-19, Birmingham. https://dafx.de/paper-archive/details/T3CyZW6t3-G6mZQdsqbWAQ

- *Observed:* linear and logarithmic pot characteristics can be identified from measurements. The law depends on the
  pot and on the circuit it sits in (demonstrated on a Big Muff Pi tone stack).
- *Inference:* **wiper position and taper** (Linear, Log A, Reverse Log C) are modelled separately from **scratch**
  (dirt on the track under a moving wiper). Level and tone follow the taper even with zero dirt. Holding the knob
  still stops the scratch, except above 70 % Dirt with Vibration. The *Tone* circuit is a generic 250 k + 22 nF
  treble cut.
- *Artistic:* the worn-arc width and scratch spectrum are tuned by ear.

### Feedback instruments, unpredictability and stored gestures

**Mudd, T. (2023).** *Playing with Feedback: Unpredictability, Immediacy, and Entangled Agency in the No-Input Mixing
Desk.* CHI '23. https://www.research.ed.ac.uk/en/publications/playing-with-feedback-unpredictability-immediacy-and-entangled-ag/

**Elia, G., Overholt, D. (2024).** *Musicking with Dynamical Systems: Introducing a Digitally-Controlled Analog
No-Input Mixer.* Audio Mostly '24. https://vbn.aau.dk/ws/portalfiles/portal/749594001/3678299.3678302.pdf

- *Observed:* in interviews with twenty practitioners, Mudd found that players value working with systems that cannot
  be fully predicted, together with tactile immediacy. Elia and Overholt add digital control to an analog no-input
  mixer, which makes it possible to store, recall, interpolate and sequence control points.
- *Inference:* gestures can be **stored and sequenced** (Gesture Lane, MIDI, presets, Seed + Lock), while the
  **micro-events inside a gesture stay physical and locally unpredictable**. The grid schedules gesture *starts*,
  never individual crackles. *New Take* changes the micro-events and keeps the timing.
- *Artistic:* the local Studio feedback loop is a designed Studio effect. It is not an imitation of a measured
  no-input desk.

### Antialiasing of nonlinearities

**Parker, J. D., Zavalishin, V., Le Bivic, E. (2016).** *Reducing the Aliasing of Nonlinear Waveshaping Using
Continuous-Time Convolution.* DAFx-16, Brno. https://www.dafx.de/paper-archive/details/vem_XXF5qBbfiWOH2RVVAA

- *Observed:* applying the nonlinearity to a continuous-time approximation of the signal, then filtering analytically,
  reduces aliasing without heavy oversampling. The method can also be used inside feedback systems.
- *Inference:* the input-stage drive and Studio Drive use **first-order antiderivative antialiasing (ADAA)** instead
  of oversampling. That keeps the latency at 0 samples, so every parallel bus stays sample-aligned. Contact
  transitions use short band-limited ramps instead of hard switches.
- *Artistic:* **Digital Crush** aliases on purpose. It is labelled as a Studio effect.

### Environment: hum colour, ground loops, dimmers, phones, RF and handling

The environment models come from a separate, deeper research pass, `Docs/research/ElectricalInterference.md`.
That note labels every claim and lists what could be read in full for each source. Main sources:

- Whitlock, *Hum & Buzz in Unbalanced Interconnect Systems* (Jensen AN-004) and *Interconnection of Balanced and
  Unbalanced Equipment* (AN-003).
- Whitlock, *Understanding, Finding, & Eliminating Ground Loops*.
- Whitlock and Fox, *Ground Loops: The Rest of the Story* (AES 129).
- EN 50160 (mains voltage harmonics).
- ETC, *Risetime and Lamp Noise*.
- Claesson and Rossholm, *Notch Filtering of Humming GSM Mobile Telephone Noise*.

What each model uses:

- **Hum colour.** *Observed:* the mains voltage is a sine with small odd harmonics. Field (capacitive) pickup and
  leakage currents respond to the rate of change, which weights harmonic *n* by about *n*. Even harmonics point to a
  rectifier. *Inference:* the shared mains generator makes a line-shaped "round" waveform and a
  harmonic-weighted "bright" one, neither with a 2nd harmonic. Body blends between them. The Supply buzz keeps its
  2× mains pulses.
- **Ground loop.** *Observed:* shield current × shield (and contact) resistance sits in series with the signal.
  Typical figures: 2-prong leakage of 5 µA, 100 µA or 1 mA; shield resistance of milliohms per metre; 50 mΩ
  contacts; ground differences from millivolts up to about 1 V (CATV). Balanced inputs reject roughly 30 dB, and
  transformers much more, with rejection falling as frequency rises. *Inference:* V = I·R_loop, scaled to a 300 mV
  nominal = −18 dBFS. A worn or partly open sleeve or shield raises R_loop, and a fully open one breaks the loop. The
  interface filter's rejection falls 6 dB/octave above a knee. *Artistic:* the Natural/Expressive caps, and the
  nominal-level mapping, which is a convention.
- **Dimmer buzz.** *Observed:* a phase-control dimmer switches the load current at the firing angle, with a rise of
  about 5 µs for a cheap dimmer and 350–800 µs for a choked one. The induced noise follows dI/dt and is loudest near
  half brightness. *Inference:* firing-angle current with a polyBLEP step, a rise-time filter and differentiation.
  That gives odd harmonics locked to the mains.
- **2G phone.** *Observed:* GSM TDMA sends 577 µs bursts every 4.615 ms (216.7 Hz), with an idle frame every 26
  frames, and audio circuits detect the envelope. *Inference:* a band-limited burst train through a receiver
  band-pass, and a steep distance law (detected power falls as the square of the field). *Artistic:* the call-state
  sequencing (check-in, incoming, in call) and the ±3 dB power drift.

- **RF interference: square-law detection.** *Observed:* audio circuits turn RF into audio because diodes,
  transistors and IC inputs are square-law detectors; "a 10 dB reduction in RF level reduces audible interference by
  20 dB" (Brown, *RFI in Audio Systems*). An op-amp's input-referred offset shift grows with the square of the RF
  amplitude (TI SBOA128A). Symptoms range from audible radio or voices to buzz (Whitlock, 2005).
  *Inference:* the detected audio is k·e(t)², where e(t) is the envelope of the RF at the gear, followed by the
  detecting stage's DC coupling (20 Hz) and its band limit (two poles). *Interference* is the RF field strength a
  (RF level 20·log10 a dB); the environment's waveform scales with a and the source's coupling scales with a again,
  so the injected level follows a². Ten dB less RF is exactly 20 dB less noise. At radio frequencies the cable
  capacitance, not the audio source, sets the node impedance, so RF is not scaled by *Source Circuit*.
  *Artistic:* the three band limits (AM 4.5 kHz, two-way 3 kHz, motor 7 kHz), the output normalisation and the
  20 ms smoothing of the amount.
- **RF transmitter types.** *Observed:* broadcast (AM, shortwave), two-way and CB radios, and sparks from brush
  motors, relays and switches are common RF sources (Whitlock, 2005, section 5.1). GSM phones are 100 % AM,
  217 Hz pulses (Brown).
  *Inference:* the transmitters differ only in e(t).
  **AM Station**: a continuous carrier with slow fading, amplitude-modulated by a programme.
  **Two-Way Radio** (default): keyed transmissions of 30–250 ms with gaps of 0.12–1.3 s, modulated by band-limited
  chatter while keyed. Key up and key down become thumps after the DC coupling, because the detected carrier power is
  a step. The traffic does not depend on the field strength.
  **Motor Sparks**: Poisson spark impulses whose rate wanders between 60 and 250 per second. The detected energy of
  each spark (A²·τ/2, τ = 50 µs) is added directly, which keeps the crackle independent of the sample rate.
  *Artistic:* the AM programme is **synthetic and never intelligible**: noise-excited ("whispered") two-formant
  syllables at 2.5–7 Hz in phrases of 3–9 with 0.15–0.6 s pauses. There are no pitch pulses, so there is no tonal
  line. The formant table is a generic set of adult vowel formants, not taken from a cited measurement. Also artistic:
  the two-way timing (chosen to keep the character of the earlier "sparse chatter"), the spark rate range and
  amplitude distribution, and the 10 ms fade when the transmitter type changes.
- **Triboelectric handling (Cable Handling, Mic path).** *Observed:* friction between a cable's conductor, dielectric
  and shield separates charge (triboelectric effect); deformation also releases stored charge (Keithley, section 2).
  Low-noise cables add a semi-conductive layer; the noise extends from quasi-DC up to about 100 kHz (FEMTO, search
  summary only).
  *Inference:* the separated charge is a current into the conductor node. Below the node's RC corner
  (node impedance × cable capacitance) the voltage is proportional to the node impedance. With a passive source
  (~50 kΩ across the band) the friction crackle is heard, and a larger *Cable Load* lowers the corner, so the crackle
  gets darker. A buffered or line source (~1 kΩ) holds it down by about 34 dB and pushes the corner far above the
  audio band, so *Cable Load* no longer matters. The realism spec (section 5) requires a sealed cable with *Mic* at
  zero to make no handling sound, so this term exists only with *Mic*, inside the same bounded handling path
  (capped by `mechanicalPeak`).
  *Artistic:* the charge-to-voltage scale (0.33, set by ear) and the 50 kΩ passive node, an estimate from typical
  pickup impedances (Lemme) and consumer output impedances (Jensen AN-003).

None of these numbers were measured on real gear for this release. They are starting points, like every other
default.

### Studio chain and Machine Pattern: provenance (routing and industrial-performance specs)

These sources motivate the **Studio** effects and the Machine Pattern. None of them describes, or is claimed to
describe, this plug-in, an artist's signature sound, or a measured machine. The Studio stages are labelled as
processing, never as hardware physics.

**Doyle, T. (2006).** *From 4AD To Nine Inch Nails* (interview with John Fryer). Sound On Sound, January 2006.
https://www.soundonsound.com/people/4ad-nine-inch-nails

**Buskin, R. (2012).** *Classic Tracks: Nine Inch Nails 'Closer'*. Sound On Sound, September 2012.
https://www.soundonsound.com/techniques/classic-tracks-nine-inch-nails-closer

- *Observed:* Fryer: "if it's power noise stuff, I distort the hell out of everything"; heavy compression ("you can
  really over-compress stuff"); "if it made a noise, it was an instrument". For 'Closer', engineer Sean Beavan
  describes a snare sample run through an SSL console "for some distortion" and a Zoom 9030 "for a little bit of
  ring modulation", fast-attack compression as the snare sound, a kick sampled from an 808 part where he "removed the
  noise but retained the grit", and a drum pattern built by running parts out of samplers through outboard gear.
- *Inference:* a fixed, labelled Studio chain that processes only the **fault artifacts or breaks** (A, D, or both),
  never the dry input: **Drive** (Soft / Hard / Asymmetric, antialiased) for deliberate distortion, **Contour** as a
  fast transient/sustain shaper in place of fast-attack compression, **Crush** (bits and hold rate, deliberately
  aliasing) for sampler grit, and the **Rhythm Gate** plus Machine Pattern for rhythmic, sequenced treatment of the
  faults. Ring modulation is **not** a Studio stage: the tuned **Resonator** gives a metallic, pitched colour
  instead, and the only ring modulator is the hidden Ghost Carrier, off by default and outside the chain.
- *Artistic:* stage order, ranges and every default. The four Machine Pattern factory presets use generic names and do
  not imitate a named artist.

**West, T., Caramiaux, B., Huot, S., Wanderley, M. M. (2021).** *Making Mappings: Design Criteria for Live
Performance.* NIME 2021, Shanghai. https://nime.org/proc/nime2021_31/index.html (DOI 10.21428/92fbeb44.04f0fc35)

- *Observed (abstract):* in a thematic analysis of interviews with two groups of participants, "mappings must manage
  the balance of control between the instrument and the player, and they should be easy to understand for the player
  and audience".
- *Inference:* Machine Pattern shares control. The player sequences **gesture starts**, probability, accent and
  targets, while the hardware model keeps the micro-events. Every step is a named, source-aware gesture (for example
  Wiggle, Seat, Pull), and the Studio stages are shown as processing, so the mapping stays readable for performer and
  audience. This complements Mudd (2023) and Elia and Overholt (2024) above.
- *Artistic:* the lane layout, step fields and Fill / Evolve behaviour.

**Dal Santo, G., Pi, X., Prawda, K., Schlecht, S., Välimäki, V. (2026).** *Shimmer Reverberation with Nonlinear
Feedback Delay Networks.* DAFx-26, Cambridge, MA, 1–4 September 2026.
https://www.dafx.de/paper-archive/details/tLDBzbMbZN8aoERSiwvSpg

- *Observed:* nonlinear and time-varying operations can be placed inside a feedback loop "while adhering to
  energy-preservation and stability criteria". With an orthogonal feedback matrix and attenuation below 1 the loop is
  stable, but the nonlinearity moves energy across the spectrum, so the decay is no longer set exactly by the
  attenuation. Soft saturation is cited as a way to ensure stability (after Norilo).
- *Inference:* the local Studio feedback loop taps after the Resonator and returns into the Drive input through a
  high-pass, low-pass, DC blocker and a soft (tanh) saturator. Its loop gain is clamped below 0.97, **including** the
  gains of the stages inside the loop. It is zero by default, killable (*Kill Feedback*), and its tail estimate is
  capped at 30 s. The tail is reported as an estimate, because nonlinearity makes the decay only approximately
  predictable.
- *Artistic:* the loop's filters, delay range and the 0.97 margin.

The routing spec also cites **Parker, Zavalishin and Le Bivic (2016)** (above) for the antialiased Drive, and the
deliberately aliasing Crush is labelled as a Studio effect.

## Calibration status (release gate)

No dry/wet recordings of real faulty hardware were captured or bundled for this release. The models are therefore
shipped as **research-informed**, and empirical calibration remains an open release gate for any marketing claim of
measured realism. See `QA.md`. A calibration pass should record, for each case, the device, interface impedance, gain,
sampling chain and uncertainty:

- a healthy TS jack and a loose one;
- a finger on an exposed plug;
- a cable under a controlled bend;
- a dirty level pot;
- a passive pickup and a buffered source.

Then compare level-matched A/B listening, spectrograms, event-rate and dwell distributions, and transient envelopes,
and tune **ranges**, not single takes.
