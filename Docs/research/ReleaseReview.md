# Release-readiness review: packaging, CI, install docs and licences

Reviewer: read-only review agent, 2026-09-24.

Scope:
- `tools/make_release.ps1`
- `.github/workflows/ci.yml`
- `Docs/Install.md`, checked against the code and against the JUCE 8.0.8 sources
- `LICENSE` and `THIRD_PARTY.md`, checked against what JUCE 8.0.8 statically links

Severity:
- **Blocker** = must be fixed before the ZIP is distributed (licence compliance or data loss).
- **Major** = a user-visible defect or a broken claim.
- **Minor** = everything else.

---

## Blockers

### B1. Third-party notices are incomplete, and none of the licence texts are in the ZIP (THIRD_PARTY.md, make_release.ps1:61-68)

The table in THIRD_PARTY.md misses two components that JUCE 8.0.8 **always** links through `juce_graphics`, which
the plug-in uses for all text rendering. The JUCE root `LICENSE.md` lists both:

| Component | Licence | What the licence requires for a binary release |
|---|---|---|
| **HarfBuzz** (`juce_graphics/fonts/harfbuzz`) | "Old MIT" | The copyright notice and permission paragraphs must appear in all copies. |
| **SheenBidi** (`juce_graphics/unicode/sheenbidi`) | Apache 2.0 | §4(a): recipients must be given a copy of the licence (and any NOTICE file). |

The components already listed also carry binary-distribution conditions:
- **libjpeg (IJG).** If only executable code is distributed, the documentation must state "this software is based in
  part on the work of the Independent JPEG Group".
- **FLAC and Ogg Vorbis (BSD-3).** Binary redistributions must reproduce the copyright notice, the conditions and
  the disclaimer. `JUCE_USE_FLAC` and `JUCE_USE_OGGVORBIS` default to 1 in `juce_audio_formats.h`, so the codec
  sources are compiled into the module.
- **zlib and libpng.** No notice is required in binaries; listing them is fine.

THIRD_PARTY.md says "Full licence texts ship with JUCE: `JUCE_8_0_8/LICENSE.md` …", but the end user has no JUCE
checkout, and the release script packs only `LICENSE` and `THIRD_PARTY.md`.

**Fix.**
1. Stop compiling what is never used. Only WAV is written (`OfflineRenderer.cpp` uses `WavAudioFormat` directly), so
   add to `IFX_DEFS` in CMakeLists.txt:
   ```cmake
   JUCE_USE_FLAC=0
   JUCE_USE_OGGVORBIS=0
   ```
   Then drop the FLAC / Ogg rows.
2. Add the missing rows to THIRD_PARTY.md:
   ```markdown
   | HarfBuzz (bundled in JUCE `juce_graphics`) | as shipped with JUCE 8.0.8 | "Old MIT" | statically linked |
   | SheenBidi (bundled in JUCE `juce_graphics`) | as shipped with JUCE 8.0.8 | Apache License 2.0 | statically linked |
   ```
   Add this sentence to the body: "This software is based in part on the work of the Independent JPEG Group."
3. Ship the licence texts in the ZIP. Add this to make_release.ps1 after the notices loop; it finds JUCE the same way
   CMake does:
   ```powershell
   $juce = @((Join-Path $root "JUCE"), (Join-Path (Split-Path $root -Parent) "JUCE_8_0_8")) |
           Where-Object { Test-Path (Join-Path $_ "LICENSE.md") } | Select-Object -First 1
   if (-not $juce) { throw "JUCE checkout not found: third-party licence texts cannot be packed" }
   $texts = [ordered]@{
       "JUCE-LICENSE.md"        = "LICENSE.md"
       "HarfBuzz-COPYING.txt"   = "modules/juce_graphics/fonts/harfbuzz/COPYING"
       "SheenBidi-LICENSE.txt"  = "modules/juce_graphics/unicode/sheenbidi/LICENSE"
       "libpng-LICENSE.txt"     = "modules/juce_graphics/image_formats/pnglib/LICENSE"
       "IJG-README.txt"         = "modules/juce_graphics/image_formats/jpglib/README"
       "zlib-README.txt"        = "modules/juce_core/zip/zlib/README"
       "VST3-SDK-LICENSE.txt"   = "modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt"
   }
   foreach ($k in $texts.Keys) {
       $src = Join-Path $juce $texts[$k]
       if (-not (Test-Path $src)) { throw "Missing licence text: $src" }
       Add-Item $src "$name/ThirdPartyLicenses/$k"
   }
   ```
   Extend the licence check to fail if any `ThirdPartyLicenses/` entry is missing. Extend CI `checks` to grep for
   `HarfBuzz` and `SheenBidi`.

### B2. The commercial licence gates are still open (THIRD_PARTY.md "Commercial release gate")

The bundled VST3 SDK is **3.7.12** (`pluginterfaces/vst/vsttypes.h: kVstVersionString "VST 3.7.12"`). It is dual
licensed: under the proprietary option it requires a Steinberg-signed agreement before publishing, and the
alternative is GPLv3. JUCE 8 needs a commercial tier for a closed-source release. The document states both gates
correctly, but they are release blockers until they are ticked in `Docs/QA.md`. This is a process item, not a code
item.

(If a later JUCE release bundles a VST3 SDK that Steinberg ships under MIT (3.8 and later, per Steinberg's 2025
announcement — check the bundled `LICENSE.txt`), the Steinberg agreement would no longer be needed, but that
MIT notice must then ship with the other texts.)

---

## Majors

### M1. The stand-alone app and the plug-in share one settings file, so each wipes the other's keys (PluginProcessor.cpp:113-123; JUCE `juce_audio_plugin_client_Standalone.cpp:73-79`)

On Windows, the JUCE stand-alone wrapper stores its settings with `applicationName = JucePlugin_Name`,
`filenameSuffix = ".settings"` and `folderName = ""`. By `PropertiesFile::Options::getDefaultFile`
(juce_PropertiesFile.cpp:105-118) that resolves to `%APPDATA%\Interference\Interference.settings`. That is the
**same file** as the processor's options (`folderName "Interference"`, application name "Interference").

In the stand-alone app, two `PropertiesFile` objects therefore own one file, and each save writes only its own keys:
- the app's keys: `audioSetup`, `filterState`, `shouldMuteInput`;
- the processor's keys: `tooltips`, `renderFolder`.

What goes wrong:
- Whichever object saves last wins, so the audio-device setup or the options silently disappear. The processor saves
  on a 3 s timer and in its destructor.
- *Hard reset* deletes the stand-alone app's device setup as well.

Install.md documents the path correctly for the plug-in, but it does not mention the collision.

**Fix.** Give the processor its own file:
```cpp
o.applicationName = "Interference Options";   // %APPDATA%\Interference\Interference Options.settings
o.folderName      = "Interference";
// and for IFX_DATA_ROOT: dataRoot.getChildFile ("Options").getChildFile ("Interference Options.settings")
```
1.0 has not shipped, so no migration is needed. Update Install.md (the table and the Uninstall list):
- `%APPDATA%\Interference\Interference Options.settings` holds the plug-in options (tooltips, last render folder;
  deleted by Hard reset);
- `%APPDATA%\Interference\Interference.settings` holds the stand-alone app's audio / MIDI device setup and its last
  state (written by the stand-alone app only).

### M2. The README in the ZIP links to a document that is not packed (README.md, make_release.ps1:62)

`README.md` links `Docs/research/ElectricalInterference.md`. The script packs only `Docs/*.md` (not recursive), so
the link is dead in the release. Either pack that one file:
```powershell
Add-Item (Join-Path $root "Docs\research\ElectricalInterference.md") "$name/Docs/research/ElectricalInterference.md"
```
or change the README row to say "(source repository only)". The other internal notes in `Docs/research/` should stay
out.

### M3. The signing step signs any push with the production certificate (ci.yml:113-141)

The step itself is safe:
- the PFX goes to `RUNNER_TEMP` and is deleted in `finally`;
- secrets are masked in the log, and fork PRs get no secrets, so they ship unsigned with a notice;
- `signtool verify /pa` fails the job for an untrusted certificate, so a signature is never faked.

But every `push`, to any branch, signs with the release certificate, so an experimental branch produces signed
binaries. Restrict signing to release tags and to a protected environment:
```yaml
      - name: Sign binaries (only with a configured certificate)
        if: startsWith(github.ref, 'refs/tags/v')
        environment: release        # (job-level) with required reviewers holding the secrets
```
The environment line must go on the job (for example a separate `release` job that needs `build`), because
`environment` is not valid at step level.

---

## Minors

**m1. The ZIP is reproducible only on the same PowerShell / .NET version (make_release.ps1:6-10, 84-90).**
- `CompressionLevel::Optimal` deflate output differs between .NET Framework 4.x (Windows PowerShell 5.1) and .NET 8/9
  (PowerShell 7, which uses zlib-ng from .NET 9). Say so in the script description: "reproducible for identical
  inputs **and the same PowerShell**". CI runs the script with `powershell` (5.1), so the reference is 5.1.
- `Sort-Object -Property Entry -CaseSensitive` compares with the **current culture**, so entry order can differ
  between machines with different locales. Use an ordinal sort:
  ```powershell
  $sorted = [System.Collections.Generic.List[object]]::new($items)
  $sorted.Sort([System.Comparison[object]] { param($a, $b) [string]::CompareOrdinal($a.Entry, $b.Entry) })
  ```

**m2. The relative-path computation breaks with `-BuildDir .\build` or a trailing slash (make_release.ps1:48-58).**
`$vst3.Length` is taken from an unnormalised path, while `FullName` is normalised, so `Substring` cuts the wrong
number of characters and produces corrupt entry names. Normalise first. The space in "interfearance vst" is
otherwise handled correctly (`Join-Path`, `& $exe`, `ReadAllBytes`).
```powershell
$vst3 = (Resolve-Path -LiteralPath $vst3).ProviderPath.TrimEnd('\')
$exe  = (Resolve-Path -LiteralPath $exe).ProviderPath
```
Do the same for `$root` at the top: `$root = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).ProviderPath`.

**m3. `-Build` runs the tests without `IFX_DATA_ROOT` (make_release.ps1:40-44).** CI isolates the data folder, but a
local release build lets the DSP suite write into the developer's Documents folder. Set it for the test run:
```powershell
$env:IFX_DATA_ROOT = Join-Path ([IO.Path]::GetTempPath()) "interference-release-tests"
New-Item -ItemType Directory -Force $env:IFX_DATA_ROOT | Out-Null
```

**m4. `$ErrorActionPreference = "Stop"` with native tools under Windows PowerShell 5.1.** With 5.1, when stderr is
redirected (a CI log, or `> log 2>&1`), native stderr lines become `NativeCommandError` records, and `Stop` then
aborts on the first CMake warning. The `-Build` path is exposed to this; the packaging-only path CI uses is not. Wrap
the native calls:
```powershell
function Invoke-Native([scriptblock] $b) { $eap = $ErrorActionPreference; $ErrorActionPreference = "Continue"; try { & $b } finally { $ErrorActionPreference = $eap } }
Invoke-Native { cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64 }; if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
```

**m5. Pin the runner image (ci.yml:59, 161).** `windows-latest` moves between images. The generator is hard-coded as
"Visual Studio 17 2022", so an image that ships only a newer Visual Studio would break configure. Use
`runs-on: windows-2025` (or `windows-2022`) and bump it deliberately. Also cache the key on the JUCE commit, not only
the tag, if the tag might ever be moved (`git ls-remote` in a first step).

**m6. The release ZIP does not say whether it is signed.** Add a line to the ZIP (for example a generated
`SIGNATURE.txt`: "signed by … / UNSIGNED build"), or at least put it in the uploaded artifact name, so testers cannot
mistake an unsigned CI build for a release.

**m7. Install.md details.**
- Step 2 should mention that Windows may block files from a downloaded ZIP (*Unblock* before unzipping). This is
  already in Troubleshooting; move it up to Install.
- "Documents" can be redirected to OneDrive, and the plug-in follows the redirection (`CSIDL_PERSONAL`). Say so, so
  users find their presets.
- The VST3 path is `build/Interference_artefacts/Release/VST3/Interference.vst3`, packed as
  `Interference-1.0.0-win64/VST3/Interference.vst3/…`. That matches the JUCE CMake layout
  (`Contents/x86_64-win/Interference.vst3`, plus `Contents/Resources/moduleinfo.json` generated by JUCE 8). Correct.
- The preset and support paths match `StateIO::userDataFolder()` and `Diagnostics::supportFolder()`. The
  `IFX_DATA_ROOT` paragraph matches the code (programmatic override > environment variable > Documents). Correct.

**m8. CI checks: YAML and steps verified by inspection.**

Verified correct:
- `${{ github.workspace }}\ci-data` is valid unquoted.
- `inputs.slow` is available for `workflow_dispatch`.
- Single-line `pwsh` steps inherit the runner's automatic `$LASTEXITCODE` exit.
- The exe and artifact paths match the JUCE `_artefacts/Release` layout.
- `${env:ProgramFiles(x86)}` is the correct PowerShell syntax.
- `-match '\\x64\\'` plus a descending sort picks the newest x64 signtool.
- The slow job builds only `InterferenceTests` and runs `--slow` (the soak took 437 s locally, so a CI runner needs
  about 15-25 min, well inside the 240 min limit).

Open risk: the GUI suite needs a desktop session. GitHub's Windows images provide one, but if the snapshots come out
blank, run that step in an interactive runner.

**m9. Trademark line in the shipped docs.** THIRD_PARTY.md has the VST trademark note under "release gate". Also put
the plain notice "VST is a registered trademark of Steinberg Media Technologies GmbH" in README.md, which is the
document users read.

---

## What is fine

- The version comes from `project(Interference VERSION 1.0.0 …)`, and the regex matches.
- The licence presence checks are in both the script and CI.
- The entry timestamp comes from the last commit, in UTC.
- The SHA-256 file uses the `sha256sum` format.
- The ZIP is created fresh (`CreateNew` after deleting any old file).
- The static CRT (`/MT`) means no VC++ redistributable is needed. That is correct and worth stating in Install.md.
- `COPY_PLUGIN_AFTER_BUILD FALSE` means CI never writes to `Program Files`.
