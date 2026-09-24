# Electrical interference, ground loops and resistance: research for Interference

Status: research note, September 2026. No source code was changed for this note.
Scope: the physics of hum, buzz, RF chatter, ground loops, contact resistance and cable loading; how artists
and plug-ins have used them; and concrete DSP recommendations for the Interference plug-in.

## How to read this note

Every claim carries one of these labels:

- **[O:key]** Observed. The source identified by `key` (see References, which lists every URL) reports it.
  `[O:key, summary]` means the claim came from a search-engine summary of that page, not from reading the page.
- **[I]** Inference. An engineering conclusion drawn from observed sources and circuit theory. Where an
  inference contains a number, the arithmetic is shown or the number is marked *estimate*.
- **[A]** Artistic. A deliberate design choice for musical use that goes beyond what the sources support.

**What was available.** Full text was read for the Jensen application notes and seminar (Whitlock), the
Whitlock & Fox AES paper (generic version), Jim Brown's RFI slide paper, two TI application reports, the
Claesson & Rossholm GSM paper, the EN 50160:2007 text, the Ney Contact Manual (chapter 1), Keithley's
handbook chapter 2, the Lau et al. fretting report, Cascone's CMJ article, and several web pages listed in
References. The following were **not accessible** and were not used beyond their bibliographic records or
abstracts: Ott, *Electromagnetic Compatibility Engineering*; Morrison, *Grounding and Shielding*; Ballou,
*Handbook for Sound Engineers* (its grounding chapter is by Whitlock, whose seminar material was used instead);
Muncy, "Noise Susceptibility in Analog and Digital Signal Processing Systems", JAES 43(6), 1995 (bibliographic
record only); the ScienceDirect and ResearchGate fretting papers (HTTP 403). No primary source was found for any
claim that Aphex Twin used mains hum deliberately, and none for a Goodhertz product with a hum generator. Both
are therefore left out.

---

## Summary of top findings

1. **Hum versus buzz comes from the coupling path, not only from the mains.** The mains voltage itself is
   mostly a sine with small odd harmonics. EN 50160 caps the 3rd, 5th and 7th at 5 %, 6 % and 5 % and THD at
   8 % [O:EN50160]. Whitlock measures "commonly 5 % THD" from flat-topping [O:AN004]. Capacitive (electric-field)
   and inductive (magnetic) coupling both respond to the *rate of change*, so harmonic *n* is weighted by
   roughly *n*. That turns a mild waveform into a buzz [I, from O:WF2010 and O:GS2005]. Whitlock's
   leakage-current spectrum is "almost entirely odd-order", and those odd harmonics "give 'buzz' its sonic
   character" [O:GS2005].
2. **Ground loops are Ohm's law.** Shield current times shield resistance adds in series with the signal
   [O:AN004]. Typical inputs: leakage currents of 5 µA, 100 µA or 1 mA for low, medium and high-power 2-prong
   gear; shield resistance of 16 to 41 mΩ/ft; 50 mΩ per connector contact. With a 300 mV reference that gives
   hum from about −109 to −37 dB [O:AN004]. A CATV ground can reach −6 dB [O:AN004].
3. **Even harmonics mean a rectifier.** Rectified supply currents repeat at 2× mains (100/120 Hz) [O:WMH, O:ELLIOTT].
   Leakage and field coupling from the mains voltage give odd harmonics of 50/60 Hz [O:GS2005]. Today's
   `Environment` hum waveforms contain a 2nd harmonic in both the clean and bright recipes. That fits a
   rectifier path, not field pickup [I].
4. **Phase-control dimmers are the worst buzz source.** A cheap dimmer's current rises in about 5 µs (10 to
   90 %), and induced ground voltage is proportional to dI/dt, which peaks near 50 % brightness [O:WF2010].
   Theatre dimmers slow the rise to 350 or 500 µs to protect audio and video [O:ETC]. Firing angle and rise
   time are two physical, playable parameters.
5. **RF enters audio through square-law detection.** A 10 dB drop in RF gives a 20 dB drop in the audible
   interference [O:BROWN]. Op-amp offset shift rises with the square of RF amplitude [O:TI-EMIRR]. Model
   detected RF as envelope², not as filtered noise times a random gain.
6. **GSM buzz has an exact, well-documented pattern.** It is 100 % AM with square bursts at 5200/24 ≈
   216.7 Hz (577 µs slot in a 4.615 ms frame), 2 W peak and 100 mW average [O:BROWN, O:CLAESSON]. An idle frame
   comes every 26 frames (about every 120 ms), half-rate runs at about 108 Hz, and harmonics decay as 1/f²
   [O:CLAESSON]. It is ideal as a small, iconic, tempo-free environment source.
7. **How loud hum gets depends on the receiving impedance.** A body coupled to the mains has a source impedance
   of the order of 1 MΩ (and much higher at 50 Hz by capacitance estimate), and measures about 100 mVpp
   [O:OSI]. So a finger on a 1 MΩ guitar input is roughly 40 dB louder than on a 10 kΩ line input
   [I, arithmetic in §1.2]. Today's Touch and Loose Jack hum ignore the Source Circuit setting.
8. **Balanced does not mean immune.** An active balanced input fed from an unbalanced source can reject as
   little as 15 to 30 dB [O:AN004, O:AN003]. Output-type isolators reject hum by about 70 dB but buzz at 3 kHz
   by only about 35 dB [O:GS2005]. Remedies change the *spectrum* of the residual noise. That makes a strong
   "Interface" control.
9. **Contacts at audio level never arc and never self-clean.** Voltages under 30 mV do not permanently damage
   surface films [O:NEY]. Micro-arcs last 5 to 100 ns and need currents near a material limit, generally
   below 100 mA [O:NEY]. Sliding contact noise is ohmic, proportional to current [O:NEY]. So pot scratch should
   scale with signal plus DC current, and crackle should come from resistance steps, not synthesized arcs [I].
10. **Artists and sound designers have used these faults directly.** Examples include the lightsaber (projector
   hum plus TV buzz picked up by a microphone passing a picture tube) [O:BURTT]; Tom Morello tapping a live cable
   tip [O:MORELLO]; Kubisch's induction-headphone walks [O:KUBISCH]; no-input mixing [O:NAKAMURA]; and the
   "aesthetics of failure" [O:CASCONE]. Commercial plug-ins mostly offer hum as a static noise layer (iZotope
   Vinyl, RC-20, UAD ATR-102, Soundtoys Radiator) [O:VINYL, O:RC20, O:ATR102, O:RADIATOR]. No reviewed product
   models ground loops or coupling paths causally [I, from the products reviewed].

---

## 1. Electrical interference in audio

### 1.1 The four coupling paths

- **Common-impedance coupling.** Two circuits share a conductor, usually a cable shield, so one circuit's
  current produces a voltage in the other [O:AN004]. Whitlock calls it the dominant mechanism in unbalanced
  systems ("vast majority of problems") [O:GS2005].
- **Electric-field (capacitive) coupling.** Strong AC electric fields surround conductors at high voltage. A
  grounded shield diverts the coupled current [O:GS2005]. It matters mainly when the source impedance is very
  high [O:GS2005].
- **Magnetic coupling.** Any conductor loop in a time-varying field has a voltage induced in it [O:GS2005].
  Pickup is proportional to loop area [O:AN004]. Useful magnetic shielding at power frequencies takes steel.
  Ordinary cable shields do not provide it [O:GS2005].
- **Radiated / RF coupling.** Cables and internal wiring act as antennas, and semiconductor junctions detect the
  RF [O:BROWN].

[I] For the plug-in, these paths suggest four named injection routes (resistive, capacitive, magnetic, RF). Each
has its own spectral weighting, and each fault source chooses which routes it opens.

### 1.2 Capacitive coupling to high-impedance inputs; the body as antenna

- Human body capacitance to its surroundings is "tens to low hundreds of picofarads". The ESD human-body model
  is 100 pF in series with 1.5 kΩ [O:WBODY]. Another source gives 100 to 200 pF to far ground
  [O:EXTREME, summary].
- An informal lab study measured mains hum on a person at about 100 mVpp with a source impedance of order 1 MΩ.
  On an ungrounded metal frame it measured about 2 Vpp at about 10 MΩ. A loop antenna found ≤1 mV away from a
  power supply and 4 mV right next to it. The author concluded that magnetic coupling was negligible in that
  room [O:OSI]. That page argues for antenna-mode reception. The argument is one lab's interpretation.
  The measured voltages and impedances are the useful part.
- Touching a scope probe shows 50/60 Hz because the body forms a capacitive divider between mains wiring and
  ground, loaded by the probe's 1 MΩ [O:SCOPEFORUM, summary].
- Touching grounded strings lowers guitar hum because the player is no longer a floating antenna
  [O:FRALIN, O:HAZE, summary].

[I] A simple Thevenin model follows. Assume *C_w* ≈ 1 to 2 pF (*estimate*) from the body to live wiring,
*C_g* ≈ 150 pF from body to ground [O:EXTREME], and 230 V at 50 Hz:

- Open-circuit body voltage: V_th = 230 · C_w/(C_w+C_g) ≈ 3 V rms (*estimate*).
- Source impedance: Z_th = 1/(ω(C_w+C_g)) ≈ 21 MΩ at 50 Hz.
- Voltage across an input resistance R: V_th · jωR(C_w+C_g) / (1 + jωR(C_w+C_g)). This is a first-order
  **differentiator** (a high-pass) with corner f_c = 1/(2πR(C_w+C_g)).

| Input R | Hum at the input (50 Hz) | Differentiator corner f_c |
|---|---|---|
| 1 MΩ (guitar amp) | ≈ 0.14 V (−17 dBV) | ≈ 1 kHz |
| 100 kΩ | ≈ 14 mV (−37 dBV) | ≈ 10 kHz |
| 10 kΩ (line) | ≈ 1.4 mV (−57 dBV) | ≈ 100 kHz |
| 1.5 kΩ (mic pre) | ≈ 0.2 mV (−73 dBV) | ≈ 700 kHz |

All rows are *estimates* from the formula. The 1 MΩ row is consistent with the ~100 mVpp observed [O:OSI]. Two
consequences:

- **Level**: each decade of input impedance changes touch hum by about 20 dB.
- **Colour**: below f_c each harmonic *n* is boosted by *n* (+6 dB/octave). At 1 MΩ the buzz stops brightening
  around 1 kHz. At line impedance it stays differentiated across the whole audio band, but the level is much
  lower.

### 1.3 Magnetic coupling

- A voltage is induced in any conductor exposed to a changing magnetic field, proportional to the field's rate
  of change [O:WF2010]. In premises wiring the ground wire sits unevenly between line and neutral. It forms a
  "conduit transformer" whose induced voltage is proportional to run length and to dI/dt of the load current
  [O:WF2010]. Measured on a 6.1 m reference sample at 6 A: 15 mV at 1 kHz, 34 mV at 2 kHz, 81 mV at 5 kHz and
  163 mV at 10 kHz. That is proportional to frequency [O:WF2010].
- The worst-case wiring geometry is "over 1,000 times worse" than twisted L-N wiring [O:WF2010].
- Single-coil pickups behave as directional antennas for mains hum. Humbuckers cancel it as a common-mode
  signal [O:WPICKUP, O:LEMME].
- Star-quad microphone cable improves magnetic immunity by about 40 dB over standard shielded twisted pair
  [O:GS2005].
- Transformer leakage flux can cause buzz, and toroids "can (and do) still cause problems" [O:ELLIOTT].
  Magnetic materials add harmonics above 100/120 Hz through non-linearity [O:WMH].

[I] Magnetic pickup is driven by the **current** spectrum of nearby loads, weighted by *n*. Load currents are far
more distorted than the voltage (rectifiers, dimmers). So magnetic hum is usually buzzier than electric-field hum
from the same room. Orientation matters for single coils, which makes it a natural gesture.

### 1.4 Harmonic content: hum, supply buzz and dimmer buzz

**Mains voltage.**

- 50 Hz ±1 % during 99.5 % of a year, and +4/−6 % at all times. Individual harmonic limits: 3rd 5 %, 5th 6 %,
  7th 5 %, 9th 1.5 %, 11th 3.5 %, 13th 3 %, even harmonics 2 % (2nd), 1 % (4th), 0.5 % (6th to 24th). THD up to
  the 40th is ≤ 8 % [O:EN50160].
- Whitlock: "hum" is predominantly the fundamental; "buzz" is a mixture of high-order harmonics from line
  distortion that "commonly reaches 5 % THD". Buzz is often more audible than hum because hearing is most
  sensitive at 2 to 5 kHz [O:AN004].
- Harmonics are "almost entirely odd-order due to 'flat-top' distortion". Significant energy exists above
  100 kHz, including AM radio [O:GS2005].

**Rectifier (supply) buzz.**

- The bridge rectifier makes peaks at 100/120 Hz [O:ELLIOTT, summary]. Diodes conduct in short pulses: about
  1.5 ms per 10 ms half-cycle in one example [O:ELLIOTT, summary], and 3.5 ms (35 %) at 1.61 A with a 5.36 A
  peak in another [O:ELLIOTT].
- Poor grounding lets diode switching noise into the ground wiring, where it causes "hum, or more usually a
  'buzz'" [O:ELLIOTT].
- In a centre-tapped ±supply with a grounded centre tap, the ripple on each rail is at the *mains* frequency,
  not twice it [O:ELLIOTT].
- Hum from heavy ground currents is "usually at the second harmonic" (100/120 Hz) because those currents come
  from rectifying supplies [O:WMH].

**Dimmer buzz.**

- Theatre dimmers specify rise time (10 % to 90 % of current). 350 µs is adequate for theatre and 500 µs is
  required for TV studios to protect audio and video [O:ETC]. 800 µs is used for demanding low-noise sites
  [O:CONTROLBOOTH, summary].
- A cheap 600 W wall dimmer measured about 5 µs rise time at 50 % brightness [O:WF2010]. When the SCR fires,
  current goes from zero to full "in a couple of microseconds" [O:CONTROLBOOTH, summary].
- Half-load rise time is about 80 % of the full-load figure [O:ETC].

[I] Waveform consequences for synthesis:

- A **full-wave rectifier's DC-side current pulses** all have the same sign, twice per cycle. The spectrum is
  harmonics of 2f (100, 200, 300 Hz ... at 50 Hz), which reads as a "120 Hz buzz". That is the shape
  `Environment` produces today (`|sin|^10`).
- A **phase-control dimmer** makes one current step per half-cycle, *alternating in sign*. The induced voltage
  (∝ dI/dt) is a train of pulses at 2f with alternating polarity. Its spectrum is the **odd harmonics of f**,
  flat up to about 1/(π·t_rise). That is roughly 60 kHz for 5 µs and 640 Hz for 500 µs. So a cheap dimmer gives
  a bright, spiky odd-harmonic rasp at any level, and a choke-equipped dimmer gives a duller buzz. Step height ∝
  |sin α| at firing angle α, so the buzz is loudest near α = 90° (half brightness), as Whitlock & Fox observed.
- **Magnetostriction** (acoustic transformer or lamp hum) is at 2f [O:WMH]. That is a mechanical sound. It
  belongs in a microphonic path, not in an electrical injection.

### 1.5 RF interference and AM demodulation

- Wiring and cables are antennas. Diodes, transistors and ICs perform **square-law detection** [O:BROWN].
- Because detection is square law, a 10 dB reduction in RF lowers the audible interference by 20 dB. Halving
  cable length gives 6 dB less RF voltage and 12 dB less detected output [O:BROWN].
- An op-amp's usual response to EMI is a DC offset shift from rectification in its p-n junctions. The
  relationship between RF amplitude and offset is quadratic [O:TI-EMIRR].
- Symptoms range from audible radio, CB or TV (TV heard as buzz) to a subtler "veiled" or "grainy" sound
  [O:GS2005].
- Sources include AM, shortwave, FM and TV broadcasts; two-way radios; cell phones; sparks from brush motors,
  relays and switches; corona on power-line insulators; dimmers; fluorescent lights; and switching supplies
  [O:GS2005].
- Variable-speed motor drives chop at 10 to 20 kHz, with harmonics to hundreds of kHz [O:BROWN].
- SSB voice occupies about 300 to 3400 Hz. If demodulated with a BFO off by more than about 30 Hz, it sounds
  "Donald Duck"-like [O:SSB].

[I] What this means for the audio:

- An **AM broadcast** detected by a junction gives faint programme audio (voice or music), band-limited and
  distorted by square-law detection. Its level ∝ (field strength)².
- A **keyed transmitter** (two-way, CB) gives bursts with a keying edge. The square law turns key-up and
  key-down into audible thumps.
- **Spark sources** (brush motors) give impulsive broadband RF. After detection this becomes crackle whose
  density follows the motor's commutation rate.

### 1.6 Cell-phone GSM buzz

- GSM switches its radio on and off at 5200/(3·8) Hz ≈ 216.7 Hz. Full rate has an idle frame every 26 TDMA
  frames. Half rate gives about 108 Hz. The Fourier components decay as 1/f², leaving about 15 harmonics below
  3.4 kHz. Interference reaches equipment within 1 to 2 m of the handset. Amplitude depends on transmit power,
  and phase depends on slot timing, and both change during a call [O:CLAESSON].
- The signal is 100 % AM with short square pulses: 2 W peak, 100 mW average. The detected spectrum is midrange
  audio [O:BROWN].
- The slot is 577 µs within a 4.615 ms frame. Transmit current pulses can exceed 1 A. The buzz is intermittent
  and rhythmic ("dit-dit-dit") and flares when the handset talks to the tower. It is rare on 4G/5G networks
  [O:SCIENCEABC, summary].
- TI calls it "TDMA noise" and says the 217 Hz waveform can couple into the speaker, earpiece or microphone path
  [O:TI-TDMA].

[I] Arithmetic: 26 × 4.615 ms ≈ 120 ms, so full-rate buzz has a periodic drop-out at about 8.3 Hz. That gives
the characteristic stutter within the tone.

### 1.7 Switching supplies, motor drives and computers

- A switching supply may run at 250 kHz. In light-load **burst mode**, the effective repetition rate can fall
  into the audio band [O:PATENT-SMPS, summary]. Ferrite transformer mechanical resonances are commonly 8 to
  15 kHz [O:PATENT-SMPS, summary]. Designers move burst frequencies away from 2 to 4 kHz, where hearing is most
  sensitive [O:PATENT-SMPS, summary].
- Users widely report noise from USB audio interfaces that changes with CPU or GPU load or mouse movement. It is
  loudest under GPU load and disappears on battery power [O:GEARSPACE, O:HIFIME, summary]. These are
  practitioner reports, not measurements.

[I] Computer and SMPS noise is **load-modulated whine**: pitch and level track a load variable. It fits the
plug-in's sidechain rule (the sidechain may *control*, never be heard). A host signal's envelope can stand in for
"computer load", modulating an internally synthesized whine.

### 1.8 Triboelectric and microphonic cable noise

- The triboelectric effect makes charge imbalance from friction. It is distinct from piezoelectric (stress)
  charge [O:KEITHLEY]. PVC and nylon rate "weak" for triboelectric effects in Keithley's insulator table
  [O:KEITHLEY].
- "Low-noise" cable uses a semi-conductive layer between dielectric and shield. Mechanically induced charge
  appears "typically in the low frequency range from quasi-DC up to 100 kHz" [O:FEMTO, summary]. Impact
  "slapping" noise comes from the capacitance change as the dielectric deforms [O:FEMTO-SEARCH, summary].
- Super-low-noise coax is reported about 60 dB quieter than standard coax [O:IEEE-TRIBO, abstract via summary].

[I] The noise is a *charge* source, so the audio voltage is Q/C shunted by the source impedance. A buffered
(low-impedance) source shorts it out. A passive pickup or piezo (high impedance) lets it through. Its spectrum
is low-frequency-weighted thumps and rustle, not resonant tones. A modal "cable resonance" bank (the current
`CableHandling` model) is better explained as a mechanical pickup: a contact mic or a microphonic tube or
transducer.

### 1.9 Shield current, pin 1 and SCIN

- The "pin 1 problem" (Muncy's term) is common-impedance coupling built into equipment. Shield current flows in
  internal wiring shared with audio circuitry, so the shield becomes "a very low-impedance signal input"
  [O:GS2005]. Rane attributes it to tying pin 1 to signal ground instead of chassis [O:RANE151].
- It is tested with the "hummer", which forces about 50 mA AC through shield connections [O:GS2005].
- Shield-current-induced noise (SCIN, Muncy 1994): shield current's field induces unequal voltages in an
  imperfect twisted pair. Braided shields are best and foil-plus-drain shields worst [O:GS2005].
- RF is also coupled mainly by pin-1 problems and SCIN [O:BROWN].

---

## 2. Ground loops

### 2.1 Mechanism

- In an unbalanced interface, the shield carries both the signal return and power-line-derived current.
  Voltage between A and B (the shield drop) is in series with the signal, so the input amplifies both
  [O:AN004].
- The inter-chassis current is charge and discharge of capacitance between line and chassis: power-transformer
  interwinding capacitance and EMI or safety capacitors [O:AN004, O:GS2005].
- UL limits leakage to 0.75 mA for 2-prong devices and 5 mA for grounded ones [O:GS2005].
- An ungrounded chassis can float "over 50 V", with current generally under 1 mA [O:AN003]. A floating chassis
  voltage between 0 and 120 V is possible, and 60 V is "reasonable typical" [O:AN004].
- Between safety grounds: a few millivolts on the same branch, up to several volts across a building, and
  "1 volt is not unusual" [O:GS2005]. These are mostly magnetically induced in the wiring [O:WF2010].
- Soil resistance between ground rods is often tens of ohms. Earth grounds are not at the same potential
  [O:GS2005].

### 2.2 Typical magnitudes

Whitlock's calculated hum for unbalanced interconnects, in dB re 300 mV. Leakage current is L = 5 µA, M = 100 µA,
H = 1 mA; each connection adds 50 mΩ [O:AN004]:

| Cable | #26 shield (41 mΩ/ft) L / M / H | #24 (25 mΩ/ft) L / M / H | #22 (16 mΩ/ft) L / M / H |
|---|---|---|---|
| 1 m (3 ft) | −109 / −83 / −63 | −111 / −85 / −65 | −112 / −86 / −66 |
| 3 m (10 ft) | −101 / −75 / −55 | −105 / −79 / −59 | −107 / −81 / −61 |
| 6 m (20 ft) | −96 / −70 / −50 | −100 / −74 / −54 | −103 / −77 / −57 |
| 15 m (50 ft) | −89 / −63 / −43 | −93 / −67 / −47 | −96 / −70 / −50 |
| 30 m (100 ft) | −83 / −57 / −37 | −87 / −61 / −41 | −91 / −65 / −45 |

More data points:

- Leakage by device class [O:AN004]:
  - 5 µA: low-power gear under 20 W (CD players, decks, turntables).
  - 100 µA: 20 to 100 W gear.
  - 1 mA: over 100 W gear (big amplifiers, powered subwoofers, projection TVs).
- A worked example gives −53, −55 and −69 dB at three inputs [O:AN004].
- A CATV shield connection drove just under 200 mA through audio cables, giving "truly horrible" hum of about
  −6 dB [O:AN004].
- A 25 ft cable with a 1.0 Ω shield carrying 316 µA gives −60 dB re −10 dBV. Belden 8241F (0.065 Ω, 2.6 mΩ/ft)
  is about 24 dB better [O:GS2005].
- Shield resistances [O:BROWN]:

  | Cable | Shield resistance |
  |---|---|
  | Heavy braid (8241F) | 2.6 Ω/1000 ft |
  | Double braid (8281) | 1.1 Ω/1000 ft |
  | Foil/drain #22 | 16 Ω/1000 ft |

- Audibility: about −80 dB is annoying to most listeners, and inaudibility for critical residential listening
  may need about −120 dB [O:AN004].

[I] Check of the formula: 3 m of #26 = 0.41 Ω plus 0.1 Ω of contacts = 0.51 Ω. At 1 mA that gives 0.51 mV,
and 20·log(0.51/316) = −55.8 dB, matching the table's −55. The plug-in can therefore compute hum as
**20·log10(I_shield·R_loop / V_ref)** from physical inputs.

### 2.3 Spectrum

- Leakage current through a parasitic capacitance (Whitlock plots 3 nF on a typical outlet) is mostly odd
  harmonics of 60 Hz, plus energy above 100 kHz [O:GS2005].
- Ground voltage from the conduit transformer rises with frequency (∝ dI/dt). A dimmer at 50 % with a 5 µs
  rise is the worst common case [O:WF2010].
- SOS: hum comes from the 50/60 Hz supply, and buzz from "the related harmonics that accrue on mains
  supplies" [O:SOS, summary].

[I] Ground-loop noise through leakage capacitance is a **differentiated** mains waveform: odd, *n*-weighted, and
buzzier than the line voltage. Ground-voltage-difference loops carry the building's load-current signature
(dimmers, rectifiers). A pure fundamental-only "ground hum" is unrealistic except where it is IR drop from a
large, clean 50/60 Hz current.

### 2.4 Balanced vs unbalanced, and CMRR

| Configuration | Rejection | Source |
|---|---|---|
| Unbalanced, no isolator | 0 dB by definition | [O:GS2005] |
| Active balanced input fed from unbalanced source (470 Ω imbalance) | ≈ 30 dB at 60 Hz | [O:AN003] |
| Active "ground isolator" (SSM2141-based) with 200 Ω to 1 kΩ source | 15 to 30 dB | [O:AN004] |
| SSM2141 with only 1 Ω source imbalance | degrades by about 25 dB | [O:GS2005] |
| Output-type (bifilar) transformer, active input | ≈ 55 dB at 60 Hz, falling to ≈ 30 dB above 1 kHz | [O:AN003] |
| Output-type isolator (e.g. HE-2 class) | ≈ 70 dB at 60 Hz, ≈ 35 dB at 3 kHz | [O:GS2005] |
| Balanced-to-unbalanced output transformer + pad | ≈ 60 dB at 60 Hz, falling 6 dB/octave ("effective for hum but not for buzz") | [O:AN003] |
| Input-type (Faraday-shielded) transformer | ≈ 100 dB at 60 Hz, 65 to 70 dB at 3 kHz | [O:AN003, O:GS2005] |
| 4:1 input transformer, balanced to unbalanced | ≈ 120 dB at 60 Hz, 85 dB at 3 kHz | [O:AN003] |
| Transformer isolator vs active, same unbalanced sources | 90 to 110 dB, over 70 dB better | [O:AN004] |

- Only common-mode impedance balance matters for rejection. Signal symmetry does not [O:GS2005, quoting IEC
  60268-3].
- A floating (isolated) device puts its chassis voltage on the isolator as common mode. With 120 dB CMRR and
  60 V, the output is still about −74 dB re 300 mV. Grounding the device separately brings it to about −110 dB
  [O:AN004].

### 2.5 Why hum changes when cables are moved or touched

- **Loop area.** A stereo pair tied at both ends forms a loop. Magnetic pickup ∝ loop area. Separating L and R
  cables increases hum [O:AN004, O:GS2005].
- **Contact resistance.** Connectors left undisturbed oxidize and develop high, often non-linear contact
  resistance. "Hum or other noise that changes when the connector is wiggled indicates a poor contact"
  [O:GS2005].
- **Plug reversal.** Reversing a 2-prong plug almost always changes the hum level, because it changes which
  capacitance carries more current [O:AN004].
- **Other equipment.** Hum can appear and disappear when unrelated equipment is switched [O:AN004].
- **Touching.** A hand on a floating chassis or a live tip adds a body-coupled path (§1.2). A hand on grounded
  metal can remove one [O:FRALIN, summary].

### 2.6 Remedies and how they sound

| Remedy | Audible effect | Source / label |
|---|---|---|
| Heavier shield (lower mΩ/ft), shorter cable | Whole spectrum drops by 20·log(R1/R2). Colour unchanged. | [O:AN004, O:BROWN] |
| Bundling cables, moving from fields | Magnetic component falls; resistive component unchanged | [O:AN004] |
| Output-type isolator | Hum largely gone. Buzz (upper harmonics) remains, only about 35 dB down at 3 kHz. Residual is thinner and buzzier. | [O:GS2005] |
| Input-type isolator | Hum and buzz both gone to the noise floor. Cheap "telephone-grade" transformers can lose deep bass, distort bass and blur transients. | [O:GS2005] |
| Active balanced input from unbalanced source | Only 15 to 30 dB less hum. The spectrum is mostly preserved. | [O:AN004] |
| Shield lifted at one end (balanced only) | Shield current stops. RF risk rises, because the shield can act as an antenna. A capacitor on the lifted end restores RF grounding. | [O:RANE151, O:GS2005] |
| Safety-ground lift | Illegal and dangerous. It can electrocute, and fault current can melt the signal cable. | [O:GS2005, O:BROWN] |
| Single outlet / star distribution | Ground voltage differences shrink toward a few mV | [O:GS2005, O:SOS] |
| Power conditioner / "balanced power" | Marginal; 10 to 15 dB "more routinely" | [O:AN004, O:GS2005, O:BROWN] |

[I] A plug-in must never present a mains safety-ground lift as a recommended fix. It can model a **signal-shield
lift** or an **isolator** as switchable interface states.

---

## 3. Resistance and its effects in cables and connectors

### 3.1 Conductor and shield resistance

- Inner-conductor resistance is "insignificant" for hum. Shield resistance is what matters [O:GS2005].
- The only cable property with a significant effect on audio noise coupling is shield resistance [O:GS2005].
  A cable at 6.5 mΩ/ft with 90 % coverage is preferred over one at 25 mΩ/ft with 100 % coverage [O:AN004].
- Cable capacitance with source impedance forms a low-pass. With 1 kΩ and 50 pF/ft, 20 kHz is down 0.5 dB at
  50 ft, 1.5 dB at 100 ft and 4 dB at 200 ft [O:GS2005].
- Belden 8241F: 17 pF/ft and 2.6 mΩ/ft [O:GS2005].
- For unbalanced-to-balanced adapters, total cable capacitance should stay under 1000 pF, "about 20 feet"
  [O:AN003].

### 3.2 Contact resistance, films, fritting and fretting

- Contact resistance is the sum of bulk resistance, constriction (a-spot) resistance and film (tunnel)
  resistance [O:NEY].
- Base metals grow films 10 to 30 Å thick within minutes, then several hundred Å. Noble-metal films stay under
  about 20 Å [O:NEY].
- Films over about 100 Å need **fritting** (electrical puncture) at about 10⁵ to 10⁶ V/cm. **Any voltage below
  30 mV will not permanently damage a film** [O:NEY].
- The dry-circuit regime is ≤ 20 mV open-circuit and ≤ 100 mA [O:DRYCIRCUIT, summary].
- Fretting: contact resistance of tin-plated contacts rises with fretting cycles as insulating oxide debris
  collects. Studies use 100 mΩ as a failure threshold [O:FRETTING, summary]. Test amplitudes run from about ±5
  to ±90 µm at 10 Hz, and shorter tracks (±5 µm) rise faster. A thin oxide film gives an initial resistance
  hump before it cracks [O:LAU2013]. Spec limit for consumer interconnects: 60 mΩ [O:LAU2013].
- Metal-oxide junctions act as diodes and cause intermodulation, the "rusty bolt" effect known from RF
  [O:PIM, summary].
- Whitlock reports that oxidized audio connectors develop "often distortion-producing non-linear" resistance
  [O:GS2005].

[I] Line and instrument signals are dry-circuit signals. They never self-clean a tarnished contact, which is why
a dirty jack stays crackly. A plausible nonlinearity is weak and level-dependent: a small series-diode-like term
that matters most at low signal levels. Its magnitude is **unmeasured** here.

### 3.3 Intermittent contacts: timing, micro-arcing and crackle spectrum

- Resistance drifts upward slowly, with rapid intermittences within fractions of a second on top. Tin is
  especially affected [O:MAUL, see Docs/Research.md].
- The connector industry defines a discontinuity as ≥ 10 Ω lasting ≥ 1 µs (EIA-364-46). Shorter events need
  nanosecond detection (EIA-364-87) [O:EIA36446, summary].
- Micro-arcs of 5 to 100 ns can occur below the classical minimum arc voltage and current. They become less
  likely at lower currents, down to a material-dependent limit current (generally < 100 mA) below which none
  are observed [O:NEY].
- Classic minimum arc voltage for gold is about 15 V at 0.38 A [O:ARC, summary].
- Stick-slip gives low resistance during stick and high resistance during slip, up to full separation ("open
  circuit noise") [O:NEY].

[I] Implications:

- At line and instrument levels (≤ a few volts, µA to mA), arcing does not occur. Audible crackle is the signal
  being **gated or resistively modulated**, plus bias or charge steps, as the plug-in already models.
- Real discontinuities can last microseconds, shorter than one sample at 48 kHz (20.8 µs). A band-limited model
  should turn sub-sample opens into short impulses of the right area, rather than ignoring or stretching them.
- A resistance step multiplied by the signal gives a spectrum that is the signal convolved with a step. That is
  broadband with a 1/f character, so bright material crackles more brightly. Clusters (hysteresis) come from
  the Maul model.

### 3.4 Cable capacitance on passive pickups

- Pickup inductance is 1 to 10 H. Winding capacitance is about 80 to 200 pF. Cable adds about 300 to 1000 pF.
  Resonance peak height is 0 to 12 dB and depends on load and eddy currents. Above resonance the response falls
  at 12 dB/octave, or up to 18 dB/octave with eddy currents [O:LEMME].
- Pickups are designed for a load of 1 MΩ or more. Lower loads cut treble [O:WPICKUP].
- Cable measurements: 52 to 160 pF/m. Examples include Sommer Spirit LLX 52, Mogami W2524 130, Belden 9778 148
  and Canare GS-6 160 pF/m [O:CABLESMITHS]. The measurement method is not stated.
- Cable shifts pickup resonance by up to 1.4 kHz across the cables measured, while buffered sources are
  unaffected [O:PAIVA, see Docs/Research.md].

[I] Resonance f₀ = 1/(2π√(L·C)) with C = 100 pF winding + cable. All values are *estimates* from the formulas:

| Cable | 2.2 H (single coil) | 5 H (humbucker-class, *estimate*) |
|---|---|---|
| none | 10.7 kHz | 7.1 kHz |
| 3 m × 52 pF/m | 6.7 kHz | 4.4 kHz |
| 3 m × 130 pF/m | 4.8 kHz | 3.2 kHz |
| 3 m × 160 pF/m | 4.5 kHz | 3.0 kHz |
| 6 m × 130 pF/m | 3.6 kHz | 2.4 kHz |
| 6 m × 160 pF/m | 3.3 kHz | 2.2 kHz |

The 2.2 H figure is a "4.1 kHz, 5 dB peak" single coil from a search-engine summary; treat it as indicative
[O:PICKUP-SEARCH, summary]. The Lemme range brackets these results.

### 3.5 High- vs low-impedance sources

- Typical Zo is 100 Ω to 1 kΩ and Zi is 10 to 100 kΩ, transferring 90 to 99.9 % of the voltage. Termination is
  not needed for audio cables under about 4,000 ft [O:GS2005].
- Consumer unbalanced outputs are usually 330 Ω to 1 kΩ with 4.7 to 47 µF coupling. Semi-pro outputs are 47 to
  220 Ω [O:AN003].
- Electric-field noise usually matters only with very high source impedance [O:GS2005].
- Guitar amp Hi input: 1 MΩ to ground. Lo input: about 68k/68k divider, about 6 dB down [O:ROBINETTE].

[I] Four quantities scale with source and input impedance: touch hum (§1.2), open-tip hum, triboelectric noise
(§1.8) and cable-capacitance tone (§3.4). A single **Source Circuit** switch should drive all four, not only
the RLC filter.

### 3.6 Dirty pots

- CRV (contact resistance variation) is the apparent wiper-to-element resistance measured while the wiper moves
  at constant speed with a specified current [O:POTCOM].
- Sliding-contact noise is "ohmic": its magnitude is proportional to current, verified up to several hundred mA
  [O:NEY]. Sources are tarnish films, embedded debris, stick-slip, bounce and changing a-spots [O:NEY].
- A precision slip ring can achieve 10 µV/mA (10 mΩ equivalent noise resistance) [O:NEY].
- Wire-wound pots add stepwise "resolution noise" [O:NEY].
- DC across a pot, often from a leaky coupling capacitor, makes scratch loud. It grows as electrolytics age
  [O:POTFORUMS, summary].
- Alps RK27: operating life 15,000 cycles, residual resistance ≤ 20 to 30 Ω, attenuation at minimum 70 to
  100 dB depending on value [O:ALPS].

[I] The existing DirtyPot design (multiplicative modulation of the through signal, plus optional DC bias through
a charge circuit) is consistent with "noise ∝ current". A refinement is to scale the jitter with |signal| + DC
rather than only as a gain modulation.

### 3.7 Cold solder joints and broken strands

[I] No primary source with numbers was read for these. By analogy with §3.2 and §3.3:

- A **cold joint** is a film-covered, low-force contact. It behaves like a tarnished contact: a static
  resistance, sensitivity to vibration and temperature, and intermittent opens under flexing.
- A **broken-strand conductor** behaves like a weak spot along the cable. Opens are localized at the break and
  triggered by bending. That is the model `CableHandling` already uses.

### 3.8 TS/TRS and switching jacks

- Guitar-amp input jacks use switching contacts that ground the grid when no plug is inserted, "to reduce
  noise", and for safety [O:ROBINETTE].
- Grounding the tip matters because an unterminated 1 MΩ grid is exactly the high-Z node that §1.2 shows picking
  up about 0.1 V [I].

[I] Sequences to reproduce:

- **Insertion.** On a TS plug the sleeve engages first. The tip then passes through the sleeve spring region and
  finally meets the tip spring. The plug-in already models the sleeve first and the tip later.
- **Shorting jack.** When a plug is withdrawn from a switching jack, the input becomes grounded, so it goes quiet
  rather than humming. Without a shorting jack, the input floats and hums.
- **Missing option.** A "Switching Jack" option on Loose Jack would decide which of these happens. It would make
  the Short to Sleeve concept structural instead of probabilistic.

---

## 4. Deliberate uses in engineering and music

### 4.1 Performance practice and instruments

- **No-input mixing.** Toshimaru Nakamura feeds a mixer back into itself. He did "what the manual says not to
  do" and gives "the biggest share for accidental occurrence" while admitting "some control"
  [O:NAKAMURA, summary]. Mudd's CHI '23 interview study found that practitioners value unpredictability together
  with tactile immediacy [O:MUDD, see Docs/Research.md].
- **Circuit bending.** Reed Ghazala found it in 1967, when an open toy amplifier shorted against a metal drawer.
  His "body contacts" make the performer's body a variable resistor in the circuit [O:GHAZALA, summary].
- **Hardware hacking.** Nicolas Collins's *Handmade Electronic Music* teaches induction-coil pickups for
  electromagnetic fields. It includes a chapter on making "your skin part of the circuit" [O:COLLINS, summary].
- **Electrical Walks.** Christina Kubisch has worked with electromagnetic induction since the late 1970s. From
  2003 her induction-coil headphones have made urban fields audible as "loops of rhythmic sequences ... long
  drones" [O:KUBISCH, summary].
- **Noise music.** Merzbow's early work used contact mics, broken equipment and mixer feedback
  [O:MERZBOW, summary].
- **Guitar.** Tom Morello unplugs his guitar and taps the cable's jack end against the guitar's metal plate or
  his hand, often with a wah, to make a loud buzz. At Reading 1993 the trick reportedly failed in the UK
  [O:MORELLO, summary]. The blog's explanation (UK earthing practice) is not verified here.

### 4.2 Glitch and post-digital music

- Cascone lists "glitches, bugs, application errors, system crashes, clipping, aliasing, distortion,
  quantization noise, and even the noise floor of computer sound cards" as raw materials [O:CASCONE].
- He cites Oval painting on CDs to make them skip, Ryoji Ikeda's *+/−* (1996) with its "high frequencies and
  short sounds", and Carsten Nicolai (Noto) [O:CASCONE].
- Nicolai's *Telefunken* (2000) is a CD of impulse frequencies and test signals. It is meant to be patched into a
  TV's video input so the audio draws stripes [O:TELEFUNKEN, summary]. It runs the interference path in reverse:
  audio into video.

### 4.3 Film and foley

- Ben Burtt made the lightsaber from the idle hum of old Simplex projector interlock motors plus a buzz. The buzz
  appeared when his microphone "passed right behind the picture tube" of a switched-on TV. He mixed the two
  "fifty-fifty" [O:BURTT].
- Other sources add that the recorder's microphone cable was broken [O:BURTT-SEARCH, summary].
- CRT line frequency is 15.625 to 15.750 kHz [O:HSCAN]. A CRT also has strong deflection fields at the frame
  rate.

[I] Burtt's recipe is a moving pickup (microphone or cable) passing a strong magnetic field source. The level
envelope follows proximity. It maps directly onto a "walk past the TV" gesture.

### 4.4 Plug-ins that model hum and noise

| Product | What it offers (as documented) | Source |
|---|---|---|
| iZotope Vinyl | *Electrical Noise* ("60 Hz grounding hum") and *Mechanical Noise* (turntable rumble) gains | [O:VINYL, summary] |
| XLN RC-20 Retro Color | Noise module with sampled textures including "electric circuit hum" and stompbox static; Follow/Duck make noise react to the signal | [O:RC20, summary] |
| UAD Ampex ATR-102 | Hum control, 60 Hz with NAB EQ or 50 Hz with CCIR | [O:ATR102, summary] |
| Soundtoys Radiator | Noisy/Clean switch reproducing the Altec 1567A's noise | [O:RADIATOR, summary] |
| Waves X-Hum (the inverse) | Eight harmonically linked notches for removal | [O:XHUM, summary] |

[I] Every one of these treats hum as a *static layer* (gain, maybe ducking). None makes hum depend on a cause.
Interference's differentiator is causality: hum appears because a shield broke, a tip floated or a loop formed.
The research above supports deepening that.

### 4.5 Takeaways

- [A] Players value controllable unpredictability. Keep the *cause* playable (gesture, lane, MIDI) and the
  micro-structure physical. This is already the plug-in's philosophy.
- [A] The most memorable real interference sounds have a *rhythm or gesture*: GSM stutter, dimmer rasp versus
  dimmer level, the lightsaber proximity swell, Morello's taps. These are the best candidates for new playable
  sources.

---

## 5. Implementation guidance

Each item gives the physics, the DSP model, parameter ranges, the user controls, how it can be played, and what to
avoid. All level ranges are **starting points for PhysicalLimits**, following the project's rule that numbers are
tunable until calibrated [I].

**Level reference.** Map the nominal program level to −18 dBFS. That is the common 0 VU = −18 dBFS alignment,
used here as a convention, not a sourced fact [A]. Whitlock's dB-re-300 mV figures then convert as
dBFS ≈ dB(re nominal) − 18 [I].

### 5.1 Shared mains generator (Environment)

- **Physics.** One mains voltage for the whole rig, at 50/60 Hz ± 1 % [O:EN50160], with small odd harmonics
  from flat-topping [O:AN004, O:GS2005].
- **DSP.** Keep the single phase accumulator. Output the *line voltage* waveform v(φ) = Σ aₙ sin(nφ + θₙ) with
  odd n ≤ 25. Also output per-coupling derived waveforms (§5.2), computed by weighting harmonic *n* with
  coupling-specific gains (additive synthesis is cheap at 13 odd partials).
- **Ranges.**
  - a₃ 0.5 to 5 %, a₅ 1 to 6 %, a₇ 0.5 to 5 %, a₉ ≤ 1.5 %, a₁₁ ≤ 3.5 %, a₁₃ ≤ 3 %, higher odd ≤ 1.5 %
    [O:EN50160, as ceilings].
  - Even harmonics ≤ 0.5 % for the voltage.
  - Drift: a slow random walk of ±0.02 to ±0.1 Hz (*estimate*), never beyond ±1 %.
- **Control.** "Grid Distortion" (Clean grid ... Heavily loaded building). The existing **Mains 50/60 Hz**
  control stays.
- **Playable.** Not directly. Slow drift only (tempo must never retune mains, per the realism spec).
- **Avoid.** Hum at non-mains pitches. Independent phases per slot. A strong 2nd harmonic in field-coupled hum.

### 5.2 Coupling recipes (replace `humClean` / `humBright`)

| Coupling path | Harmonic weighting of the line or current spectrum | Character | Which sources use it |
|---|---|---|---|
| Resistive IR drop of a clean 50/60 Hz current | aₙ (flat) | Round hum, fundamental-dominated | Ground-loop IR from large linear currents |
| Capacitive / E-field into R | aₙ · n / √(1 + (n f / f_c)²), f_c = 1/(2πR·C) | Brighter; harmonics lifted +6 dB/oct up to f_c | Open tip (Loose Jack), Touch, open shield |
| Leakage-current ground loop | aₙ · n (differentiated line voltage) | Odd-harmonic buzz | Ground Loop (2-prong gear) |
| Magnetic (coil or loop) from load currents | iₙ · n, where iₙ is the load-current spectrum (rectifier or dimmer) | Buzzy; set by the loads, not the voltage | Single-coil pickup hum, cable loop area |
| Rectifier ground return (pin 1, internal) | Harmonics of 2f: pulse train of width 1.5 to 3.5 ms per 10 ms half-cycle | 100/120 Hz buzz with even-order mains harmonics | Buzz Band "Supply" type |
| Dimmer-induced | Odd harmonics of f, flat to 1/(π t_rise), amplitude ∝ \|sin α\| | Rasp; spikes twice per cycle | Buzz Band "Dimmer" type |

The table is [I], based on O:GS2005, O:WF2010, O:ELLIOTT and O:ETC.

- **Current code.** `humClean` = s1 + .12·s2 + .18·s3 + .05·s5 and `humBright` include s2. For field coupling,
  set a₂ ≈ 0. Map the **Body** parameter physically: body-to-ground capacitance and contact area shift f_c, so
  "Body" really does change "coupling colour, never a pitch" [I].
- **Avoid.** Brightening hum by adding noise. Brightness must come from harmonic weighting.

### 5.3 New: Ground Loop (environment and connection)

- **Physics.** Shield current × loop resistance, in series with the signal [O:AN004]. Current comes from 2-prong
  leakage (µA to mA, capacitive) or from a ground voltage difference between outlets (mV to about 1 V, or several
  volts to a CATV ground) driven through the loop [O:GS2005, O:AN004].
- **DSP.**
  - V_gl(t) = I_src(t) · R_loop(t).
  - R_loop = L·r_shield + 2·R_contact + R_contact,wear(t).
  - The `R_contact,wear` term can be driven by existing contact states. A partially engaged Loose Jack sleeve, or
    a Cable Handling shield-damage excursion, raises R and therefore hum. That is a new, causal link.
  - I_src is the leakage-current waveform (§5.2 row 3) or ground-difference voltage / R_loop, clipped at a
    "source-limited" current.
  - Add the magnetic loop term ∝ loop area × magnetic waveform.
  - Pass the sum through the Interface CMRR filter (§5.4). Inject it into the C path of the slot the loop runs
    through.
- **Ranges.**

  | Parameter | Range | Source |
  |---|---|---|
  | Cable length | 0.3 to 30 m | [O:AN004 table range] |
  | Shield | 3.6 (heavy braid) to 135 mΩ/m (#26 drain) | 1.1 to 41 mΩ/ft [O:AN004, O:BROWN] |
  | Contact | 50 mΩ per connection nominal; worn 0.05 to 1 Ω | 1 Ω is an *estimate* |
  | Current, 2-prong | 5 µA / 100 µA / 1 mA | [O:AN004] |
  | Current, grounded / other outlet | 1 mV to 1 V difference | [O:GS2005] |
  | Current, CATV | about 200 mA through the chain | [O:AN004] |
  | Resulting hum, Natural | −110 to −40 dB re nominal | *estimate* |
  | Resulting hum, Expressive | up to −10 dB re nominal (CATV case) | *estimate* |

- **Controls.**
  - **Other Gear** (Low-power / Mid / High-power / Grounded).
  - **Outlets** (Same strip / Same room / Across the building / Cable-TV line).
  - **Cable Run** (m).
  - **Shield** (Foil+drain / Braid / Heavy braid).
  - **Interface** (§5.4).
  - Monitor: show the computed dB re nominal and the dominant path.
- **Playable.**
  - Wiggling the plug (Loose Jack) or bending damaged shield (Cable Handling) modulates R_contact, so the hum
    breathes with movement.
  - A "Separate L/R" or "Coil the excess" gesture raises loop area.
  - A MIDI note or lane step "switches on other gear". The leakage current steps and plug polarity flips (a
    plug reversal changes the level [O:AN004]).
- **Avoid.**
  - Ground-loop hum on a single isolated device.
  - A loop that survives an input-type isolator at full level.
  - Presenting a mains-earth lift as a fix.
  - A pure-sine loop.

### 5.4 New: Interface / remedy selector with frequency-dependent CMRR

- **Physics.** Rejection depends on common-mode impedance balance and on transformer type. Transformer rejection
  falls with frequency through interwinding capacitance [O:GS2005, O:AN003].
- **DSP.** Treat the ground-noise voltage as common-mode. The residual equals the noise filtered by
  H_rej(f) = 10^(−CMRR₆₀/20) · (1 + j f/f_k), a first-order rise above a knee f_k. That approximates
  "decreasing at 6 dB per octave" [O:AN003]. Optionally add a transformer high-pass and a small bass saturation
  for the "cheap isolator" state (loss of deep bass, bass distortion [O:GS2005]).
- **States** (CMRR at 60 Hz → at 3 kHz):

  | State | CMRR at 60 Hz | CMRR at 3 kHz | Source |
  |---|---|---|---|
  | Unbalanced | 0 | 0 | |
  | Balanced active, unbalanced source | 30 dB | ≈ 25 to 30 dB | [O:AN003, O:AN004] |
  | Output-type isolator | 70 dB | 35 dB | [O:GS2005] |
  | Input-type transformer | 100 dB | 65 to 70 dB | [O:GS2005, O:AN003] |
  | Shield lifted at receiver (balanced) | shield-current path removed | RF term +6 to +12 dB | *estimate* |

- **Controls.** "Interface: Unbalanced / Balanced / Balanced + Isolator Box / Balanced + Input Transformer",
  "Lift Shield (far end)".
- **Playable.** Automation of Interface is a dramatic, physically honest *filter* on the noise: output-type
  isolators leave a thin buzz.
- **Avoid.** Claiming balanced means silent. Applying CMRR to the *signal*.

### 5.5 Dimmer buzz (new Buzz Band type)

- **Physics.** A phase-control dimmer switches current in about 5 µs (cheap) or 350 to 800 µs (theatrical
  choke). Induced voltage ∝ dI/dt, largest near 50 % brightness [O:WF2010, O:ETC].
- **DSP.**
  - At each half-cycle zero crossing, schedule a firing at phase α. Brightness 0 to 100 % maps to α from 180° to
    0°.
  - Emit a band-limited pulse of polarity (+, −, +, −), amplitude ∝ |sin α| · load, and width t_rise. Use a
    raised-cosine derivative, or a minBLEP step through a one-pole of time constant t_rise.
  - The result is odd harmonics of f with a spectral knee at about 1/(π t_rise).
  - Optionally add a 2f acoustic "filament sing" component to the *mechanical* path only.
- **Ranges.**
  - t_rise: 5 µs (Cheap wall dimmer) to 800 µs (TV-studio choke) [O:WF2010, O:ETC].
  - Load 0 to 100 %. Half load shortens rise to 80 % [O:ETC].
  - Level through the Ground Loop or magnetic coupling (not an independent layer).
- **Controls.** "Dimmer" (0 to 100 % brightness), "Dimmer Kind" (Cheap wall / Stage / TV studio).
- **Playable.** Sweeping brightness sweeps the buzz timbre and level through a maximum at half: a
  physically-derived "wah". The Gesture Lane can "fade the lights". Firing phase is locked to mains, not to
  tempo.
- **Avoid.** Pitch changes (the buzz stays locked to mains). Even-harmonic dimmer buzz.

### 5.6 Supply / rectifier buzz (existing Buzz Band, refined)

- **Physics.** Capacitor-input rectifiers draw pulses at 2f with 15 to 35 % conduction [O:ELLIOTT]. Pulses in
  ground wiring cause buzz. Centre-tapped rails ripple at f [O:ELLIOTT].
- **DSP.** Replace |sin|^10 with a pulse whose width is set by conduction time (1.5 to 3.5 ms at 50 Hz), plus a
  small sawtooth ripple at 2f. Offer "Split supply" to shift to f.
- **Controls.** "Supply Load" (sets conduction width and pulse height).
- **Avoid.** Using this as the Touch hum.

### 5.7 RF: square-law detected interference (refines `rf`)

- **Physics.** Junction detection is square law. Audible level ∝ RF² [O:BROWN, O:TI-EMIRR].
- **DSP.**
  - Build an RF *envelope* e(t) per transmitter type. Output y = k · e(t)² → DC blocker → receiver low-pass
    (2 to 5 kHz for "radio", wider for spark crackle).
  - Types:
    1. **AM station**: e = 1 + m·p(t), where p is a *synthetic* programme. Use formant-filtered noise
       syllables or a user-invisible internal source [A], never real broadcasts.
    2. **Two-way / CB**: keyed bursts of 0.5 to 8 s (*estimate*), with a key-up transient and an optional SSB
       "shifted voice" [O:SSB for the character].
    3. **Motor sparks**: Poisson impulses at commutation rate (50 to 300 events/s, *estimate*), square-law
       detected into crackle.
- **Ranges.** "Radio Distance" in dB of RF. Audible output moves 2 dB per dB. Natural max about −40 dB re
  nominal (*estimate*).
- **Controls.** "Transmitter" (AM Station / Two-way / Motor), "Distance", "RF Filter" (ferrite/filter: reduces
  RF by 0 to 30 dB, so audible by 0 to 60 dB).
- **Playable.** MIDI note = "key the radio". Distance on a lane gives proximity swells.
- **Avoid.** Clean intelligible speech. Detected output that scales linearly with the control.

### 5.8 GSM burst (new small source)

- **Physics.** See §1.6 [O:CLAESSON, O:BROWN, O:SCIENCEABC].
- **DSP.**
  - Pulse train at 216.667 Hz, 577 µs on (duty 1/8), 100 % AM.
  - Every 26th frame blank (full rate). "Half rate" gives about 108 Hz with its own pattern.
  - Band-limited edges (polyBLEP). Square-law detection is already implied by a unipolar envelope.
  - Receiver high-pass at about 100 Hz and low-pass at 3 to 5 kHz give the 1/f² harmonic decay
    [O:CLAESSON].
  - Level varies slowly with "power control" (±3 dB drifts, *estimate*).
- **Call states** [A for sequencing, O:SCIENCEABC for the existence of pre-ring bursts]:
  - **Check-in**: short bursts about 0.3 to 1 s, a few times.
  - **Ringing / handshake**: irregular dit-dit clusters for 1 to 3 s.
  - **In call**: continuous.
- **Controls.** "Phone" (Away / Check-in / Incoming / In Call), "Phone Distance" (0.1 to 2 m; influence within
  1 to 2 m [O:CLAESSON]).
- **Playable.** Note-on triggers a handshake sequence. Its internal rhythm stays physical (216.7 Hz,
  120 ms blanking), and only its start is quantized.
- **Avoid.** Retuning the 216.7 Hz to key. Presenting it as current-network behaviour (label it "2G phone").

### 5.9 SMPS / computer whine (new, sidechain-controllable)

- **Physics.** Burst-mode repetition falls into the audio band at light load. Ferrite resonances are at 8 to
  15 kHz [O:PATENT-SMPS]. Computer noise follows CPU/GPU load [O:GEARSPACE, summary].
- **DSP.**
  - Pulse-packet oscillator whose repetition frequency f_b = g(load). Load 0 to 1 maps to 20 kHz down to 200 Hz
    (*estimate*).
  - Packets excite a resonant bandpass at 8 to 15 kHz.
  - Add a "digital hash" term: impulses gated by load bursts.
  - Couple through the ground loop, so Interface rejection applies.
- **Controls.** "Computer Load" (manual or **follows sidechain level**; audible content comes only from the
  internal oscillator, preserving the sidechain rule), "Charger Plugged".
- **Playable.** Following the kick drum makes the whine chirp with the groove. That is plausible because the
  "load" is simply the sidechain envelope [A].
- **Avoid.** Passing sidechain audio through. Whine on a battery-powered, isolated chain (offer "On Battery" to
  switch it off, per [O:GEARSPACE]).

### 5.10 Triboelectric / microphonic cable (Cable Handling refinement)

- **Physics.** Friction and deformation release charge at quasi-DC to 100 kHz [O:FEMTO]. The voltage appears
  across the source impedance [I].
- **DSP.**
  - Add a triboelectric term alongside the modal "Mic" path. Charge impulses (rate ∝ rub speed, size ∝ bend
    acceleration) are integrated by a one-pole (cable C), then high-passed by the source impedance:
    corner = 1/(2π Z_s C_cable).
  - With Buffered/Line (Z_s ≈ 100 to 1k Ω) the corner is far above audio, so near-silence. With Passive Pickup,
    audible thumps and rustle.
- **Ranges.** Peak Natural −30 dB re nominal, Expressive −15 dB (*estimates*). Shield "Low-noise cable" option:
  −35 dB (low-noise) or −60 dB (super-low-noise) [O:IEEE-TRIBO].
- **Controls.** "Cable Type" (Standard / Low-noise), existing "Mic" for the transducer path.
- **Playable.** Shake and tap gestures (existing).
- **Avoid.** Triboelectric noise on buffered sources. Continuing noise after motion stops (the spec already
  forbids this).

### 5.11 Contact intermittency upgrades (Loose Jack, Cable, Pot)

- **Physics.** Opens can be shorter than a sample [O:EIA36446]. No arcs at audio level [O:NEY]. Noise ∝ current
  [O:NEY]. Films persist below 30 mV [O:NEY].
- **DSP.**
  1. Sub-sample opens become an impulse of area −x·τ (signal × duration) injected via a minBLEP residual. Add a
     "fizz" density parameter for fretting.
  2. Scale pot jitter by (|x| + V_dc).
  3. An optional "Tarnish" nonlinearity: y = x − r·tanh(x/V_t) with V_t ≈ 30 mV equivalent (*estimate*). It
     gives low-level crunch that fades at high level [I/A].
  4. Keep the lognormal dwell model. Log dwell times in telemetry for later calibration.
- **Avoid.** Synthesized arc sizzle on line-level signals.

### 5.12 Passive pickup, cable and magnetic pickup hum

- **Physics.** L 1 to 10 H, winding C 80 to 200 pF, cable 52 to 160 pF/m [O:LEMME, O:CABLESMITHS]. Single
  coils are directional hum antennas [O:WPICKUP].
- **DSP.**
  - Parameterize the existing RLC as Pickup (Single coil 2.2 H / P-90-class / Humbucker 5 H, *estimates*) and
    Cable (length × pF/m preset: Low-cap 52, Standard 130, High 160).
  - Load = 250k or 500k pot ∥ 1 MΩ amp.
  - Add **pickup hum**: magnetic waveform (§5.2) × cos(θ_guitar) for single coils, and × 0.03 for a humbucker
    (−30 dB, *estimate*).
  - Pass pickup hum through the *same* pickup RLC as the strings.
- **Controls.** "Pickup", "Cable", "Guitar Angle", "Hand on Strings" (grounds the body: touch hum −20 dB,
  *estimate*, [O:FRALIN]).
- **Playable.** Angle lane or CC ("turning away from the amp"). Hand-on-strings as a momentary MIDI hold.
- **Avoid.** Pickup hum in Buffered/Line mode.

### 5.13 Touch coupling calibration (Exposed Plug Touch)

- **Physics.** See §1.2.
- **DSP.** Level ∝ R_in/|R_in + Z_body|, colour through f_c = 1/(2π R_in C_body). Replace "Circuit Exposure"
  internally with **Input Impedance** presets (Hi-Z 1 MΩ, 100k, Line 10k, Mic 1.5k). Keep the old label as the
  user-facing name if preferred.
- **Ranges.** From §1.2: about −17 dBV at 1 MΩ, falling 20 dB per decade (*estimates*).
- **Playable.** Morello-style taps: short touch gestures on an exposed tip with a wah in the Studio chain [A].
- **Avoid.** Touch hum as loud on a line input as on a guitar amp.

### 5.14 Suggested PhysicalLimits additions (starting points, all unmeasured)

| Id | Unit | Natural | Expressive | Basis |
|---|---|---|---|---|
| groundLoopMax | dB re nominal | −40 | −10 | AN-004 table (−37 worst normal); CATV −6 |
| cmrrFloor | dB | 0 | 0 | Unbalanced = 0 [O:GS2005] |
| dimmerRiseMin | µs | 5 | 5 | [O:WF2010] |
| gsmLevelMax | dB re nominal | −35 | −15 | *estimate* |
| rfDetectedMax | dB re nominal | −40 | −20 | *estimate* |
| smpsWhineMax | dB re nominal | −50 | −25 | *estimate* |
| triboPeak | dB re nominal | −30 | −15 | *estimate* |

---

## 6. What the plug-in models today, and the gaps

| Area | Today (from source) | Assessment |
|---|---|---|
| Mains hum | One global phase, 50/60 Hz glide, two fixed harmonic recipes, both with a 2nd harmonic (`Environment.h`) | Right structure (shared phase). Recipes are not tied to coupling physics, and the 2nd harmonic is unjustified for field pickup [I]. |
| Buzz Band | \|sin\|^10 pulses at 2f | Rectifier-like. No dimmer type, conduction width or firing angle [I]. |
| RF "Interference" | Band-passed noise × random slow AM in bursts | Plausible texture, but linear in the control and not square-law. No transmitter types [I]. |
| Loose Jack | Tip/sleeve engagement, semi-Markov contact, open tip → hum by Shield, short to sleeve | Good. Hum is exposure only: no ground-loop IR through sleeve resistance, no switching-jack option, no dependence on input impedance [I]. |
| Plug Touch | Touch area, pressure, Body colour, Exposure, Shield, load LP | Good. Level not tied to input impedance; Body not tied to a physical corner frequency [I]. |
| Cable Handling | Conductor and shield contacts at weak spot; microphonic modal bank with Mic | Good. No triboelectric term that scales with source impedance; shield damage is exposure-only, not IR [I]. |
| Dirty Pot | Taper, worn arc, jitter, DC bias, tone circuit | Good and consistent with Ney. Could scale jitter with current [I]. |
| Source Circuit | Buffered vs Passive RLC with Cable Load pF | Good. No pickup-type or cable-length presets, and it does not drive other injections [I]. |
| Ground loop, interface, CMRR | none | Main gap. |
| GSM, SMPS/computer | none (Create "Buzz" excitation is a labelled studio source) | Gap. |

---

## 7. Gap and priority table

Impact means musical value plus realism gain. Effort is a rough implementation estimate for this codebase.

| Rank | Gap | Impact | Effort | Why |
|---|---|---|---|---|
| 1 | **Coupling-aware hum spectra** (line waveform × coupling weighting; no field 2nd harmonic; Body → f_c) | High | Low | Every hum in the plug-in improves at once. Pure Environment/envWave change, with sources choosing a path. |
| 2 | **Ground Loop control** (I·R model, cable run, shield, outlets, R_contact linked to sleeve and shield contact states) | High | Medium | Most common real-world hum. Directly playable through existing contact motion. Numbers well sourced (AN-004). |
| 3 | **Interface / remedy selector with CMRR(f)** | High | Low (after 2) | Turns remedies into audible, honest filters. Output-type isolator buzz residue is distinctive. |
| 4 | **Dimmer buzz type** (firing angle + rise time) | High | Low-Medium | Well-sourced (Whitlock & Fox, ETC). Gives a new "fade the lights" gesture. |
| 5 | **Source-impedance-aware injection** (touch, open-tip, tribo, RF scale with Source Circuit / Input Impedance) | Medium-High | Low | Fixes a realism hole: line inputs currently hum like guitar amps. |
| 6 | **GSM burst source** | Medium | Low | Iconic, rhythmic, exact spec. A good one-shot or lane source. |
| 7 | **Square-law RF with transmitter types** | Medium | Medium | Better radio-breakup design. Needs a synthetic programme source [A]. |
| 8 | **Single-coil pickup hum + Guitar Angle / Hand on Strings** | Medium | Low | Natural guitar gesture. Reuses Passive Pickup. |
| 9 | **SMPS / computer whine following sidechain load** | Medium | Medium | Modern, relatable. Sidechain as control fits the routing rules. |
| 10 | **Triboelectric term in Cable Handling** | Medium | Low | Makes Mic/handling depend on the source, as physics says. |
| 11 | **Pickup and cable presets** (L, pF/m × length) | Low-Medium | Low | Better labels for an existing model. |
| 12 | **Sub-sample opens, current-scaled pot noise, Tarnish nonlinearity** | Low-Medium | Medium | Finer crackle realism. Magnitudes unmeasured. |
| 13 | **Pin-1 problem toggle / "hummer"** | Low | Low | Educational; a niche but authentic buzz path. |
| 14 | **Switching-jack option on Loose Jack** | Low | Low | Makes "withdrawn = quiet or humming" structural. |

**Calibration priorities** (extending Docs/Research.md release gate): record a 3 m and a 6 m unbalanced run
between two 2-prong devices and one grounded device. Record through an output-type and an input-type isolator, a
dimmer at 25/50/75 %, a GSM-era handset if one is available, and a finger on a 1 MΩ vs 10 kΩ input. Compare
spectra against §5.2.

---

## References

Access notes: "full" means full text was read; "summary" means only a search-engine summary; "abstract" means
only the abstract.

- **[AN004]** Whitlock, B. *Hum & Buzz in Unbalanced Interconnect Systems*, Jensen AN-004 (1996). Full.
  https://www.jensen-transformers.com/wp-content/uploads/2014/08/an004.pdf
- **[AN003]** Whitlock, B. *Interconnection of Balanced and Unbalanced Equipment*, Jensen AN-003 (1995). Full.
  https://www.jensen-transformers.com/wp-content/uploads/2014/08/an003.pdf
- **[GS2005]** Whitlock, B. *Understanding, Finding, & Eliminating Ground Loops in Audio & Video Systems*,
  generic seminar (2005). Full.
  https://www.jensen-transformers.com/wp-content/uploads/2014/08/generic-seminar.pdf
- **[WF2010]** Whitlock, B., Fox, J. *Ground Loops: The Rest of the Story*, AES 129th Convention (2010), generic
  version. Full.
  https://www.jensen-transformers.com/wp-content/uploads/2015/02/AES-Ground-Loops-Rest-of-Story-Whitlock-Fox-Generic-Version.pdf
- **[RANE151]** Rane Note 151, *Grounding and Shielding Audio Devices*. Full.
  https://www.ranecommercial.com/legacy/note151.html
- **[RANE110]** Rane Note 110, *Sound System Interconnection*. Partial.
  https://www.ranecommercial.com/legacy/pdf/ranenotes/Sound_System_Interconnection.pdf
- **[SOS]** Robjohns, H. *Understanding & Solving Ground Loops*, Sound On Sound (2023). Summary and page.
  https://www.soundonsound.com/sound-advice/understanding-solving-ground-loops
- **[BROWN]** Brown, J. *RFI in Audio Systems: Pin 1 Problems, Poor Shielding, and Poor Input/Output Filtering*,
  Audio Systems Group. Full. http://audiosystemsgroup.com/AES-RFI-SF08.pdf
- **[TI-EMIRR]** Hall, C., Kuehl, T. *EMI Rejection Ratio of Operational Amplifiers*, TI SBOA128A. Full.
  https://www.ti.com/lit/an/sboa128a/sboa128a.pdf
- **[TI-TDMA]** TI AN-1496, *Noise, TDMA Noise, and Suppression Techniques*, SNAA033D. Full.
  https://www.ti.com/lit/an/snaa033d/snaa033d.pdf
- **[CLAESSON]** Claesson, I., Rossholm, A. *Notch Filtering of Humming GSM Mobile Telephone Noise*. Full.
  https://www.diva-portal.org/smash/get/diva2:836430/FULLTEXT01.pdf
- **[SCIENCEABC]** *Why Do Speakers Make A Weird Noise Near A Ringing Phone?* Summary.
  https://www.scienceabc.com/eyeopeners/speakers-make-weird-noise-near-ringing-phone
- **[EN50160]** BS EN 50160:2007, *Voltage characteristics of electricity supplied by public distribution
  networks*. Full text (sections 4.1, 4.11).
  https://fs.gongkong.com/files/technicalData/201110/2011100922385600001.pdf
- **[ETC]** ETC, *Risetime and Lamp Noise: What's the Connection?* Full.
  https://support.etcconnect.com/ETC/FAQ/Risetime_and_Lamp_Noise_-_What's_the_Connection
- **[CONTROLBOOTH]** *Dimmer rise time*, ControlBooth wiki. Summary.
  https://www.controlbooth.com/wiki/Dimmer-rise-time
- **[ELLIOTT]** Elliott, R. *Linear Power Supply Design*, ESP. Full page and summary.
  https://sound-au.com/power-supplies.htm
- **[WMH]** *Mains hum*, Wikipedia. Page. https://en.wikipedia.org/wiki/Mains_hum
- **[PATENT-SMPS]** *Method and apparatus to reduce audio frequencies in a switching power supply* (patent family,
  e.g. US 7471530), plus related burst-mode literature. Summary.
  https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/7471530
- **[GEARSPACE]** *Where do PC noises in USB interfaces come from?* Gearspace. Summary (practitioner reports).
  https://gearspace.com/board/audio-student-engineering-production-question-zone/1421200-where-do-pc-noises-usb-interfaces-come.html
- **[HIFIME]** *Fix USB Audio Noise, Hum and Ground Loops*, Hifime. Summary.
  https://hifimediy.com/2026/05/fix-usb-audio-noise/
- **[OSI]** *The Source of Mains Hum*, Open Source Instruments. Page.
  https://www.opensourceinstruments.com/Electronics/A3013/Hum.html
- **[WBODY]** *Body capacitance*, Wikipedia. Page. https://en.wikipedia.org/wiki/Body_capacitance
- **[EXTREME]** *Capacitance of a human body*, Extreme Electronics. Summary.
  https://www.extremeelectronics.co.uk/capacitance-of-a-human-body/
- **[SCOPEFORUM]** *50Hz when you touch Oscilloscope probe*, Electronics-Lab forum. Summary.
  https://www.electronics-lab.com/forums/threads/50hz-when-you-touch-oscilloscope-probe.307193/
- **[FRALIN]** *Understanding Guitar Grounding*, Fralin Pickups. Summary.
  https://www.fralinpickups.com/2018/11/12/understanding-guitar-grounding/
- **[HAZE]** *Hum and Guitar String Ground*, Haze Guitars. Summary.
  https://hazeguitars.com/blog/hum-and-guitar-string-ground
- **[LEMME]** Lemme, H. *The Secrets of Electric Guitar Pickups*. Full; the HTTPS certificate had expired, so it
  was read over HTTP. http://buildyourguitar.com/resources/lemme/
- **[WPICKUP]** *Pickup (music technology)*, Wikipedia. Page.
  https://en.wikipedia.org/wiki/Pickup_(music_technology)
- **[PICKUP-SEARCH]** Search summary on single-coil 2.2 H / 4.1 kHz figures (secondary; source page not opened).
- **[CABLESMITHS]** *Guitar Cable Capacitance league table*, Cablesmiths. Page.
  https://cablesmiths.com/blogs/news/the-capacitance-league-table
- **[PAIVA]** Paiva, R., Penttinen, H. *Cable Matters*, AES 131st Convention (2011). Abstract (see
  Docs/Research.md). https://aes2.org/publications/elibrary-page/?id=15992
- **[MAUL]** Maul, C., McBride, J., Swingler, J. (2001) and Maul, McBride (2002). Abstracts (see
  Docs/Research.md). https://eprints.soton.ac.uk/22114/ ; https://ieeexplore.ieee.org/document/1040838/
- **[NEY]** Deringer-Ney, *Ney Contact Manual: Electrical Contacts for Low Energy Uses*, ch. 1. Full.
  https://deringerney.com/wp-content/uploads/2022/04/Ney-Contact-Manual-Revised-1st-Edition.pdf
- **[EIA36446]** EIA-364-46 *Microsecond Discontinuity Test Procedures*, GlobalSpec record. Summary.
  https://standards.globalspec.com/std/13313005/eia-364-46
- **[DRYCIRCUIT]** US 4491797A *Test contact resistance of dry circuit contacts*. Summary.
  https://patents.google.com/patent/US4491797A/en
- **[FRETTING]** Fretting corrosion of tin-plated contacts (ScienceDirect and ResearchGate). Summary; HTTP 403.
  https://www.sciencedirect.com/science/article/abs/pii/S0301679X07000333
- **[LAU2013]** Lau, J. et al. *Effects of Fretting Corrosion on Au-Sn Plated Contacts*, JSB Tech (2013). Full.
  https://www.digipas.com/documents/Effects-of-Fretting-Corrosion-on-Au-Sn-Plated-Contacts.pdf
- **[PIM]** *Rusty bolt effect*, Wikipedia; *Passive Intermodulation*, Electronics Notes. Summary.
  https://en.wikipedia.org/wiki/Rusty_bolt_effect
- **[ARC]** Search summary of minimum arc voltage and current for gold (15 V, 0.38 A). Secondary.
- **[KEITHLEY]** Keithley, *Low Level Measurements Handbook*, section 2. Full.
  https://xdevs.com/doc/Keithley/Ch2_LowLevMsHandbk.pdf
- **[FEMTO]** FEMTO, *Low-noise cables*. Summary. https://www.femto.de/en/accessories/low-noise-cables/
- **[FEMTO-SEARCH]** Search summary on triboelectric "slapping" noise and conductive PVC layers. Secondary.
- **[IEEE-TRIBO]** *Femtocoulomb range triboelectric noise meter for super-low noise cables*, IEEE. Abstract via
  summary. https://ieeexplore.ieee.org/document/9460067/
- **[POTCOM]** *Potentiometer Resistance Terms*, potentiometers.com. Page.
  https://www.potentiometers.com/potcomFAQ.cfm?FAQID=3
- **[POTFORUMS]** *DC leakage making pots scratchy?* The Amp Garage; *DC on pots*, GroupDIY. Summary.
  https://ampgarage.com/forum/viewtopic.php?t=21701 ; https://groupdiy.com/threads/dc-on-pots-how-much-is-ok.1786/
- **[ALPS]** Alps RK27 datasheet. Full.
  https://analogmetric.com/specification/ALPS/ALPS%20RK271%20datasheet.PDF
- **[ROBINETTE]** Robinette, R. *How Fender Normal/Bright/Hi/Low Input Jacks Work*. Page.
  https://robrobinette.com/How_Fender_Input_Jacks_Work.htm
- **[SSB]** *Single-sideband modulation*, Wikipedia. Page.
  https://en.wikipedia.org/wiki/Single-sideband_modulation
- **[HSCAN]** *Horizontal scan rate*, Wikipedia. Page. https://en.wikipedia.org/wiki/Horizontal_scan_rate
- **[CASCONE]** Cascone, K. *The Aesthetics of Failure*, Computer Music Journal 24(4), 2000. Full.
  https://direct.mit.edu/comj/article/24/4/12/93451/ ;
  https://www.monoskop.org/images/0/01/Cascone_2000_The_Aesthetics_of_Failure_Post-Digital_Tendencies_in_Contemporary_Computer_Music.pdf
- **[NAKAMURA]** *A Guide to Toshimaru Nakamura's No-Input Mixing Board*, Bandcamp Daily; Torn Light Records
  feature. Summary.
  https://daily.bandcamp.com/lists/toshimaru-nakamura-discography-list ;
  https://www.tornlightrecords.com/this-might-cause-feedback-toshimaru-nakamura-and-the-no-input-mixing-board/
- **[MUDD]** Mudd, T. (2023) CHI '23. Abstract (see Docs/Research.md).
- **[GHAZALA]** *Circuit bending*, Wikipedia. Summary. https://en.wikipedia.org/wiki/Circuit_bending
- **[COLLINS]** Collins, N. *Handmade Electronic Music*, book page. Summary.
  https://www.nicolascollins.com/handmade.htm
- **[KUBISCH]** Kubisch, C. *Electrical Walks*. Summary. https://electricalwalks.org/about/ ;
  https://en.wikipedia.org/wiki/Christina_Kubisch
- **[MERZBOW]** *Merzbow*, Encyclopedia.com. Summary.
  https://www.encyclopedia.com/education/news-wires-white-papers-and-books/merzbow
- **[MORELLO]** *Tom Morello's "Nightmare" Performance*, Ground Guitar. Page.
  https://www.groundguitar.com/blog/tom-morello-recalls-his-nightmare-performance/
- **[TELEFUNKEN]** Nicolai, C. *telefunken*. Summary. https://www.carstennicolai.de/?c=works&w=telefunken ;
  https://tba21.org/telefunken
- **[BURTT]** *Ben Burtt Special: Star Wars – The Sounds (Part 1)*, Designing Sound (2009). Page.
  https://designingsound.org/2009/09/03/ben-burtt-special-star-wars-the-sounds-part-1/
- **[BURTT-SEARCH]** Secondary accounts of the broken microphone cable (e.g. Krotos, Cheat Sheet). Summary.
  https://krotos.studio/blog/lightsaber-sound-effect-deep-dive
- **[VINYL]** iZotope Vinyl documentation. Summary.
  https://s3.amazonaws.com/izotopedownloads/docs/vinyl/en/using-vinyl/index.html
- **[RC20]** XLN Audio RC-20 Retro Color. Summary.
  https://www.xlnaudio.com/products/addictive_fx/effect/rc-20_retro_color
- **[ATR102]** Universal Audio, Ampex ATR-102 manual. Summary.
  https://help.uaudio.com/hc/en-us/articles/32491139365140-Ampex-ATR-102-Mastering-Tape-Recorder-Manual
- **[RADIATOR]** Soundtoys Radiator. Summary. https://www.soundtoys.com/product/radiator/
- **[XHUM]** Waves X-Hum. Summary. https://www.waves.com/plugins/x-hum
