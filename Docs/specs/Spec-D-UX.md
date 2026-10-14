# Spec D — Ease of use, feedback, accessibility (U1, U2, U4, N3, D6, D7)

UI facts: `MainView` (single window; header, mode row, source palette, two `SlotCard`s + `Illustration`s, `ContextPanel`, bottom
tabs lane/Studio/Create/Routing/Environment, `OutputStrip`), `Overlay` base class (Help, Options, Debug, Ghost, Render),
widgets in `UI/Widgets.*` (`ParamKnob`, `ParamToggle`, `ParamChoice`, `LevelMeter`, `OutputLed`, `DataStream`), theme in
`UI/Theme.*`, 30 Hz timer copies `EngineTelemetry` to views, `setFocusOrder()` defines keyboard order.

---
## U1 — First-run guided tour

* `UI/TourOverlay.cpp` (`TourOverlay : Overlay`), shown once when option `tourDone` is false and no preset has been loaded;
  also Help → "Take the tour". Never blocks audio; any click on the dim area advances, `Esc` skips, `Don't show again` checkbox.
* Steps are data (`Tour.def`): `{ componentID, title, body(tr), placement }`. 8 steps: (1) What this is (one sentence, cause→sound),
  (2) Pick a source (`SourcePalette`), (3) Slot cards & illustration (drag the plug), (4) "Why quiet?" button, (5) Play/Trigger,
  (6) Preset box + Randomize, (7) Environment tab, (8) Help `?`.
* Spotlight = dim layer with a rounded cut-out around `findComponentByID`; if the target is hidden (e.g. a tab), the tour selects the
  tab (`MainView::selectTab`) first. Re-layout on resize; tolerant when a target is missing (step skipped, logged in Debug).
* Component IDs: add `setComponentID("tour.<name>")` at construction of the 8 targets (small edits in MainView/SlotPanel).
* Strings through `tr()`. Accessible: each step announced (JUCE `AccessibilityHandler` title/description), keyboard `Enter/Right/Left/Esc`.
* Tests: `tour.runsAllStepsOnce`, `tour.skipPersists`, `tour.missingTargetSkipped`, `tour.resizeKeepsSpotlight`, snapshot of step 3.
* Acceptance: a new user reaches an audible result (load "Ungrounded Input", press Play) within the tour; no state is modified.

---
## U2 — Macro / "Simple" screen

### Concept
A "Simple" view (toggle `Simple | Full` in the header, persisted in options) with a big preset box, two illustrations, four macro
knobs and Dry/Wet + meters. Macros are **automatable parameters** (`ParamsExt/macro.def`: `macro1…macro4`, 0–1, default 0.5 = neutral)
applied by `MacroStage` in `ParamPipeline` as *offsets around the base values*, so Full view still shows the underlying knobs
(ghost ticks show effective values).

| Macro | Meaning | Mapping (per source, normalised offset at ±1 from 0.5 centre) |
|---|---|---|
| Severity | how often/how bad faults are | Loose Jack: wear, intermittency, wiggle; Touch: touch, exposure, pop; Cable: damage, shield damage, intermittency; Pot: dirt, scratch; Env: hum, rf |
| Wear | how worn the hardware is | jkWear, cbDamage, ptDirt, ptWearWidth, tcShield (inverse) |
| Mood | colour | tcBody, jkShield (inverse), ptCircuit tone amount, Echo tone, EQ tilt |
| Space | room/environment | hum, buzz, rf, glGear steps (Off→2-prong→…), stereo |

* Mapping tables live in `Core/MacroMap.h` as `constexpr` arrays `{param, weight, curve}` per `SourceType` (registry field `macroRows`)
  and are unit-tested to stay in range for all extremes.
* Neutral position reproduces exact base values (bit-exact when all macros = 0.5).
* Simple view hides tabs and the Details drawer; `MainView::layoutSimple()` is a separate layout function (no edits inside
  `resized()` beyond a branch) to limit merge risk.
* Tests: `macro.neutralIsIdentity`, `macro.extremesStayInRange`, `macro.automationRoundTrip`, `macro.viewTogglePersists`, GUI snapshot.
* Acceptance: with only the Simple view, a first-time user can reach 10 distinctly different sounds using 4 knobs + presets.

---
## U4 — Meters and "why is it silent?"

Existing: `LevelMeter`, `OutputLed`, `Why quiet?` dialog from `QuietReason` bits, dormant control explanations.

Additions:
1. **Input meter** (pre-hardware) beside the output meter, peak + RMS, clip LED, "No input" hint when |x| < −80 dBFS for 3 s
   and Excitation is Off: chip text "No signal reaches the plug-in — use Create mode / Excitation, or play audio through it."
2. **Silence assistant**: header chip "Quiet" appears if wet-minus-dry RMS < −70 dBFS for 5 s while Mix > 0 in Hardware mode:
   click → the existing Why-quiet dialog aggregated for both slots with *one-click fixes* ("Move plug toward worn area",
   "Press Play", "Turn Hum up", "Switch Mode to Create").
3. **Stage meter strip** in Studio tab already exists (`StageMeter`) — add a tiny per-slot "activity" LED to the slot cards.
4. Telemetry additions in `EngineTelemetry`: `inputPeak/inputRms` atomics (audio thread writes, UI reads), `silentSeconds` computed on
   the UI timer from `wetDiffRms` (new atomic).
* Tests: `meter.inputPeakMatchesSine`, `silence.chipAppearsAndClears`, `silence.fixButtonsApplyKnownChanges`, no allocation in the audio path.
* Acceptance: loading each factory preset with a 440 Hz tone at −18 dBFS never shows the chip unless the preset is by design silent.

---
## N3 — Visual feedback (scope and animated hardware)

* **Telemetry ring** `ScopeBuffer` (SPSC, 4096 min/max pairs @ ~2 kHz decimation, written per block by the audio thread via
  `std::atomic<uint32_t>` write index; reader copies on the UI timer). Buses: C (output wet), A (injected), D (fault difference).
* **ScopeView** component: waveform (min/max), optional spectrum (2048-pt `juce::dsp::FFT` on the UI thread from a copy of the
  last 2048 samples of a *separate* 8 kHz-decimated stream), peak-hold. CPU ≤ 1 % of one core at 30 Hz (measured on test laptop).
* **Illustration animation**: the existing functional illustrations already move with telemetry; add easing and small particles
  (sparks on `Open→Good` transitions, dust on pot scratch) driven by `SourceTelemetry` deltas. Respect "reduced motion" (D6).
* Placement: scope at the bottom of the Routing tab and as a pop-out overlay (`S`). Off in Light mode (D3).
* Tests: `scope.ringWrapNoTear`, `scope.minMaxCorrect`, `scope.noAllocation`, `scope.disabledInLightMode`, snapshot test.

---
## D6 — Accessibility

* Audit with a script `tools/a11y_audit.py` that parses the GUI test's dump of the component tree (add `TestGui` dumper):
  every interactive component has non-empty `getTitle()` and `getDescription()`, `isAccessible()`, keyboard focusable, appears in
  focus order, minimum 24×24 px.
* **Contrast**: `tools/contrast.py` computes WCAG ratios for every `Theme` text/background pair (≥ 4.5:1 body, ≥ 3:1 large/UI);
  fix colours (e.g. `textMuted 0xff8a929e` on `panel 0xff171b22` is ~5.9:1 — verify) and add **High-contrast theme**
  (Options) with pure white text and 2 px focus rings.
* **Never colour-only**: states (modified, armed, error) also use shape/text. Focus ring always visible.
* **Reduced motion** option disables value animation, particles, scope smoothing.
* **Screen reader**: JUCE `AccessibilityHandler` for custom components (`ParamKnob` as slider with value text, `LaneEditor`
  as grid with step state, `Illustration` as image with generated description "Loose jack, plug 63 % inserted, contact good").
* Tests: `a11y.allControlsNamed`, `a11y.focusOrderComplete`, `a11y.contrastTable`, `a11y.laneGridAnnouncements`.
* Acceptance: operate the whole plug-in (load preset, change a knob, edit a lane step, open Help) with keyboard only; NVDA
  (Windows) reads control names and values (manual QA row).

---
## D7 — Localisation

* Infrastructure (F0.6) first: `tr("text")` everywhere; `LocalisedStrings` loaded from embedded `.txt` resources
  (`Resources/i18n/<lang>.txt`, binary data via `juce_add_binary_data`) chosen by Options → Language (Auto/English/…).
* String inventory: `tools/extract_strings.py` scans for `tr("…")`, `TRANS("…")` and the tooltip/choice strings in `Params.h`
  macro table (displayed through `tr(info.tooltip)`); outputs `en.pot`-like `Resources/i18n/template.txt`; CI fails if a
  translated file lacks keys (warning) or has unused keys (warning), and **fails on format-specifier mismatch** (`%d`, `{}`).
* Param **names and choices shown to hosts stay English** (stable IDs/names for automation lanes); only the plug-in UI is localised.
* Languages: first wave `de, fr, es, pt-BR, ja, zh-Hans`; translators get `template.txt` + context column + screenshots. Machine
  translation only as draft; native review required before shipping.
* Layout: pseudo-locale `en-XA` (adds 40 % padding, accents, brackets) in the GUI test finds truncation; `Label`s use `minimumHorizontalScale`
  and tooltips carry full text. CJK: JUCE falls back to system fonts; Linux may lack CJK fonts → test and document.
* Number/date formats: use `juce::String` with `getLocale`-independent formatting for parameter values (dots), keep decimal point stable.
* Not in scope: right-to-left layout.
* Tests: `l10n.everyKeyHasEnglish`, `l10n.pseudoLocaleFitsAllLabels`, `l10n.formatSpecifiersMatch`, `l10n.languageSwitchRebuildsUI`.
* Acceptance: switching language rebuilds the UI without reopening the editor and persists in options; no label clipped in any shipped language at 100 % scale.
