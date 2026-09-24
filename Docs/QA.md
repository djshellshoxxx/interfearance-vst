# QA evidence — Interference 1.0.0

Everything below was run on the build machine described under *Environment*. Anything that could not be run is listed
under **Unverified release gates**. It is not claimed.

## Environment

- Windows 10 Pro 22H2 (10.0.19045), x64, 4 logical CPUs, 4 GB RAM.
- MSVC 2022 Build Tools 14.44 (x64), CMake ≥ 3.22, JUCE 8.0.8, C++17, static runtime.
- No DAW is installed on this machine.

## How to reproduce

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\InterferenceTests_artefacts\Release\InterferenceTests.exe            # fast DSP / state / render suite
build\InterferenceTests_artefacts\Release\InterferenceTests.exe --slow     # + CPU benchmark and 1 h offline soak
build\InterferenceGuiTests_artefacts\Release\InterferenceGuiTests.exe      # host contract + GUI (headless editor)
pluginval.exe --strictness-level 10 --validate-in-process --output-dir build\pluginval ^
    --validate build\Interference_artefacts\Release\VST3\Interference.vst3
powershell -ExecutionPolicy Bypass -File tools\make_release.ps1            # dist\Interference-1.0.0-win64.zip + .sha256
```

Each test runner prints a line per test and ends with `RESULT: ALL TESTS PASSED` and exit code 0, or lists the
failures and exits non-zero. `--filter <text>` runs a subset. `--list` lists every test.

Sanitizer build (optional, slower): `cmake -S . -B build-asan -DINTERFERENCE_ASAN=ON`, then build the Debug
configuration and run both test binaries.

Set `IFX_DATA_ROOT` to an absolute folder to keep the presets, options and support files the tests write out of your
Documents folder (CI does this).

### Continuous integration

`.github/workflows/ci.yml` runs the same steps on GitHub Actions:

| Job | Runner | When | What |
|---|---|---|---|
| `checks` | ubuntu-latest | every push and pull request | `LICENSE` and `THIRD_PARTY.md` exist, and `THIRD_PARTY.md` names JUCE and VST3. `python tools/gen_param_doc.py` must leave `Docs/Parameters.md` unchanged (line endings ignored). |
| `build` | windows-latest | every push and pull request | Shallow clone of JUCE 8.0.8 into `./JUCE` (cached). Configure with Visual Studio 2022 x64, build Release. Run `InterferenceTests.exe` and `InterferenceGuiTests.exe`; a non-zero exit fails the job. Upload the GUI snapshots. Optional signing. `tools/make_release.ps1` (without `-Build`), then upload `dist/*.zip` and `dist/*.sha256`. |
| `slow` | windows-latest | manual *Run workflow* only (input `slow`) | Builds the DSP test binary and runs `InterferenceTests.exe --slow`: the CPU benchmark and the 1 h offline soak. |

Signing runs only when the repository secret `SIGNING_CERT_BASE64` (a base64 PFX, with optional
`SIGNING_CERT_PASSWORD`) is set. It signs the VST3 binary and the stand-alone app with `signtool` and an RFC 3161
timestamp, then verifies both, before the ZIP is made. Without the secret the job prints a notice that the artefacts
are **unsigned**. It never creates or fakes a signature.

CI does not run pluginval or load the plug-in in a DAW: those remain local release gates (see below).
