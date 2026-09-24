# Third-party components

Interference contains no third-party audio, samples, images, fonts or measured hardware recordings. All
illustrations and the icon are drawn in code (`Source/UI/Illustrations.cpp`, `tools/make_icon.py`) by the author.
Fonts are the operating system's own UI fonts, selected at run time (not embedded).

| Component | Version | Licence used | Where |
|---|---|---|---|
| JUCE framework modules | 8.0.8 | JUCE 8 licence (commercial) **or** AGPLv3 — see release gate below | statically linked |
| Steinberg VST3 SDK (bundled in JUCE) | 3.7.x (as shipped with JUCE 8.0.8) | Steinberg VST3 Licence (proprietary option) or GPLv3 | statically linked into the VST3 |
| zlib (bundled in JUCE `juce_core`) | as shipped with JUCE | zlib licence | statically linked |
| libpng (bundled in JUCE `juce_graphics`) | as shipped with JUCE | libpng licence | statically linked |
| libjpeg (bundled in JUCE `juce_graphics`) | as shipped with JUCE | IJG licence | statically linked |
| FLAC / Ogg Vorbis (bundled in JUCE `juce_audio_formats`) | as shipped with JUCE | BSD-style | linked only if the format module pulls them in; WAV is the only format written |

Full licence texts ship with JUCE: `JUCE_8_0_8/LICENSE.md`,
`JUCE_8_0_8/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt` and the files it references.

## Credits and attribution

- Visual style: the project's own visual identity spec (`theme.md`, supplied by the publisher), covering colours,
  typography, control shapes and meters. No external brand styling is used.
- Research that informed the models is cited in `Docs/Research.md`. No text, figures or data from those papers are
  included in the product.
- Design, DSP, UI and documentation: Sheldon Davidson, with AI-assisted development (Claude, Anthropic).

## Commercial release gate (licensing)

Interference is proprietary (see `LICENSE`). Shipping it closed-source therefore requires, **before sale**:

1. A JUCE 8 commercial licence tier appropriate to the seller's revenue (JUCE 8 EULA), because the AGPLv3 option
   would require releasing the source.
2. A signed Steinberg VST3 licence agreement (the proprietary VST3 option), or a GPLv3 release.
3. "VST" is a trademark of Steinberg Media Technologies GmbH; follow Steinberg's logo/trademark guidelines on
   product pages.

None of these can be obtained or verified by the build; they are listed as open gates in `Docs/QA.md`.
