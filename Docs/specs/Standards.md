# Standards — apply to every change

## 1. Real-time rules (audio thread)
No allocation, locks, file/network I/O, `juce::String`, exceptions or logging in `processBlock` or anything it calls.
Message→audio hand-off uses the existing patterns: `AbstractFifo` (`uiFifo`, `ccFifo`), seqlock for `Pattern`, atomics.
New hand-offs reuse one of these; do not invent a fourth. Every new DSP block: bounded output for any finite input,
denormal-safe, SR-independent timing, bit-exact identity when "off", 10 ms declick on switches (Decisions.md).

## 2. Parameters — append-only
* Host parameter IDs (`id` string in `Core/Params.h`) are never renamed, removed or re-typed. Choice lists only get
  entries **appended**; ranges may widen, never narrow. State stores params by ID string (`StateIO::toValueTree`), so
  the enum order is not persisted and may be re-grouped safely.
* A feature that needs parameters adds them in its **own macro block** (see Plan, task F0.2): `IFX_PARAMS_<FEATURE>(X)`
  in `Core/ParamsExt/<feature>.def`, listed once in the `IFX_PARAMS` aggregator. Defaults must reproduce the old sound.
* After any parameter change run `python tools/gen_param_doc.py` and commit `Docs/Parameters.md` (CI checks it).
* Non-parameter state goes through `PluginState` + `StateIO`; fields are additive (unknown ignored, missing = default,
  every value clamped). Bump `PluginState::kVersion` only if an old reader would load *wrong* (not merely partial) data.
* Every new parameter/state field needs: a `TestState` round-trip row, a "load a 1.0.0 file" row, a `TestGui` parameter
  count update (`ids.size() == kNumParams` is automatic), a Manual/MidiMap entry.

## 3. Compatibility fixtures
`Tests/fixtures/` holds one `.ifxpreset` and one host-state blob per released version (add v0.0.1 as soon as it ships).
`TestState.loadsEveryFixtureIdentically` renders 2 s from each fixture and compares to a stored hash. A change that moves
a hash needs an explicit CHANGELOG line ("sound changed") and a new fixture generation.

## 4. Code style
Match the surrounding code (JUCE style: space before `(`, braces on new lines, `ifx` namespace, `Core` has no JUCE
dependency). `clang-format` config to be added in F0.4 and enforced only on changed lines (`git clang-format`) so
existing files are not reformatted (avoids merge noise). Every new file carries the ownership header used in `Source/`.
Comments explain *why* and physical/engineering rationale, never restate code.

## 5. Merge-safety protocol
1. **One feature = one branch = one PR**, branched from latest `master`, named `feat/<id>-<slug>` (e.g. `feat/v2-randomise`).
2. **Foundations first** (Plan wave 0/1) so features never edit the same shared lines. Hotspot files (Plan §4) are edited
   only through the single-line extension points the foundations create.
3. Rebase onto `master` daily while a branch is open; PRs larger than ~800 changed lines (excluding tests/docs/generated)
   must be split. Long-running branches are not allowed: ship behind a feature flag instead (§6).
4. Generated files (`Docs/Parameters.md`) are never hand-merged: on conflict take `master`'s and regenerate.
5. Only one open PR may touch a given hotspot file at a time (claim it in the PR description "Hotspots: …"); others
   wait or stack on top.
6. Squash-merge, conventional commit titles (`feat(v2): …`), CI green + one review required.

## 6. Feature flags
`Source/Core/Features.h`: `constexpr bool kFeature<Id> = false;` toggled at compile time (CMake `-DIFX_FEATURE_<ID>=ON`
→ `target_compile_definitions`). Unfinished work merges dark; the flag flips in the PR that completes acceptance.
No runtime remote flags (offline product, Privacy.md).

## 7. Definition of done (each feature)
- [ ] Spec acceptance criteria all met, each mapped to a named test
- [ ] Unit tests (Core/DSP/State) + GUI test (TestGui) where UI exists; `InterferenceTests` and `InterferenceGuiTests` green
- [ ] CPU: `TestPerf` worst case still < 80 % of one core at all rates; feature's own cost measured and recorded in QA.md
- [ ] Soak: 10-minute randomised-parameter test (TestScenarios pattern) with the feature on, no NaN/Inf/denormal growth
- [ ] State: round trip, old-file load, host save/reopen (headless host test)
- [ ] Docs: Manual section, Parameters.md regenerated, Decisions.md entry for each non-obvious choice, CHANGELOG line
- [ ] Accessibility: every new control has title/description, is keyboard reachable, appears in `setFocusOrder`
- [ ] No new compiler warnings (CI uses warnings-as-errors)
- [ ] Windows MSVC, Linux GCC (and later macOS) build green

## 8. Review checklist (copy into PR)
Audio thread safe · state compatible · parameter append-only · defaults preserve old sound · tests added and named in the
PR · docs updated · hotspots declared · flag state correct · no unrelated reformatting.
