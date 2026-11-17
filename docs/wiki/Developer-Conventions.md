# Developer Conventions

## Style

- C++17. Follow the nearby code and the JUCE guidelines in `JUCE_CLAUDE_GUIDELINES.md`.
- Headers use `#pragma once`.
- The spec files in the root (`INTERFERENCE_VST_*.md`, `JUCE_CLAUDE_GUIDELINES.md`, `include.md`, `theme.md`) are the project's rules. Read them before changing behaviour.
- Parameter IDs are fixed. Append new ones, never renumber or rename a shipped one. Presets and host automation depend on it.

## Real-time rules

From the README and `JUCE_CLAUDE_GUIDELINES.md`:

- The audio thread never allocates, locks, logs or does file IO. Keep it that way in `processBlock()` and `Engine::process`.
- UI to audio events go through `pushControlEvent`, a lock-free FIFO. Do not replace it with a mutex.
- Audio to UI state is read through telemetry and atomics. Do not call into UI code from the audio thread.
- Recording uses a lock-free buffer. A background thread writes the file.
- Rendering uses a cloned engine so it never disturbs live playback.
- Keep the process path free of `juce::String` building. Activity log lines are built on the message thread.

## Determinism

- Random decisions come from Seed, Take, source, slot and bar position (see `reseedAll` in `Engine.h`). Do not add a random source that depends on buffer size or wall-clock time. Lock must keep bounces identical.
- A different sample rate can change the waveform. Do not promise bit-identical audio across rates.
- The `Natural` caps in `PhysicalLimits.h` define what counts as physical. Expressive can go further, but must stay causal.

## Gotchas

- **Feedback** is bounded in the Studio chain and clamped with a visible reason. Keep the clamp when you change the loop.
- **Ceiling** is causal, with about 45 ms release. Do not make it look-ahead without updating the latency promise (0 samples).
- **Raw Hardware** must be exactly the modelled hardware. Anything you add after the hardware must go in the Studio path, not in the model.
- **Bypass is exact** for both the plug-in and a single slot. Test any change to the slot path with `Tests/TestEngine.cpp`.
- **Preset migrations.** Older states load safely: missing values default, unknown values ignored, bad values repaired. A new field needs the same handling in `StateIO.cpp`.
- **Environment defaults are quiet.** Changes to default hum, buzz or RF levels change existing presets, so treat them as breaking.
- **The Docs/ folder and the docs/ folder are different.** Both exist in this repo. `Docs/` is the product documentation. `docs/` holds the shared standard (`docs/standards/CDL_PLUGIN_BASELINE.md`) and this wiki (`docs/wiki/`). On a case-insensitive file system these two folders collide, so check before adding files.

## How to add a feature

1. Read the spec files and `DECISIONS.md`. Record a new decision there if you make a judgment call.
2. Add the parameter to `Source/Core/Params.h`, then run `python tools/gen_param_doc.py`.
3. Put the model in the DSP (`Source/DSP` or `Source/Core`). Keep it free of UI includes.
4. Add the control in `Source/UI` and the help text in `HelpContent.cpp`.
5. Add tests in `Tests/`. Cover the offline DSP, state round trip and GUI if relevant.
6. Update `Docs/Manual.md`, `Docs/MidiMap.md` if MIDI changed, `CHANGELOG.md`, and this wiki.
7. Open a PR. CI must pass with warnings as errors.

## Known limitations and open items

From `Docs/Issues.md`, `Docs/QA.md` and `TODO.md`:

- macOS and AU are not built or tested for 1.0. The DSP core is portable.
- DAW loading (FL Studio and others), a real-time host soak, sidechain in a host and empirical calibration are unverified release gates.
- Models are research-informed and not measurements of particular devices.
- Code signing needs a certificate. Without one, the release is marked UNSIGNED.
- The homepage and support email are placeholders until the publisher sets them.

See [[Developer-Code-Map]] for where to make changes.
