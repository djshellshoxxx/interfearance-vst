# Code review 2: DSP commits 9bbe9e8 and 3c9d96a

Reviewer: read-only review agent, 2026-09-24. Scope: `git show -w 9bbe9e8 -- Source Tests` (review / audit fixes) and
`git show -w 3c9d96a -- Source Tests` (environment models from `Docs/research/ElectricalInterference.md`). Line
numbers are for HEAD `3c9d96a`. Checklists: `skills/audio-dsp-review`, `skills/audio-numerics-review`,
`skills/audio-artifacts-debug`.

Severity: **Blocker** = won't compile / will crash / fails a test. **Major** = audible defect, broken guarantee,
sample-rate or block-size dependence, or a regression. **Minor** = everything else worth fixing.

---

## 0. Blockers

None. Both commits are real-time safe:
- no allocation, locks, IO or `std::function` in `process()`;
- the new `static const` tables in `Engine::process` are constant-initialised, so there is no guard variable;
- `std::pow` / `std::exp` run once per block or twice a second, `std::fmod` / `std::sin` once per sample;
- `takeStagePeaks` uses relaxed atomics.

## Verified correct (asked about specifically)

- **DelayedBlep sign and timing (Environment.h:12-30).** With u = time since the step (in samples) at the
  current sample, the 2-point polyBLEP residual is `+h·u²/2` on the sample before the step and `−h·(1−u)²/2` on
  the sample after it. `add()` puts the first term on `prevCorr` (the sample not yet output) and the second on
  `curCorr`, which `out()` moves forward one sample. Both signs and the one-sample alignment are right, and the
  two ends agree: u = 0 and u = 1 both give the midpoint at the step sample.
- **GSM edges (Environment.h:216-243).**
  - Rising edge: detected by `framePhase < prev` after the wrap, with u = `framePhase/inc`, and gated by the
    *new* frame's `frameOn`.
  - Falling edge: `prev < duty ≤ framePhase` with u = `(framePhase−duty)/inc`.
  - A wrap and a duty crossing cannot fall in the same sample at audio rates (`inc` is about 0.0045, far below
    1/8).
  - The first sample after `reset()` produces no edge (prev = framePhase = 0), and the first frame is silent
    until the sequencer runs.
- **Dimmer half-cycle wrap (Environment.h:196-205).**
  - `theta < prevTheta` and the sign flip both detect the new half-cycle.
  - At full brightness the chopped sine is continuous across the zero crossing, so no BLEP is needed.
  - When α < dθ, the firing in the wrap sample gets the correct u.
  - For the problems at reset, on enable and during automation, see m1 and m2.
- **Ground-loop injection before the slots (Engine.h:525-553).** This is physically right. The shield drop is
  in series with the signal in the loop *source hot → tip contact → Zin → receiver ground → shield*, so a
  resistive or open tip divides the hum together with the signal. The sleeve / shield openness term raises
  R_loop and then breaks the loop (`(1 − open)`), which matches §5.3.
- **Ground-loop numbers.** 3 m of #26 drain (0.135 Ω/m × 3 + 2 × 50 mΩ = 0.505 Ω) at 1 mA gives 0.505 mV, which
  is −55.5 dB re 300 mV. Whitlock's table gives −55. The −18 dBFS nominal level is applied consistently.
- **CMRR shape.** `g0·(cm + Δcm·fs/(2π f_k))` becomes `g0·(1 + j f/f_k)` after a half-sample delay.
  - Balanced: 30 → 27 dB at 3 kHz.
  - Isolator: 70 → 35 dB.
  - Input transformer: 100 → 67 dB.

  All three match §2.4 (but see m4 for the reference frequency). K = fs/(2π f_k) makes the term independent of
  sample rate.
- **GSM distance law.** `(0.15/d)²` means audible level ∝ E² ∝ 1/r²: square-law detection in the far field
  (λ/2π ≈ 5 cm at 900 MHz, so 0.1-2 m is far field). The clamp at 1 stands in for detector saturation at
  close range. It falls about 45 dB by 2 m, consistent with "influence within 1 to 2 m".
- **Studio time constants.** The M6 values reproduce the old per-sample constants at 44.1 kHz to within 7 %:
  kFastAtt 0.186 (was 0.2), kFastRel 0.00206, kSlowAtt 0.00503, kSlowRel 0.000302, kCtGain 0.0098,
  kRms 0.0002, kAuto 0.000504.
- **Kill clears.** They now run once per episode (`fbCleared` / `tailCleared`), and the DC blocker is set in
  `prepare()` only. M3 and M4 are fixed as intended. For a side effect of the M4 fix, see Major M3 below.

---

## 1. Major findings

### M1. `smoothTo` does not land exactly: one-pole float smoothing sticks up to ~240 ulp short (Engine.h:987, StudioChain.h:451)
**What is wrong.** `n = t + (v − t)·c` rounds back to `v` whenever `k·(1−c) < ½`, where k is the distance in
ulp. With c = e^(−1/(0.01·fs)) that is k < 0.005·fs: about 240 ulp at 48 kHz and 960 ulp at 192 kHz. The snap
threshold `1e-7·max(1,|t|)` only catches k ≤ 1-2.

Example: after automating Dry/Wet 50 % → 100 %, `wetS` settles at about 1 − 1.4·10⁻⁵ for good. Then:
- `y ≠ W` bit for bit;
- "Dry/Wet 0 % = time-aligned dry input" is no longer exact after any Dry/Wet move;
- `StudioChain::mixTo`'s `k ≥ 1` shortcut never fires once `wet[]` has been automated.

The error is inaudible (about −97 dB), but it breaks the documented bit-exact identities and any null test. The
older one-poles `trim`, `excGain` and `sendGain` (Engine.h:470-471, 404) have the same problem: after an Input
Trim move, `trim` never returns to exactly 1.

The tests miss it because they start from a snapped state, and `automatedGainsAreSmoothedNotStepped` uses a
1e-4 tolerance.

**Fix.** Snap when the step makes no progress. This is exact and costs one compare:
```cpp
static float smoothTo (float v, float target, float c) noexcept
{
    const float n = target + (v - target) * c;
    return (n == v || std::abs (n - target) <= 1.0e-7f * std::max (1.0f, std::abs (target))) ? target : n;
}
// and use it for trim / excGain / sendGain too:
trim = smoothTo (trim, trimTarget, smoothCoeff);
```
Test: run mix 0.5 for 0.5 s, then mix 0 for 0.5 s, then require the last 1 000 samples to equal the input
**exactly** (`== 0.0`). Do the same for mix 1 with Studio off (output == C).

### M2. The dimmer spike is clamped *before* the injection low-pass: its level depends on sample rate (Environment.h:208-210)
**What is wrong.** `dIdt·2` is area-normalised: a step h gives a band-limited impulse with area ∝ h, which is
right because the source's 9 kHz `injLp` integrates it. But the ±1.5 clamp acts on the *sample peak*, and that
peak grows with fs:

| Sample rate | Cheap Wall rise time | Peak at h = 1 (u = ½) | Clamp engages from |
|---|---|---|---|
| 48 kHz | 0.24 samples | about 1.27 | never |
| 96 kHz | 0.48 samples | about 2.4 | sin α ≳ 0.63 |
| 192 kHz | — | about 3.7 | sin α ≳ 0.4 |

Clipping one-sample spikes removes area. The Cheap Wall buzz at half brightness is therefore about 4 dB quieter
at 96 kHz and about 8 dB quieter at 192 kHz. The "loudest at half brightness" curve also flattens over a wide
α range at high rates. The test only runs at 48 kHz.

**Fix.** Keep the area and let `injLp` (plus the source caps) bound the peak. Keep the clamp only as a sanity
guard that scales with fs:
```cpp
const double peakGuard = 1.5 * std::max (1.0, fs / 48000.0) * 2.0;
return std::clamp (dIdt * 2.0, -peakGuard, peakGuard);
```
Test: `rmsOf (renderDimmer (0.5f, 0))` at 48 kHz and at 96 kHz (with `Harness` fs) must be within 1 dB of each
other; do the same for Stage.

### M3. After the once-per-episode clear, a held Kill Tail / Kill Feedback / choke keeps *recording* into the delay lines, and that material plays back on release (StudioChain.h:386-408, 342-346)
**What is wrong.** While `tailKill` is held, `echo[ch].push (tanh (inToEcho + fb))` still writes the live input;
only the *output* is scaled by `tailRamp ≈ 0`. The same goes for `fbLine.push (fbReturn > 0 ? t : 0)`, because
`fbReturn = fbTarget·killRamp` stays slightly above 0. When the kill is released, `tailRamp` / `killRamp` return
to 1 within about 30 ms, and the last 2.1 s of echo input (0.5 s of feedback input) recorded *during the kill*
plays back. The old per-sample wipe prevented this, so this is a behaviour regression of Kill Tail and of
lane chokes that clear the echo.

**Fix.** Write silence while killed. This is cheap and keeps the once-per-episode clear:
```cpp
echo[ch].push (tailKill ? 0.0f : std::tanh (inToEcho + fb));
// feedback tap
fbLine[ch].push (fbReturn > 0.0f && ! killNow ? t : 0.0f);
```
(Whether a resonator should keep ringing on input during a kill is a design choice; its output is muted by
`tailRamp` either way.) Test: hold Kill Tail for 1 s over drums with the Echo on, release it on silence, and
require the output after release to stay below −90 dBFS.

### M4. The new environment levels are block-rate steps: zipper and clicks, the M5 problem again (Engine.h:236-257, 540)
**What is wrong.** Two kinds of change are applied as per-block steps:
- **Continuous parameters stepped per block.** The ground-loop level (Cable Run, via `glRloop`) and the GSM level
  (Phone Distance, via `gsmLevel`) are recomputed each block and used directly. Automating Cable Run or Phone
  Distance makes a zipper at block rate.
- **Choice parameters that jump.** Other Gear, Outlets, Shield, Interface (`g0` jumps by up to 100 dB) and Phone
  Away start or stop a hum at arbitrary phase. The worst case is a GSM burst cut in the middle, because `gsmStep`
  stops and the engine's level goes to 0 in the same sample.

The level is at most backgroundCeiling, but it steps on every block, so the result depends on block size under
automation. The block-size test only uses constant parameters.

**Fix.** Smooth the targets per sample with the same `smoothTo`, and crossfade Interface changes:
```cpp
// members: float glLvlS = 0.0f, glG0S = 1.0f, gsmLevelS = 0.0f;   snap them with snapGains
// per sample
const float glTarget = glGear > 0 ? glLvl : 0.0f;          // compute glLvl as now, but keep it when glGear == 0 -> 0
glLvlS = smoothTo (glLvlS, glTarget, smoothCoeff);
glG0S  = smoothTo (glG0S, glG0, smoothCoeff);              // interface change glides over ~10 ms
gsmLevelS = smoothTo (gsmLevelS, gsmLevel, smoothCoeff);
// run the loop while glLvlS > 0 (not only while glGear > 0) and let env.gsmStep() keep running while the level fades
```
Test: automate Cable Run 1 → 30 m in one block and require the largest sample-to-sample step to stay below 1 %
of the hum peak. Also run a 64-vs-401 block test with automation.

---

## 2. Minor findings

**m1. Dimmer start-up after `reset()` or re-enable (Environment.h:198-206, 157).**
- **What is wrong.** On the first sample (`prevTheta < 0`) mid-conduction, a BLEP is added with u clamped to 1
  and height `sin α`, but the actual jump is from 0 to `sin θ`. `dimI` / `dimPrevI` are 0, so `dIdt` produces a
  full spike. Re-enabling Buzz, or switching Buzz Source to Dimmer, also reuses stale `dimBlep` / `dimI` state
  from the last time. The result is one extra "tick" at every transport start, loop jump and enable. It is barely
  audible among 100 spikes per second, but it is not "no event without a cause".
- **Fix:** initialise to steady state instead of emitting an edge.
```cpp
if (prevTheta < 0.0)
{
    dimBlep.reset(); dimBlep.prevNaive = naive; dimI = dimPrevI = naive; prevTheta = theta;
    return 0.0;
}
```

**m2. The triac turns off mid-cycle under brightness automation (Environment.h:199-200).** `naive` is
re-evaluated every sample against the current `dimAlpha`, which `setDimmer` changes every block.
- If α rises above θ during conduction, the current drops to 0 mid half-cycle. A triac cannot do that: it
  conducts until the zero crossing. The step is also un-BLEP'd.
- If α falls below θ while the triac is off, `prevTheta < dimAlpha` is false, so it fires without a BLEP.

The "fade the lights" gesture advertised in §5.5 triggers both cases. Fix: latch the firing per half-cycle.
```cpp
if (newHalf) fired = false;
if (! fired && theta >= dimAlpha && dimAlpha < kPiD)
{
    fired = true;
    const double since = std::clamp ((theta - std::max (dimAlpha, newHalf ? 0.0 : prevTheta)) / dTheta, 0.0, 1.0);
    dimBlep.add (since, sign * std::sin (theta - since * dTheta)); // height at the actual firing instant
}
const double naive = fired ? sign * std::sin (theta) : 0.0;
```

**m3. The Supply buzz has a DC offset (Environment.h:151).** The mean of |sin|¹⁰ is 252/1024 = 0.2461, not
0.18. The buzz therefore carries +0.072 DC (×1.1) into the injection path, which has only low-passes. This
predates the commit, but the line was touched here. Fix: `(r5 * r5 - 0.24609375) * 1.1`, or high-pass the
waveform.

**m4. CMRR is specified "at mains" but the knee term adds gain at mains (Engine.h:244-250).** `|1 + j·50/53| =
1.37` means the isolator gives 67.2 dB at 50 Hz, not 70. The input transformer gives 97.7 dB. Normalise per
block:
```cpp
const double fm = prm.choice (P::mains) == 1 ? 60.0 : 50.0;
const float glG0 = glIface == 0 ? 1.0f : (float) (ifaceG0[glIface] / std::hypot (1.0, fm / ifaceFk[glIface]));
```

**m5. Shield resistance labels (Engine.h:241).**
- `0.0036 Ω/m` is Belden 8281 *double* braid (1.1 Ω/kft). The heavy braid 8241F is 2.6 Ω/kft = 8.5 mΩ/m. The
  research note's §5.3 range row has the same mislabel.
- "Foil + Drain" uses #26 drain (41 mΩ/ft), but the research table lists foil/drain #22 as 16 Ω/kft
  (52 mΩ/m).

Either relabel as "Double Braid" and cite 8281, or use 0.0085 Ω/m. Say which drain gauge the table is based on.

**m6. Grounded-gear model constants are estimates (Engine.h:243, 538).**
- `Vd·r/(r+0.5)` assumes a 0.5 Ω ground-path source impedance, which has no source.
- "Across Building" at 0.1 V is 10-20 dB below §2.1 ("up to several volts", "1 V is not unusual").
- CATV is modelled as a 1 V outlet difference, but the note describes a current of about 200 mA. The outcome is
  close anyway: about −2 dB re nominal against the note's −6 dB.

Mark these as estimates in the tooltip or Docs. Consider 0.3-1 V for Across Building (it is capped by
backgroundCeiling in Natural anyway).

**m7. Ground loop and GSM are outside the shared noise budget (Engine.h:525-553).** `envScale` / `humPeakCap`
bound only the slots' pickup. The ground loop, GSM and pickup hum can each reach backgroundCeiling and add up
above it. Fix: include `|envInj|` in the budget, or cap the sum of the three.

**m8. GSM state changes are slow or abrupt (Environment.h:112, 179-181, 262-297).**
- `setPhone` does not restart the sequencer. Choosing In Call can wait out the rest of a Check-in quiet segment
  (up to 8 s).
- While the phone is Away, `framePhase` freezes.
- Choosing Away cuts a burst in the middle.

Fix:
```cpp
void setPhone (Phone p) noexcept { if (p != phone) { phone = p; gsmCounter = 0; } }
// keep calling gsmStep() while Away (gsmActive = false) so the filters ring down and the frame clock runs
```

**m9. GSM frame clock is not derived from host time (Environment.h:88).** The mains phase is re-derived from
`hostSeconds` on reset, but `framePhase` restarts at 0. Loops still repeat exactly, but starting playback at bar
5 gives different burst timing at bar 5 than playing through from bar 1. The same history dependence applies to
the call-state counters, which carry across the per-bar reseeds (as RF already does). Suggest:
`framePhase = frac (hostSeconds · 216.667)` and `frameCount = floor (…) % 26`, so the idle frame is also
position-locked.

**m10. Remaining block-size dependence in the switch declicks (Engine.h:272, 382-383, 472-477).**
- A Monitor or Studio Source change that arrives during a running crossfade waits for the next block boundary.
- The excitation-type switch zeroes `excGain` mid-block, but `excGainTarget` stays 0 until the next block.

In both cases the timing depends on the host block size. Fix: evaluate the pending switch per sample (an int
compare), and compute `excGainTarget` per sample from `excTypeActive`.

**m11. Passive filter is reset every sample while off (Engine.h:491).** When it is switched on, the RLC biquad
starts from zero state, so the 10 ms crossfade blends in a start-up transient. Run the filter all the time (one
biquad per channel) so the switch is a pure crossfade.

**m12. Existing presets' sound changed.**
- The hum recipe (odd harmonics only, a₂ = 0, new normalisation) and the ChargeCircuit change alter every
  preset that uses hum or pops. "Defaults = previous sound" holds only for the 10 new parameters.
- `humBright` can peak at about 1.24, not "~1".

Note this in CHANGELOG and in the state-version / compatibility section.

**m13. Test gaps.**
- `killFeedbackAndTailHeldStayCheap` checks wall-clock time (< 2 000 ms), so it can fail on a loaded 4 GB
  machine or a Debug build. Compare against a Kill-off run of the same harness (ratio < 2), or count clears
  through a test hook.
- `groundLoopBreaksWhenTheSleeveOpens` passes through *tip* gating (insert 0 opens the tip too), so the sleeve
  path (R rises, then breaks) is untested. Assert that partial sleeve openness *raises* the hum before it breaks.
- Missing tests:
  - no DelayedBlep unit test (for example GSM at 44.1 kHz: energy at an inharmonic alias bin ≥ 20 dB below the
    naive pulse train);
  - no 48-vs-96 kHz test for Contour or the dimmer;
  - no exactness-after-automation test (M1);
  - no click tests for Monitor, Studio Source, Passive, Input Drive, Ceiling, Excitation type, Other Gear and
    Interface switches (largest step ≤ a threshold);
  - no test that a released kill stays silent (M3).

---

## 3. Summary of fixes by priority

1. M3: push silence into the echo / feedback lines while killed (regression).
2. M1: make `smoothTo` snap on no progress, and use it for `trim`, `excGain` and `sendGain`.
3. M4: smooth ground-loop and GSM levels and the interface g0; keep the generators running while they fade.
4. M2: stop the dimmer spike clamp from scaling with sample rate.
5. m1, m2: dimmer start-up and per-half-cycle latch. m3: Supply buzz DC. m8: GSM state changes.
6. The remaining Minor items and the tests in m13.
