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

This was an unattended build without institutional library access. For every paper only the **abstract and
bibliographic record** were read (publisher or repository page, or a search-engine abstract where the page refused
automated access). No full text was used. The table says what was available for each paper.

| Paper | Available | Used for |
|---|---|---|
| Maul, McBride, Swingler 2001 | Abstract (IEEE T-CPT 24(3):370–377; Southampton ePrints page refused automated access) | Contact model: slow drift + rapid intermittences |
| Maul, McBride 2002 | Abstract / bibliographic record (48th IEEE Holm Conf., pp. 165–174) | Contact model: short-duration discontinuities |
| Paiva, Penttinen 2011 | Abstract (AES 131st Convention; AES page refused automated access, Aalto portal record) | Passive-pickup + cable-capacitance circuit mode |
| Holmes, Van Walstijn 2019 | Abstract (DAFx-19) | Dirty Pot: taper separate from scratch |
| Mudd 2023 | Abstract (CHI '23) | Play/Create: local unpredictability |
| Elia, Overholt 2024 | Abstract (Audio Mostly '24, ACM) | Stored, recalled and sequenced gestures |
| Parker, Zavalishin, Le Bivic 2016 | Abstract (DAFx-16) | Antialiased (ADAA) saturation |

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

### Environment: hum colour, ground loops, dimmers and phones

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

None of these numbers were measured on real gear for this release. They are starting points, like every other
default.

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
