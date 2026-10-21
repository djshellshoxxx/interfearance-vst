# Developer Guide

## Build

JUCE 8.0.8 is pinned. CMake looks for it in this order: `-DINTERFERENCE_JUCE_DIR=<path>`, `./JUCE`,
`../JUCE_8_0_8`, otherwise FetchContent at the pinned tag. JUCE is not vendored (about 100 MB).

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build build --config Release --parallel
```

Options include `INTERFERENCE_ASAN` (MSVC AddressSanitizer). Full build steps: README "Building from source".

## Continuous integration

`.github/workflows/ci.yml` builds on Windows (VS 2022, x64, Release) and runs the checks in the later steps.
This documentation pass did not build or run the project (Windows only).

## Rules from `DECISIONS.md`

- Zero latency: first-order ADAA instead of oversampling for drive and preamp saturation; the Ceiling has no lookahead.
- Source settings are stored per source type, not per slot, so moving or bypassing a source keeps its settings.
- Determinism: RNG sub-streams are re-keyed from (seed, source, slot, bar index) at each bar while the transport plays.
- Check `PROGRESS.md` and `Docs/QA.md` before a release.
