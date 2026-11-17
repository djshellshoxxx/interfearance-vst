# Developer Overview

## What the code is

A JUCE 8.0.8 plug-in (C++17). One `InterferenceProcessor` (VST3 and stand-alone, AU only if built on Apple). The DSP is header-only in most places and never depends on the GUI. Built with CMake 3.22+, pinned to JUCE 8.0.8 (fetched, or found at `INTERFERENCE_JUCE_DIR`, `./JUCE` or `../JUCE_8_0_8`).

Identity: product `Interference`, plugin code `Ifx1`, manufacturer code `Ifrn`, version `1.0.0` (`project(Interference VERSION 1.0.0)`).

## Tech stack

- C++17, JUCE 8.0.8 modules (audio processors, GUI, DSP, formats)
- AudioProcessorValueTreeState (APVTS) for host parameters, built from the fixed table in `Source/Core/Params.h` (151 automatable parameters, `enum P`)
- Own DSP: sources, engine, filters, Studio chain, environment, gesture generator
- Third-party notices: `THIRD_PARTY.md` (JUCE, VST3 SDK, HarfBuzz, SheenBidi)
- CMake, GitHub Actions on Windows 2022

## Design decisions (and why)

These are recorded in more detail in `DECISIONS.md` and `Docs/Decisions.md`.

**Physical causes, not noise sliders.** Every fault is caused by something: movement, wear, pressure, touch, mains, or an excitation you turn on. A still, shielded, healthy circuit is silent by design. The controls are named after what you do to the hardware (Move Plug, Shake Cable, Turn Knob).

**Four source models in one engine.** `SourceType` has None, LooseJack, PlugTouch, CableHandling and DirtyPot (`Source/DSP/Source.h`). Each source has its own state and models. Two slots run in series through `Engine`.

**Shared environment, one mains phase.** `Environment` (`Source/Core/Environment.h`) holds the hum, buzz, ground loop, RF and 2G phone models for the whole rig. One mains phase is shared by both slots, so hum is coherent.

**Determinism by position, not by buffer.** Random decisions are derived from Seed, Take, source, slot and the absolute bar position (`reseedAll`, `barKey`). With Lock on, the same host position gives the same audio for any buffer size. This is why bounces match.

**Causal and clamped.** Feedback is bounded and clamped to stay stable. Ceiling is a causal limiter with a fixed release. Excessive requests are clamped with a visible reason, and the stored value is not changed.

**Zero latency.** Latency is 0 samples by design. Studio and gesture timing use the host position.

**Offline is cloned.** `Render/OfflineRenderer` renders with a cloned engine, not the live one, so the render does not disturb playback.

## Signal flow

```
 host in (mono or stereo, optional sidechain)
    │
    ├──► dry ─────────────────────────────────────────────┐
    │                                                      │
    ▼                                                      │
 Engine (buses R / C0 / C, taps, mix)                      │
    │  Slot order A->B or B->A (Swap Order)               │
    ├─► Slot A source ─┐                                   │
    │   (LooseJack /   ├─► series, open contacts          │
    │    PlugTouch /   │   interrupt, pots filter          │
    │    CableHandling │                                   │
    │    / DirtyPot)   │                                   │
    ├─► Slot B source ─┘                                   │
    │      ▲                                                │
    │      │ Gesture lanes A and B ──► gesture starts       │
    │      │ MIDI, Play buttons, illustrations, automation  │
    │      │ Environment: mains, buzz, ground loop, RF, 2G  │
    ▼                                                      │
 Studio chain (post hardware, labelled Studio)              │
    EQ > Contour > Drive > Crush > Resonator > Gate > Echo  │
    │                                                      │
    ▼                                                      │
 Monitor / Mix / Dry trim  ◄───────────────────────────────┘
    ▼
 Ceiling (causal limiter) ─► Output
```

`processBlock()` (`Source/Plugin/PluginProcessor.cpp`, starts at line 260) runs the engine for the block, then applies output stage, meters and telemetry.

## Threads

- **Audio thread**: `processBlock()` and `Engine::process`. README states the rule: no allocation, no locks, no logging, no file IO.
- **Message thread**: the UI, presets, file dialogs, render and record requests. It sends control events through `pushControlEvent` (noexcept, written into the `uiFifo` lock-free FIFO).
- **Background**: the Record buffer is written by a background thread. Activity history is kept as lines that the UI reads with `pendingActivity(cursor)`.

## Data

- `ParamSnapshot`: a block's read-only copy of parameters
- `ControlEvent`, `LaneEvent`, `TransportInfo`: events from the UI or host
- `Pattern`, `GestureGenerator`, `Gesture.h`: lane and gesture scheduling
- `ContactModel`, `WearMap`, `MotionDriver`, `PhysicalLimits`: physical state
- `EngineTelemetry`: meters and state read by the UI through `telemetry()`
- `PluginState` (in `StateIO`): what presets and host state store

## Where the UI fits

`Source/UI` holds `MainView`, `SlotPanel`, `StudioPanel`, `LaneEditor`, `Overlays`, `Widgets`, `Illustrations` and `Theme`. The editor sets parameters and sends control events. It never touches DSP state directly.

See [[Developer-Code-Map]] for file and function locations, and [[Developer-Conventions]] for the rules.
