# Architecture and Design Rationale

Sources: `DECISIONS.md`, `README.md`, `Docs/Research.md`, `Docs/research/`. *(inferred)* marks conclusions from the layout.

- **Physical fault models, not measurements.** The models are research-informed and do not measure particular devices (README).
- **Counterfactual buses.** Each source's circuit runs three times per sample (R, C0, C) with the contact state computed once, so `C = R + D + A` is exact by construction (`DECISIONS.md`).
- **Zero latency by design.** ADAA instead of oversampling, and no lookahead in the Ceiling, so reported latency is 0 and parallel buses stay aligned (`DECISIONS.md`).
- **Per-source-type settings.** Keeps a source's settings when it moves between slots or is bypassed.
- **Deterministic randomness.** RNG streams are keyed by seed, source, slot and bar, so a transport seek or loop jump is reproducible.
- **Single Gesture Lane.** The Machine Pattern lane (1 to 64 steps, per-slot A/B) is the one lane editor.
- **Layering.** `Source/Core` (parameters, randomness, physical limits, wear, patterns), `Source/DSP` (circuit models), `Source/Render`, `Source/Plugin` (JUCE processor, diagnostics, crash hook) and `Source/UI`. *(inferred from folder names.)*
