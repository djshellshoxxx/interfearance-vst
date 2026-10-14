# Development plan

Goal: ship every spec with few bugs and almost no merge conflicts. The two levers are **shared foundations built first**
(so features stop editing the same lines) and **a strict order/hotspot map** (so two branches never edit one file region).
Estimates are ideal working days for one experienced JUCE developer working with an AI assistant; double them for a
calendar view with review and DAW testing.

## 1. Waves and order

| Wave | Content | Why here |
|---|---|---|
| **0 — Foundations** (parallel, small PRs) | **E5 CI** first, then F0.1 `ifx-render` CLI · F0.2 params split · F0.3 `ParamPipeline` · F0.4 `SourceRegistry` · F0.5 flags / format / fixtures / templates · F0.6 `tr()` strings · F0.7 `SessionExtras` · F0.8 changelog fragments + per-feature test files | Nothing else can be verified without CI; the rest removes merge hotspots |
| **1 — Platform quality** | E6 crash reporting · E7 UI scale · U3 undo/redo · V1 preset library + browser · E8 calibration tooling | Independent of each other; E8 tooling only needs F0.1 |
| **2 — Workflow** | V2 randomise/mutate+locks · U4 meters/why-quiet · U2 macro screen · U5 preview · N5 preset codes · V3 morph | V2/V3/U2 all use F0.3; U3 must exist so every destructive action is undoable |
| **3 — Sound and play** | V4 new sources (one PR each, research-gated) · D3 light mode · N2 patterns · N1 Gremlin · N4 MIDI performance · N3 visual feedback | V4 needs F0.4; N1 needs F0.3; N3 shares the scope buffer with U4 |
| **4 — Reach** | D6 accessibility · D7 localisation · D4 oversampling (gated by spike) · V5 preset packs · D1 site/audio examples · D2 demo build · D5 AAX (blocked on Avid) | D6/D7 audit the finished UI; V5/D1 need the ifx-render CLI and final presets |

Release train: v0.0.1 beta (now, Windows) → v0.1 (waves 0–1) → v0.2 (wave 2) → v0.3 (wave 3) → 1.1 (wave 4). Each release
runs the full QA gate (Quality.md §6) and ships behind the flags in Standards §6.

## 2. Foundation tasks (precise)

| ID | Task | Files | Days | Unblocks |
|---|---|---|---|---|
| F0.1 | `ifx-render` console target: args `--preset f --input in.wav|--gen noise|click|pluck|drums --seconds n --rate r --out out.wav --metrics out.json --seed s`; wraps `OfflineRenderer`; metrics = peak, RMS, LUFS-ish, spectral centroid, event counts from `EngineTelemetry` | `Tools/ifx_render/main.cpp`, CMake target | 2 | E8, U5, D1, V5, golden tests |
| F0.2 | Split `IFX_PARAMS` into `IFX_PARAMS_V10` (existing text, moved verbatim) + one `.def` file per future feature; aggregator lists them in fixed order. Test: parameter ID list and defaults byte-identical to 1.0.0 (`Tests/fixtures/param_ids_1_0_0.txt`) | `Core/Params.h`, `Core/ParamsExt/*.def` | 1 | all param-adding features |
| F0.3 | `ParamPipeline` (Core, no JUCE): `effective = clamp(gremlin(macro(morph(base))))`; each stage `struct Stage { virtual void apply(ParamSnapshot&, const BlockInfo&) noexcept; }` registered in a fixed array; processor calls it once per block between `snapshotParams()` and engine. Also exposes `base` and `effective` so UI can draw both. Stages are no-ops (bit-exact) until their feature turns them on | `Core/ParamPipeline.h`, `PluginProcessor.cpp` (one call site) | 2 | V3, U2, N1, N4 |
| F0.4 | `SourceRegistry`: replace `kNumSourceTypes`, `sourceName/Cause`, `patternGesture`, `wearSettingsFor` switch statements and the per-source constructors in `Engine.h` with a descriptor table (`name, cause, gestures[3], make(), wear(), paramRange`). Behaviour-preserving; all 171 tests unchanged | `DSP/Source.h`, `DSP/Engine.h`, `UI/SlotPanel.cpp`, `UI/Illustrations.cpp` | 3 | V4 |
| F0.5 | `Core/Features.h` + CMake `IFX_FEATURE_*`, `.clang-format` (+ `git clang-format` CI step on changed lines), `Tests/fixtures/` + `loadsEveryFixtureIdentically`, `CLAUDE.md` (build/test commands, rules, hotspot map), `.github/pull_request_template.md`, CODEOWNERS | repo root | 1 | everything |
| F0.6 | `UI/Strings.h`: `juce::String tr(const char*)` wrapping `TRANS()` + `LocalisedStrings` loader; convert nothing yet, but *all new UI strings must use it*. Param tooltips/names displayed through `tr()` in `ParamKnob` only | `UI/Strings.h`, `UI/Widgets.cpp` | 1 | D7, every new UI |
| F0.7 | `SessionExtras` block in host state (separate `ValueTree` node, absent from presets): lock mask, morph on/off + amount, macro values/view mode, undo-independent UI prefs. Round-trip + missing-node tests | `Plugin/StateIO.*`, `PluginProcessor.cpp` | 1 | V2, V3, U2 |
| F0.8 | Changelog fragments `changelog.d/<id>.md` (assembled by `tools/assemble_changelog.py` at release) so CHANGELOG.md is never edited by features; split `TestGui.cpp` additions into `Tests/Feature<Id>.cpp` picked up by `file(GLOB CONFIGURE_DEPENDS Tests/Feature*.cpp)`; same for per-feature sources via `cmake/Feature_<id>.cmake` included by one line each | `CMakeLists.txt`, `tools/` | 1 | merge-free feature adds |

Foundation total ≈ 12 days. Merge them in the order E5 → F0.5 → F0.2 → F0.8 → F0.1/F0.6/F0.7 → F0.3 → F0.4.

## 3. Per-feature branch recipe

1. `git fetch && git checkout -b feat/<id>-<slug> origin/master`.
2. Write the failing tests named in the spec first (they double as the acceptance list).
3. Land the feature behind its flag; keep PRs ≤ ~800 lines by stacking: *data/DSP → state → UI → docs*.
4. Run the local gate (Quality.md §1) before every push; CI must pass before review.
5. Flip the flag in the last PR with the QA.md result rows.

## 4. Hotspot map (who may touch what, and how)

| File | Why hot | Rule |
|---|---|---|
| `Core/Params.h` | every param feature | only via `ParamsExt/*.def` + one aggregator line |
| `Plugin/PluginProcessor.cpp/.h` (901/193 lines) | undo, randomise, morph, session, preview, codes | new code lives in new classes (`UndoHistory`, `Randomizer`, `PresetLibrary`, `Auditioner`, `PresetCode`); the processor gets one member + one call per feature. Landing order: U3 → V2 → V3 → U5 → N5 |
| `UI/MainView.cpp/.h` | header buttons, tabs, overlays | add features as separate `Component`s mounted by a `HeaderExtras` strip / overlay; one `layout` line each. Landing order: V1 → U3 → V2 → U2 → U1 |
| `UI/Overlays.cpp` | each new dialog | every dialog in its own file `UI/<Name>Overlay.cpp`; `Overlays.h` gets one forward declaration |
| `DSP/Engine.h` (1176) | sources, light mode, oversampling | after F0.4, new sources add *no* lines here; D3/D4 land after V4 and are the only ones allowed to restructure it |
| `UI/HelpContent.cpp` | docs per feature | append a new section at the end of the file; do not reorder |
| `CMakeLists.txt` | new sources/targets | one `include(cmake/Feature_<id>.cmake)` line |
| `Docs/Manual.md`, `QA.md` | docs | append-only sections keyed by feature ID |
| `.github/workflows/ci.yml` | E5 owner only until E5 merged | afterwards one job per concern |

## 5. Parallel work lanes (if more than one developer / agent)

* Lane A (infra): E5 → F0.5 → F0.8 → E6 → E7 → D6/D7.
* Lane B (state/presets): F0.2 → F0.7 → U3 → V1 → V2 → N5 → U5 → V5.
* Lane C (engine): F0.4 → F0.3 → V3/U2/N1 → V4 sources → D3 → D4.
* Lane D (research/QA): E8 tooling → V4 research notes → calibration sessions → Quality.md gates.
Lanes only meet at the hotspot rules above.

## 6. Estimates (ideal days, including tests + docs)

| ID | Days | | ID | Days | | ID | Days |
|---|---|---|---|---|---|---|---|
| E5 CI | 2 | | V1 browser | 6 | | U1 tour | 3 |
| E6 crash | 4 | | V2 randomise | 4 | | U2 macro | 6 |
| E7 UI scale | 3 | | V3 morph | 5 | | U4 meters | 3 |
| E8 calibration | 10 (+ fieldwork) | | U3 undo | 4 | | N3 visuals | 5 |
| V4 sources | 8 each ×4 | | U5 preview | 3 | | D6 a11y | 4 |
| D3 light mode | 4 | | N5 codes | 3 | | D7 l10n | 6 |
| N1 Gremlin | 4 | | V5 packs | 10 (content) | | D1 site | 5 |
| N2 patterns | 4 | | N4 MIDI perf | 5 | | D2 demo | 3 |
| D4 oversampling | 6 (spike 2) | | D5 AAX | 3 (after SDK access) | | Foundations | 12 |

Total ≈ 150 days sequential; ≈ 60–70 calendar days with the four lanes.

## 7. Top risks and mitigations

| Risk | Mitigation |
|---|---|
| CI cannot run (account/billing) | E5 step 0 triage; no feature merges without a green run on both OS |
| A feature changes the sound of old projects | Fixtures + `loadsEveryFixtureIdentically`; params default to "off/neutral" |
| `PluginProcessor` becomes a god class | New logic in new classes; reviewers reject processor growth > 30 lines per feature |
| Real-time regressions | CPU gate in TestPerf, soak test, `-fsanitize=thread` Linux job, no-alloc test (hook `operator new` in test builds, assert zero allocations in `processBlock`) |
| Source refactor (F0.4) breaks DSP | Behaviour-preserving; golden-hash test of 20 scenarios before/after |
| Windows-only developer tooling | Everything scripted twice (PowerShell and bash) or Python |
| Scope creep in V4 / E8 | Research-gate and acceptance criteria are hard stops; unmet = feature stays behind flag |
