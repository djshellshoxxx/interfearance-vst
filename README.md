# Interference

Interference is an audio effect and playable sound source built from **faulty analog signal paths**. It models four
kinds of misbehaving hardware: a **Loose Jack**, an **Exposed Plug Touch**, **Cable Handling** and a **Dirty Pot**.
Load one, or any two different ones, in slots A and B. Every fault has a cause (movement, wear, pressure, touch,
mains hum, or an excitation you switched on), so a healthy, still, shielded circuit is silent. The models are
research-informed, not measurements of particular devices.

- **Hardware**: an insert effect with subtle hum, resistive loss, scratch or an occasional fault.
- **Play**: MIDI, GUI gestures, automation or the 1–64 step Gesture Lane disturb the hardware in time.
- **Create**: internal excitation makes one-shots and loops on a silent track, and renders them to WAV.
- **Environment**: the rig's surroundings, shared by both slots: mains hum, supply or light-dimmer buzz, radio
  interference, a ground loop through the cable shield with the receiving interface, and a nearby 2G phone.

157 automatable parameters, 27 factory presets in 7 categories, a Studio chain for post-processing clearly labelled
as such, and offline WAV render.

Version 0.0.1 beta · VST3 + CLAP + stand-alone · Windows x64 · 0 samples latency · mono and stereo · 44.1–192 kHz.

## Documentation

| Document | Contents |
|---|---|
| [Docs/Manual.md](Docs/Manual.md) | User manual: quick start per mode, sources, two slots, environment, lane, seed, Studio, render, presets, levels |
| [Docs/SoundDesign.md](Docs/SoundDesign.md) | 13 sound-design recipes: ground loops, dimmer wah, phone bursts, radio breakup, pops, grit |
| [Docs/Install.md](Docs/Install.md) | Install, update, uninstall, file locations, troubleshooting |
| [Docs/Routing.md](Docs/Routing.md) | Signal graph, named buses and monitor taps |
| [Docs/MidiMap.md](Docs/MidiMap.md) | MIDI notes, controllers, MIDI learn, automation |
| [Docs/Parameters.md](Docs/Parameters.md) | Every parameter: ID, range, default, description |
| [Docs/Research.md](Docs/Research.md) | Research basis and its limits; calibration status |
| [Docs/research/ElectricalInterference.md](Docs/research/ElectricalInterference.md) | Research behind the Environment models (hum coupling, ground loops, dimmers, RF, 2G) |
| [Docs/QA.md](Docs/QA.md) | Tests, validation commands and results, unverified release gates, support matrix |
| [Docs/Issues.md](Docs/Issues.md) | Known issues and fixed-issue log |
| [Docs/Decisions.md](Docs/Decisions.md) | Engineering decisions and assumptions |
| [Docs/Privacy.md](Docs/Privacy.md) | Privacy statement: offline, no telemetry |
| [CHANGELOG.md](CHANGELOG.md) | Versions, release notes, compatibility policy |
| [LICENSE](LICENSE), [THIRD_PARTY.md](THIRD_PARTY.md) | Licence and third-party notices |

## Building from source

Requirements: CMake ≥ 3.22, Visual Studio 2022 (or Build Tools) with the x64 C++ toolset, and **JUCE 8.0.8**. JUCE is
found in this order: `-DINTERFERENCE_JUCE_DIR=<path>`, then `./JUCE`, then `../JUCE_8_0_8`. If none of those exist,
CMake downloads tag 8.0.8.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\InterferenceTests_artefacts\Release\InterferenceTests.exe        # offline DSP / state / render tests
build\InterferenceGuiTests_artefacts\Release\InterferenceGuiTests.exe  # host contract + GUI tests
powershell -ExecutionPolicy Bypass -File tools\make_release.ps1         # release ZIP in dist\
```

`InterferenceTests.exe --slow` adds the CPU benchmark and the 1 h offline soak. Set `IFX_DATA_ROOT` to an absolute
folder to redirect the presets, options and support files (tests, portable use).

Artifacts:

- `build\Interference_artefacts\Release\VST3\Interference.vst3`
- `build\Interference_artefacts\Release\Standalone\Interference.exe`

## Source layout

| Folder | Role |
|---|---|
| `Source/Core` | Sample-independent state and physical event models: contact, wear map, motion, gestures, pattern, RNG, parameter table |
| `Source/DSP` | The four sources, Engine (buses R/C0/C, taps, mix), Studio chain, filters, telemetry |
| `Source/Plugin` | JUCE processor, state and presets, factory presets, diagnostics |
| `Source/Render` | Offline renderer and live recorder |
| `Source/UI` | Theme, widgets, illustrations, slots, lane editor, Studio/routing panels, overlays, help |
| `Tests` | Offline DSP tests and headless host/GUI tests |
| `tools` | Parameter-doc generator, icon generator, release script |
| `.github/workflows` | Continuous integration (see below) |

The DSP never depends on the GUI. The audio thread never allocates, locks, logs or does file IO.

## Continuous integration

`.github/workflows/ci.yml` (GitHub Actions) runs on every push and pull request:

- **checks** (Ubuntu): licence notices (`LICENSE`, and `THIRD_PARTY.md` naming JUCE, VST3, HarfBuzz and SheenBidi),
  and `Docs/Parameters.md` must match `python tools/gen_param_doc.py`.
- **build** (Windows, pinned `windows-2022`, VS 2022 x64): JUCE 8.0.8 (cached), Release build with warnings as errors,
  both test suites (any failure fails the job), then an **unsigned** `tools/make_release.ps1` ZIP. The ZIP, its
  `.sha256`, the built binaries and the GUI snapshots are uploaded as artifacts.
- **release** (Windows, `v*` tags only, protected environment `release`): signs the tested binaries from **build**
  when a certificate is configured, then packages the release ZIP. `SIGNATURE.txt` in the ZIP names the signer, or
  says UNSIGNED.
- **slow** (Windows, manual *Run workflow* only): `InterferenceTests.exe --slow`.

Signing happens only for `v*` tags, in the `release` environment, and only when it holds the secret
`SIGNING_CERT_BASE64` (base64 PFX, optional `SIGNING_CERT_PASSWORD`). A signature is never faked. pluginval and DAW
loading are not part of CI (see `Docs/QA.md`).


## Required shared plug-in standard

This project follows the [Circuit Drift Labs Shared Audio Plugin Standard](docs/standards/CDL_PLUGIN_BASELINE.md). It is required for the plug-in target; standalone-only requirements apply only when a standalone target is included. The product-specific specification supplements the shared standard and records the applicable profiles, compliance status, and any exceptions.
