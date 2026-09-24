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
