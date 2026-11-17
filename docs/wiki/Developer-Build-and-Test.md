# Developer Build and Test

## Toolchain

- Visual Studio 2022 (or Build Tools) with the x64 C++ toolset, the primary and tested platform
- CMake 3.22 or newer
- JUCE 8.0.8. Resolved in this order: `-DINTERFERENCE_JUCE_DIR=<path>`, then `./JUCE`, then `../JUCE_8_0_8`. If none exist, CMake downloads tag 8.0.8 with `FetchContent` (shallow)
- Apple builds add an AU target (`if(APPLE)` in `CMakeLists.txt`). Not built or tested for 1.0

## Build

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Artifacts:

- `build\Interference_artefacts\Release\VST3\Interference.vst3`
- `build\Interference_artefacts\Release\Standalone\Interference.exe`

The build is set to warnings as errors in CI.

## Tests

```
build\InterferenceTests_artefacts\Release\InterferenceTests.exe        offline DSP, state and render tests
build\InterferenceTests_artefacts\Release\InterferenceTests.exe --slow  adds CPU benchmark and 1 h offline soak
build\InterferenceGuiTests_artefacts\Release\InterferenceGuiTests.exe  host contract and GUI tests
```

Set `IFX_DATA_ROOT` to an absolute folder to redirect presets, options and support files during tests or portable use.

Test sources are in `Tests/`. Each `Test*.cpp` covers one area (core, engine, environment, pattern, limits, sources, studio, state, render, RF, scenarios, performance) plus GUI tests in `TestGui.cpp`. `Docs/QA.md` lists what has been run and what is still an unverified release gate. pluginval and DAW loading are not part of CI.

## Release packaging

```
powershell -ExecutionPolicy Bypass -File tools\make_release.ps1
```

Writes the release ZIP into `dist\` with a `.sha256` and a `SIGNATURE.txt`.

## Parameter docs

`Docs/Parameters.md` is generated from `Source/Core/Params.h`. After changing parameters run:

```
python tools/gen_param_doc.py
```

CI fails if the committed file does not match the generator output.

## CI (`.github/workflows/ci.yml`)

Runs on every push and pull request.

- **checks** (Ubuntu): licence notices (`LICENSE` and `THIRD_PARTY.md`, which must name JUCE, VST3, HarfBuzz and SheenBidi), and the `Docs/Parameters.md` consistency check.
- **build** (Windows, `windows-2022`, VS 2022 x64): JUCE 8.0.8 (cached), Release build with warnings as errors, both test suites, then an unsigned `make_release.ps1` ZIP. Uploads the ZIP, its `.sha256`, the binaries and the GUI snapshots as artifacts.
- **release** (`v*` tags only, protected environment `release`): signs the tested binaries if the `SIGNING_CERT_BASE64` secret is set (optional `SIGNING_CERT_PASSWORD`), then packages. `SIGNATURE.txt` says who signed, or UNSIGNED.
- **slow** (manual *Run workflow* only): `InterferenceTests.exe --slow`.

Signing happens only for `v*` tags and only with the certificate secret. Never fake a signature.

## Release process

1. Confirm CI is green and run the slow job if performance matters.
2. Work through the unverified release gates in `Docs/QA.md` (DAW load, host soak, licences, signing).
3. Update `CHANGELOG.md`.
4. Tag `v<version>`. The `release` job builds, signs (if the certificate secret is set) and publishes the packaged ZIP.

## Wiki sync

Pages in `docs/wiki/` are mirrored to the GitHub wiki by `.github/workflows/wiki-sync.yml` on pushes to `main` that touch that folder, or on manual dispatch. If the wiki repo does not exist yet, the job posts a notice and exits without failing. Create it once from the Wiki tab.

## Troubleshooting builds

- Configure cannot find JUCE: set `-DINTERFERENCE_JUCE_DIR=...` or place JUCE next to the repo.
- Build fails on warnings: CI treats warnings as errors. Fix the warning, do not suppress it globally.
- Output differs between sample rates: expected. Determinism is guaranteed for the same sample rate and version only (see [[User-Guide]]).
