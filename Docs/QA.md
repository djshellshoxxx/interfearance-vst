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
| `checks` | ubuntu-latest | every push and pull request | `LICENSE` and `THIRD_PARTY.md` exist, and `THIRD_PARTY.md` names JUCE, VST3, HarfBuzz and SheenBidi. `python tools/gen_param_doc.py` must leave `Docs/Parameters.md` unchanged (line endings ignored). |
| `build` | windows-2022 (pinned: ships VS 2022) | every push and pull request | Shallow clone of JUCE 8.0.8 into `./JUCE` (cached). Configure with Visual Studio 2022 x64 with warnings as errors (`-DCMAKE_COMPILE_WARNING_AS_ERROR=ON`), build Release. Run `InterferenceTests.exe` and `InterferenceGuiTests.exe`; a non-zero exit fails the job. Upload the GUI snapshots and the built binaries. `tools/make_release.ps1` (without `-Build`) makes an **unsigned** ZIP (its `SIGNATURE.txt` says UNSIGNED), uploaded as `interference-win64-unsigned` with its `.sha256`. |
| `release` | windows-2022 | `v*` tags only, protected environment `release` | Takes the tested binaries from `build` (no rebuild), signs and verifies them when the environment holds a certificate, sets `IFX_SIGNED_BY` to the certificate subject for `SIGNATURE.txt`, runs `make_release.ps1` and uploads `interference-win64-release-<tag>`. |
| `slow` | windows-2022 | manual *Run workflow* only (input `slow`) | Builds the DSP test binary and runs `InterferenceTests.exe --slow`: the CPU benchmark and the 1 h offline soak. |

Signing happens only in the `release` job, only for `v*` tags, and only when the `release` environment (protected by
required reviewers) holds the secret `SIGNING_CERT_BASE64` (a base64 PFX, with optional `SIGNING_CERT_PASSWORD`).
It signs the VST3 binary and the stand-alone app with `signtool` and an RFC 3161 timestamp, then verifies both, before
the ZIP is made. Without the secret the tag is packaged unsigned with a notice, and `SIGNATURE.txt` says UNSIGNED.
Branch builds and pull requests are never signed. A signature is never created or faked.

CI does not run pluginval or load the plug-in in a DAW: those remain local release gates (see below).

## Results (2026-09-24, Release build, commit history on `master`)

### Automated tests

| Suite | Binary | Tests | Result |
|---|---|---|---|
| Core, sources (Loose Jack, Plug Touch, Cable Handling, Dirty Pot), Engine, Studio, Pattern, State, Presets, Render, Record, Environment, Limits, Scenarios, Rf | `InterferenceTests.exe` | 171 (3 slow skipped) | **pass**, 0 failed checks (realism rows also re-run at 44.1 kHz / 64 and 96 kHz / 1024) |
| Perf (slow): CPU benchmark, cost breakdown, 1 h offline soak | `InterferenceTests.exe --slow --filter Perf` | 3 | **pass** |
| Host contract + GUI (headless editor: buses, IDs, state, MIDI/learn, reset/randomize/A-B, resize × 5 tabs, overlays, presets × source pairs, concurrency, teardown during render, record, debug tools, secret pixel) | `InterferenceGuiTests.exe` | 17 | **pass**, 0 failed checks (adds Details / Environment fit at 1000×700, project reopen identity, Why-quiet and dormant flags, focus-chain titles and tooltips) |

What the suites assert (examples, not exhaustive): silence invariant for every source pair, mode and mix; exact
identity for bypass, empty/bypassed slots, 0 % wet and Studio off; `C = R + D + A`; block-size independence
(64 / 257 / 333 / 401 / 997 samples); locked-seed bounce, loop and seek repeatability; all sample rates 44.1–192 kHz
finite and bounded; NaN/Inf input sanitised; zero-length blocks; extreme-parameter fuzz; every realism-table row per
source; every PhysicalLimits bound; pair distinctness in both orders; tempo changes and 7/8, 5/4 bars; offline render
vs live engine; DC input and log sweeps; ceiling behaviour; declicked switches and smoothed automation; no pop without
a contact transition; ground-loop / dimmer / 2G phone / RF transmitter physics (odd harmonics, interface rejection,
216.7 Hz bursts, square law); WAV headers for every format; overwrite protection; bad paths; render cancellation.

### CPU and latency (Intel Core i5-2520M, 2 cores / 4 threads, 2.5 GHz, 2011 laptop)

Worst case = two slots + hum/buzz/RF + passive pickup + input drive + every Studio stage + feedback + ceiling +
Ghost Carrier + lane running, stereo, 256-sample blocks, under `ScopedNoDenormals` (as in `processBlock`):

| Sample rate | Share of one core | Real-time factor |
|---|---|---|
| 44.1 kHz | ~12 % | ~8× |
| 48 kHz | ~12 % | ~8.5× |
| 96 kHz | ~21–25 % | ~4.0–4.7× |
| 192 kHz | ~39–55 % (varies run to run) | ~1.8–2.6× |

Cost breakdown at 48 kHz (quiet machine): empty slots ≈ 1.6 %, one source ≈ 2.9 %, two sources ≈ 3.8 %, + Studio
EQ/Contour ≈ 6.9 %, everything ≈ 9 % of one core. Timings vary ±30 % with background load on this machine.
Gate (slow test): under 80 % of one core at every rate and 192 kHz ≤ 6× the 48 kHz cost; final build: pass.
Latency: **0 samples** in every configuration (ADAA instead of oversampling, no lookahead).

### Soak (offline)

60 minutes of 48 kHz stereo audio, worst case, with continuous automation (Wiggle, Bend, Dry/Wet, Drive, Resonator
and Ghost frequency every second, Kill Feedback every 97 s): processed in 437 s; 5-minute segments took 33–48 s with
no upward drift; output finite, peak −5.6 dBFS. **Pass.** The engine allocates only in `prepare()`, so memory is
constant by construction; no separate memory profiler was run.

### Validation, packaging, install

- **pluginval** (Tracktion), strictness level **10**, in-process, on the final Release VST3: **pass** (exit code 0,
  25 test groups: scan, cold/warm open, info, programs, editor, editor while processing, audio processing,
  non-releasing processing, state and state restoration, automation, editor automation, automatable parameters,
  parameters, background-thread state, parameter thread safety, VST3 validator, buses, fuzz parameters). Log:
  `build/pluginval/pluginval_run.txt`. Command: see *How to reproduce*.
- **Release ZIP**: `dist/Interference-1.0.0-win64.zip` (29 files, about 6.2 MB). Its SHA-256 is written next to it in
  `Interference-1.0.0-win64.zip.sha256` (this file is inside the ZIP, so it cannot quote its own hash). Built twice
  from the same commit with Windows PowerShell 5.1: identical hash. Contents: VST3 bundle, stand-alone app, README, CHANGELOG, LICENSE, THIRD_PARTY, all user docs,
  the electrical-interference research note, `ThirdPartyLicenses/` (JUCE, HarfBuzz, SheenBidi, libpng, IJG, zlib,
  VST3 SDK) and `SIGNATURE.txt` (UNSIGNED build).
- **Build**: Release, all targets, 0 errors, 0 warnings (MSVC 14.44 with JUCE's recommended warning flags).
- **Installed** to `C:\Program Files\Common Files\VST3\Interference.vst3` on this machine (FL Studio 2025 scans it);
  the installed binary is byte-identical to the build.

## Unverified release gates

These could not be exercised unattended on this machine and are **not** claimed:

1. **DAW load.** FL Studio 2025 is installed, but loading the plug-in, automating it, saving and reopening a project
   and bouncing need a person at the keyboard. Steps: rescan plug-ins (Options → Manage plugins → Find plugins),
   insert Interference on a mixer track, load *Ungrounded Input* and *Breathing Ground Loop*, move knobs, automate
   Dry/Wet, save, reopen, render; confirm identical audio with Lock on.
2. **Real-time host soak** (1 h in a DAW with the editor open) and host-side memory check.
3. **Sidechain in a real host** (the headless host test covers the bus layout only).
4. **Empirical calibration** against recordings of real faulty hardware (see `Research.md`): required before any
   claim of measured realism.
5. **Publisher items:** real homepage and support e-mail (placeholders), JUCE commercial licence and Steinberg VST3
   licence agreement (see `THIRD_PARTY.md`), code signing certificate.
6. **Sanitizer build** (`-DINTERFERENCE_ASAN=ON`) was not run in this session (4 GB RAM machine).
7. **macOS / AU / Linux / CLAP**: not built.
8. **Low disk space** while rendering or recording: the write-failure paths exist (bad path and cancel are tested;
   a full disk reports the writer error and drops samples during Record, counted in the popup) but a full disk was not
   simulated.
