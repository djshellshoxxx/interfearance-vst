# Interference Wiki

Interference is a VST3 and stand-alone audio effect and sound source (Windows x64, version 1.0.0) built from faulty analog signal paths. It models four pieces of misbehaving hardware: a **Loose Jack**, an **Exposed Plug Touch**, **Cable Handling** and a **Dirty Pot**. Load one, or any two different ones, in slots A and B. Every fault has a cause (movement, wear, pressure, touch, mains hum, or an excitation you switched on), so a healthy, still, shielded circuit is silent.

**Who it is for:** sound designers and producers who want rhythmic or textural faults on a track, and developers who want to know how the models are built.

## Quick start

1. Install from the release ZIP (see [[Install]]) or build it (see [[Developer-Build-and-Test]]).
2. Insert Interference on a track. Click **Loose Jack** in the source row.
3. Set Move Plug to 90 to 100 percent and add a little Wiggle. Seat the plug fully and the faults stop.
4. For rhythm, switch to **Play**, turn on the Gesture Lane and start the host transport.

## Pages

**For users**
- [[User-Guide]]: every control, mode, source, lane, environment and render option
- [[Install]]: install, update, uninstall, file locations
- `Docs/SoundDesign.md`: 13 sound design recipes
- `Docs/Parameters.md`: every parameter (generated from `Source/Core/Params.h`)

**For developers**
- [[Developer-Overview]]: architecture, design decisions, signal flow
- [[Developer-Code-Map]]: where everything lives and where to edit what
- [[Developer-Build-and-Test]]: toolchain, build, tests, CI, release
- [[Developer-Conventions]]: style, real-time rules, gotchas, known limitations

**Repo docs (authoritative, longer)**
- `Docs/Manual.md`, `Docs/Parameters.md` (generated), `Docs/MidiMap.md`, `Docs/Routing.md`, `Docs/Install.md`, `Docs/QA.md`, `Docs/Issues.md`, `Docs/Decisions.md`, `Docs/Research.md`, `Docs/research/*`, `CHANGELOG.md`, `TODO.md`
