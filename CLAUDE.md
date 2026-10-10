# Interference — working notes for developers and AI agents

JUCE 8.0.8 audio plug-in (VST3, CLAP, Standalone). C++17. Specs and plan: `Docs/specs/` (start at its README).
Rules that matter most: `Docs/specs/Standards.md`.

## Build and test
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release      # Windows: -G "Visual Studio 17 2022" -A x64
cmake --build build --parallel                                # ~7 min cold on 4 cores
export IFX_DATA_ROOT=$(mktemp -d)                             # keep test output out of ~/Documents
build/InterferenceTests_artefacts/Release/InterferenceTests [--filter text] [--slow] [--list]
xvfb-run -a build/InterferenceGuiTests_artefacts/Release/InterferenceGuiTests    # Linux needs a display
python3 tools/gen_param_doc.py                                # after any Params.h change
```
JUCE is cloned into `./JUCE` (git-ignored) or found via `-DINTERFERENCE_JUCE_DIR`. Linux deps: ALSA, freetype, fontconfig, X11 dev packages.
DSP-only work: build target `InterferenceTests` only.

## Where things live
`Source/Core` pure logic, no JUCE (params, pattern, gestures, environment) · `Source/DSP` engine, sources, Studio chain ·
`Source/Plugin` processor, state, presets, diagnostics, crash hook · `Source/UI` editor · `Source/Render` offline renderer ·
`Tests` console runners (own tiny framework) · `Docs` user + engineering docs.

## Non-negotiables
Audio thread: no alloc/locks/IO. Parameter IDs append-only. State loads must clamp and tolerate unknown fields. Defaults keep the old
sound. Every physical fault needs a cause (healthy, still, shielded = silent). Add tests with each change; keep warnings at zero.
One feature = one branch; do not reformat unrelated code; hotspot files are listed in `Docs/specs/DevelopmentPlan.md` §4.
