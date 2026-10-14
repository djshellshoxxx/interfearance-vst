# Spec B — Presets and state (V1, V2, V3, U3, U5, N5, V5)

Shared facts: `PluginState` (params snapshot, seed, seedLock, take, slotA/B, `Pattern`, MIDI map, name/category/description)
is the single sound representation (`Plugin/StateIO.h`). Presets are `.ifxpreset` XML. Factory bank in `FactoryPresets.cpp`
(27 presets, 7 categories). A/B lives in the processor (`abStates[2]`). `randomize()` currently resets to init then draws
ranges per source in `PluginProcessor.cpp`.

---
## V1 — Preset library and browser

### Goals
Search, tag filter, favourites, recents, folders, rename/duplicate/delete, drag-and-drop import, pack import/export,
"similar presets" — without changing the sound representation.

### Data model
* **Preset file** (`.ifxpreset`) gains optional attributes on the root: `author`, `tags` (comma list, lower-case, max 8,
  each ≤ 24 chars), `created` (ISO date), `packId`. Old readers ignore them (verified: `fromValueTree` ignores unknown).
  `PluginState` gets `juce::StringArray tags; juce::String author;` (metadata only; excluded from `soundEquals`).
* **Library index** `PresetLibrary` (new, `Plugin/PresetLibrary.h/.cpp`, JUCE, message-thread API):
  ```cpp
  struct PresetEntry { juce::String id;        // "factory:<n>" | "user:<relative path>" | "pack:<packId>/<n>"
                       juce::String name, category, author, description; juce::StringArray tags;
                       juce::File file;        // invalid for factory
                       bool favourite = false; int64 lastUsed = 0; int useCount = 0;
                       std::array<uint8_t,16> fingerprint; }; // for "similar"
  class PresetLibrary { public: void rescan(); // async
                         juce::Array<PresetEntry> query(const Query&) const; ... };
  ```
* **Per-user metadata** (favourites, recents, use counts) in `userDataFolder()/library.json`, not in preset files (read-only
  media and packs stay untouched). Atomic write (`TemporaryFile`), tolerant reader.
* **Folders**: `Presets/<sub>/…` up to 3 levels; folder = category if the file's `category` is empty.
* **Scan** runs on a `juce::ThreadPool` job; results posted with `MessageManager::callAsync`; file mtime cache avoids
  re-parsing (only header attrs parsed via `XmlDocument` limited to root + `<Params>` skipped → fast for 5 000 presets).
  Folder watcher: poll every 5 s while the browser is open (`juce::FileSystemWatcher` is not cross-platform in JUCE 8).
* **Fingerprint / similarity**: 16-byte vector = quantised normalised values of 12 summary features (source types one-hot,
  hum, rf, mean wear/dirt/damage, lane density, studio-on, drive, resonator, echo, mix). `similarity = 1 − L1/16·255`.

### UI
* New overlay `UI/PresetBrowserOverlay.cpp` (class `PresetBrowserOverlay : Overlay`), opened by the header preset box
  "Browse…" item or `Ctrl+O`/`B`. Three panes: filters (categories, tags, packs, Favourites, Recent, Mine), result list
  (name, category, tags chips, ★), details (description, source icons, "Similar" button, rename/duplicate/delete).
* Search box: case-insensitive substring over name, tags, author, description; `#tag` token filters tags; debounced 120 ms.
* Keyboard: `↑/↓` move, `Enter` load, `Space` toggle favourite, `F2` rename, `Del` delete (confirm), `Esc` close.
  Loading from the list previews nothing (see U5 for audition) and respects the existing modified-marker confirmation.
* Header combo box stays for quick switching but lists Favourites first when any exist (`addSectionHeading("Favourites")`).
* Drag-and-drop: `FileDragAndDropTarget` on the editor accepts `.ifxpreset` (load; with `Alt` import into user folder) and
  `.ifxpack` (import dialog).
* Import/export packs: `.ifxpack` = ZIP with `manifest.json` `{ "format":1, "id":"com.author.pack", "name", "version",
  "author", "licence", "presets":[{ "file","sha256" }] }` + preset files + optional `cover.png`. Import verifies SHA-256s,
  rejects path traversal (`..`, absolute, symlink), caps 5 000 files / 50 MB, installs to `Presets/Packs/<id>/`.
  Uninstall removes that folder only.

### Edge cases
Duplicate names (suffix ` (2)`), illegal filename chars, read-only folder (error banner), preset from a newer version
(`pluginVersion` > current: load, show "created with newer version" warning), corrupt files (skipped, listed in Debug),
missing user folder (create), 0 results ("No presets match — clear filters" button).

### Tests (`Tests/FeaturePresets.cpp`, `TestGui`)
`library.scansFactoryAndUser`, `library.searchTagsAndFavourites`, `library.packImportRejectsTraversal`,
`library.packRoundTripChecksums`, `library.similarRanksIdenticalFirst`, `library.metadataFileSurvivesCorruption`,
`browser.keyboardFlow`, `browser.dragDropLoad`, `state.oldReaderIgnoresNewAttributes`. Fuzz: 1 000 random-byte `.ifxpack`
files never crash or write outside the temp root.

### Acceptance
5 000-preset folder opens browser in < 300 ms (cached) and cold scan < 3 s on the test laptop; no audio-thread impact
(`TestPerf` unchanged); a pack installed and removed leaves no files behind.

---
## V2 — Randomise and mutate with locks

### Behaviour
* **Randomize** (existing button) keeps its meaning (reset + new safe random) but is rebuilt on `Randomizer`.
* **Mutate** (new, `Alt`-click Randomize or a header "Mutate" button + amount slider 0–100 %, default 20 %): nudges *unlocked*
  parameters from the current values.
* **Locks**: padlock toggle per section (Slot A, Slot B, Sources, Environment, Lane, Studio, Output) in a popover on the
  Randomize button, plus right-click → "Lock for randomise" on any knob/choice. Locked = never changed by Randomize/Mutate.
  Seed and slot sources have their own locks. Stored in `SessionExtras` (project), default: Output + Mix locked.
* **Dice history**: each action has a 32-bit `rollId`; `Ctrl+Shift+Z`-style "previous roll" is handled by U3 undo.

### Design — `Core/Randomizer.h` (no JUCE; deterministic)
```cpp
struct RandomSpec { float lo, hi; uint8_t flags; };   // in normalised space; flags: Never, Discrete, LogDomain
struct LockMask   { std::bitset<kNumParams> params; bool seed, slotA, slotB, lane; };
struct RandomizeOptions { uint64_t rollSeed; float amount /*mutate*/; bool fresh /*randomize vs mutate*/; Policy policy; LockMask locks; };
void randomize (PluginState&, const RandomizeOptions&);   // pure; same inputs -> same output
```
* `RandomSpec` table generated from `IFX_PARAMS` defaults plus an override table `Core/RandomSpecs.h` (hand-tuned ranges,
  seeded from the current `randomize()` ranges so behaviour is preserved).
* **Never-randomise set** by default: bypass, mix, mixLaw, dryTrim, outTrim, ceiling*, monitor, mode, policy, bpm, runInternal,
  mains, trigA/B, fill, killFb/killTail, rawHw, all `ghost*`. (Mode is a user intent, not a sound.)
* **Mutate step**: `x' = clamp(x + N(0, amount·w) )` in the normalised/log domain, `w` = per-param width (default 0.25,
  discrete params flip with probability `amount/4`). Gaussian from `Random.h` (sub-stream keyed `hash(rollSeed, paramIndex)`),
  so adding parameters never reshuffles old results.
* **Safety**: after drawing, run `safeLoudness(PluginState)`: renders 1.5 s of pink noise at −18 dBFS offline through the
  engine (message thread, `OfflineRenderer`, ~15 ms) and if peak of the wet path > −1 dBFS, scale `inTrim`/Studio level
  down; never touches locked params. Policy `Natural` caps are respected by the engine anyway.
* **Lane**: unlocked lane gets Euclidean-based sparse pattern (uses N2 generator once available; until then existing code).

### UI / state
`UI/RandomizePopover.cpp` (component, mounted by a header extension point). Locks persisted in `SessionExtras`;
per-knob lock icon drawn by `ParamKnob` (small 6 px padlock top-right when locked, 40 % alpha).

### Tests
`rand.deterministicForRollSeed`, `rand.lockedParamsNeverChange` (property test, 1 000 rolls × random lock masks),
`rand.valuesInRangeAndFinite`, `rand.neverSetIsUntouched`, `rand.mutateAmountZeroIsIdentity`, `rand.mutateDistanceScalesWithAmount`
(mean normalised distance monotone), `rand.safeLoudnessBoundsPeak`, `rand.legacyRangesPreserved` (statistical: old vs new
range envelopes overlap > 95 %), `session.locksRoundTrip`, GUI: lock icon toggles, Randomize undoable (U3).

### Acceptance
Every roll sounds "plausible": 1 000 random rolls × 3 s render never produce NaN, DC > 0.05, or peak > 0 dBFS;
a locked slot's rendered output is bit-identical before/after a roll that only changes the other slot.

---
## V3 — Morph between two states

### Semantics (decided)
* Morph blends **A/B states** that already exist (`abStates[2]`). New automatable parameters (in `ParamsExt/morph.def`):
  `morph` (0–1, default 0, "Morph A→B"), `morphOn` (bool, default off).
* When `morphOn`: audio uses `effective = lerp(sideA, sideB, morph)` where **the currently selected A/B side comes from the
  live APVTS values** (so knob moves and host automation edit the selected side in real time) and the other side from its
  stored state. Switching A/B swaps which side is live (existing `selectAB` flow, unchanged).
* Interpolation per parameter kind (in `ParamPipeline` stage `MorphStage`):
  Float: linear in normalised space, geometric for `skew==1` (frequency/time params). Bool/Choice/Int: take A for `m < 0.5`,
  B for `m ≥ 0.5` (existing 10 ms declick fades handle the switch). Seed, take, pattern, MIDI map: from the nearer side.
* **Precondition**: `slotA`/`slotB` source types equal in both states. Otherwise Morph is disabled with tooltip
  "A and B use different sources — morph needs the same two sources." (avoids an un-declicked source swap).
* `morph` smoothing: 20 ms one-pole on the effective values; hardware-position params keep the engine's own mechanical
  slew (Decisions), so nothing zippers.
* UI: slider under the A/B buttons labelled "A ◄─ morph ─► B"; knobs of morphed params draw a ghost tick for the live
  (edit-side) value and the solid pointer at the effective value (provided by F0.3's `base` vs `effective`).
* Presets store no morph state; A/B states are session data (existing), so `SessionExtras` stores `morphOn/amount` only.

### Tests
`morph.endpointsEqualPureStates` (m=0 → A bit-exact, m=1 → B bit-exact), `morph.midpointFloats`, `morph.skewedGeometric`,
`morph.discreteSwitchesAtHalfWithDeclick`, `morph.disabledWhenSourcesDiffer`, `morph.automationEditsSelectedSideOnly`,
`morph.noEventsOnSmoothSweep` (sweep m 0→1 on still hardware produces no pop/event), `morph.stateRoundTrip`, GUI slider.

### Acceptance
Smooth sweep 0→1 over 4 s on all four sources produces output within ±0.5 dB of per-block interpolation of endpoint RMS and
no discontinuity > 6 dB between adjacent 1 ms RMS windows.

---
## U3 — Undo / redo (global)

### Design — `Plugin/UndoHistory.h/.cpp`
* **Entry** = full `PluginState` snapshot (≈ 6 KB with 2×64-step pattern) + label + timestamp. Ring of 200, 1.5 MB cap.
* **What records**: knob/switch edits made in the UI (coalesced from `parameterGestureChanged(begin)`…`end` into one entry; a
  non-gesture change within 400 ms of the previous same-parameter change coalesces), preset load, Randomize/Mutate/Reset,
  slot source changes, lane edits (editor commits), seed/take changes, A/B copy, import. **Never**: host automation, MIDI
  CC, morph/macro/gremlin effective values, telemetry.
* **Taking the snapshot**: `recordBefore(label)` captures state *before* the change from a cached "last stable" state, so
  one entry = one user intent. Message thread only; audio thread never touches it.
* **Undo/redo apply**: `applyState(state, markAsLoaded=false)` (existing) inside an `UndoGuard` flag that suppresses
  recording. Host is notified via `setValueNotifyingHost` (already what `applyState` does), so DAW undo and ours co-exist.
* **Shortcuts**: `Ctrl+Z` / `Ctrl+Y` / `Ctrl+Shift+Z` when the editor window has focus and the focused component isn't a text
  editor or the lane editor (which keeps its own lane-local undo; add a header-level toggle in Options "Ctrl+Z in lane edits
  the lane first" default on). Header buttons `↶ ↷` with tooltips showing the next label ("Undo: Randomize").
* **History popup** (`H` or right-click on ↶): list of last 30 labels; click jumps.
* Entry reset on `setStateInformation` from host (a project load) and on hard reset.

### Tests
`undo.knobDragIsOneEntry`, `undo.presetLoadUndo`, `undo.redoClearedByNewEdit`, `undo.hostAutomationNotRecorded`,
`undo.ringCapDropsOldest`, `undo.labelsReadable`, `undo.laneUndoPriority`, `undo.noAudioThreadAccess` (TSAN job), GUI buttons enabled state.

### Acceptance
200 randomised edits then 200 undos return to the original state (`soundEquals`); memory ≤ 1.5 MB; no entry for pure automation playback.

---
## U5 — Preset audition (preview on hover/select)

### Design (no shipped audio assets)
* `Plugin/Auditioner.h/.cpp`: small generator run **in the audio thread** only while the user enables "Audition" (toggle in the
  browser and header; default off): sources `Pluck` (Karplus-Strong), `Drums` (kick/snare/hat synth 100 bpm loop), `Pad`
  (detuned saw chord, lowpassed), `Pink noise`, `Sine 1 kHz`. Selected in Options. Output level −18 dBFS RMS, hard-limited.
* The auditioned signal is **added to the plugin input** (before the modelled hardware), so the preset is heard exactly as an
  insert on that material. Works in the standalone and in hosts (even with transport stopped). Auto-mutes when the host
  transport plays *and* the input is non-silent (so it never muddies a real mix) unless "Force audition" is on.
* Browser hover for 350 ms loads the preset into a **shadow engine state** only if A/B isn't modified? Simpler and safe:
  hover = no change; **single click = load + audition retrigger**, Esc/`Ctrl+Z` returns. (Avoids a second engine.)
* Audition thread-safety: parameters `audKind`, `audOn` are atomics in the processor (not host parameters, not saved);
  generator state preallocated; reset on `prepareToPlay`.

### Tests
`aud.generatorsFiniteAndBounded`, `aud.offDoesNotChangeOutputBitExact`, `aud.levelWithinSpec`, `aud.mutesOnPlayingTransport`,
`aud.noAllocationInProcess`. Acceptance: audition on/off has no click (10 ms ramps) and < 1 % CPU.

---
## N5 — Shareable preset codes and seeds

### Format `IFX1-<base64url>` (copy/paste in forums, chat)
Binary payload: `ver(1) | flags(1) | seed(8) | slotA(1) | slotB(1) | changedBitmap(ceil(N/8)) | values(14-bit each,
for set bits only, bit-packed) | patternRLE | crc32(4)`; compressed with `juce::GZIPCompressorOutputStream`. `changedBitmap`
marks params that differ from defaults — new params default to "unchanged", so old codes remain valid. Typical size 120–400
chars. Hard cap 4 KB input; reject others.
* Quantisation: normalised 14-bit (discrete params exact). Documented "within quantisation; exact via .ifxpreset".
* **Seed code**: Crockford base32 of the 64-bit seed, grouped `ABCD-EFGH-JKMN-PQRS-T` (13 chars).
* UI: Menu → "Copy preset code", "Paste preset code…" (reads clipboard, shows preview name/sources before applying; undoable),
  "Copy seed", and drag/paste of text onto the editor.
* Security: parser is a pure function `PresetCode::decode(string, PluginState&, error)`; all values clamped through
  `StateIO::sanitise`; decompression output capped at 64 KB; fuzzed.

### Tests
`code.roundTripWithinQuantisation`, `code.defaultsOnlyIsShort`, `code.crcDetectsCorruption`, `code.rejectsOversize`,
`code.fuzzNoCrash` (100 000 random strings, libFuzzer target in nightly), `code.oldCodeLoadsAfterNewParamsAdded`.

---
## V5 — Preset packs (content programme)

### Product definition
Each pack: 16–24 presets, one theme, one story, 2–3 "hero" presets, cover image, README. Initial themes:
**Broken Studio** (bad cables/jacks/pots in a mix), **Haunted Radio** (RF + ground loop + lane), **Bad Venue PA** (dimmer buzz,
hum, 2-prong gear), **Dead Pedalboard** (loose jacks, dirty pots on guitar), **Sound-Design Toolkit** (Create-mode one-shots).

### Production pipeline
1. Author in plug-in → save `.ifxpreset` with name, category, tags (≥ 3), description (1–2 sentences, plain words, physical cause).
2. `tools/pack_build.py <folder>`: validates (see QA gates), renders 8 s previews with `ifx-render --gen drums|pluck|noise`,
   writes `manifest.json` (+SHA-256), zips to `.ifxpack`, creates `previews/*.ogg`-free WAV set for the site (D1).
3. Gates per preset (script-enforced): not silent (RMS > −60 dBFS) unless tagged `silent-until-triggered`; peak ≤ −1 dBFS on
   all three generators; no NaN; loads on `Natural` without clamp warnings unless tagged `expressive`; unique name; description present.
4. Naming: `Pack/Category/Name`; tags lower-case hyphenated; versioned packs (`1.0.0`); updates replace by `id`.
5. Licensing: author agreement (CLA text), pack-level licence in manifest; only self-generated audio (no samples).
6. Distribution: factory bank stays embedded; packs are separate downloads (site) or in-app "Import pack".

### Acceptance
Every shipped pack passes `pack_build.py --check` in CI; install/uninstall round-trips; preset count and categories visible in browser.
