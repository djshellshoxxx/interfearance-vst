# INTERACTION & BEHAVIOR SPEC: Wubforge

## Guiding principle: nothing gets stuck
Same rule as Ascent/Descent. Every overlay dismissible by Escape, click-outside, and a visible close control. Opening a new overlay closes any other open overlay. Only ONE modal/popup visible at any time. Enforced by a single OverlayManager class. Focus-loss auto-closes transient overlays after 200 ms. Automated test covers every dismissal path.

## Global keyboard shortcuts (plugin has focus)
- **Space**: play / stop preview
- **G**: generate new
- **R**: reroll current seed
- **N**: reroll notes only (Pattern Mode)
- **W**: reroll wobble modulation only
- **Z / Y** or **Ctrl+Z / Ctrl+Y**: undo / redo
- **A / B**: recall A or B slot
- **1..7**: toggle visibility of automation lanes 1 through 7
- **E**: toggle Easy / Advanced mode
- **M**: toggle MIDI / Pattern mode
- **B**: toggle Bass / Wobble mode
- **S** / **Shift+S**: quick-save / Save As
- **Escape**: close any open overlay, or defocus text field

## Control interactions
Same as Ascent/Descent for every knob, slider, and numeric field. Recap:

**Left-click drag**: adjust value.
**Left-click**: focus.
**Double-click**: reset to default.
**Right-click**: opens context menu:
1. Enter value...
2. Reset to default
3. Copy value / Paste value
4. ─────
5. MIDI Learn / MIDI Unlearn / Show MIDI mapping...
6. ─────
7. Assign to macro... (Weight, Grit, Movement, Speed, Character, Space)
8. Automate with LFO 1-4 / Seq 1-2 / Env 1-2
9. ─────
10. Lock parameter
11. Randomize this parameter

**Mouse wheel**: one step. Shift+wheel fine (0.1x). Ctrl+wheel coarse (10x).
**Ctrl+click-drag**: fine mode.
**Shift+click-drag**: absolute jump.
**Hover 400 ms**: tooltip with parameter name, value in user units, MIDI mapping.

## Handle interactions on automation lanes, LFO drawer, sequencer steps, envelope points
Identical grammar across every editable curve in the plugin:
- Single left-click on empty area: add handle/point.
- Single left-click on handle: select (larger display, value readout).
- Left-drag on handle: move, snaps to grid unless Alt.
- Right-click on handle: Delete, Set exact value, Copy, Paste, Reset, Lock position.
- Right-drag on segment between handles: bend tension.
- Double-click on handle: delete.
- Double-click on empty area: add handle.
- Shift-click multiple: multi-select.
- Ctrl+A within a lane/editor: select all.
- Right-click on empty area: lane context menu.
- Right-click on legend chip: show/hide, change color, solo lane.

**LFO drawer specifics**: the "drawn" LFO shape has up to 32 breakpoints per cycle. Snap to horizontal grid (16ths of a cycle). Same handle grammar. A small "Cycle preview" plays the drawn shape at the currently set rate.

**Step sequencer specifics**:
- Left-click on a step bar to set its value (drag up/down while holding).
- Right-click on a step: mute, solo, set probability, clear.
- Shift+click multiple steps: select group; drag any selected step to modify all proportionally.
- Right-click on the sequencer background: Reset, Randomize, Shift left/right, Reverse, Invert.

**Piano-roll specifics** (Pattern editor):
- Left-click on empty grid: add note at that pitch/time.
- Left-click on note: select.
- Left-drag on note body: move.
- Left-drag on note right edge: resize.
- Double-click on note: delete.
- Right-click on note: properties (glide toggle, accent toggle, probability, velocity, exact pitch, exact length).
- Shift-click multiple notes: multi-select. Delete key removes all.
- Right-click on empty grid: pattern context menu (Randomize, Clear, Fit to bar, Humanize, Quantize, Reverse, Invert vertically within scale, Transpose up/down octave).

## Button behaviors

**GENERATE button**:
- Left-click: reroll unlocked parameters, run validator, replace current sound. Adds to history.
- Right-click: menu with variants (Generate + store current in A/B, Generate within tight range, Generate within loose range, Generate wobble only, Generate pattern only, Generate with new seed).
- Never blocks UI. Worker thread. If over 500 ms, spinner on button, Escape cancels.

**BASS / WOBBLE toggle**:
- Left-click: switches. Confirmation only if unsaved edits would be invalidated.
- Right-click: Reset to default for this type, Copy current wobble to preset.

**MIDI / PATTERN toggle**:
- Left-click: switches. In Pattern Mode, plugin ignores host MIDI notes (still uses host transport).
- Right-click: "Use host MIDI for key detection" toggle (Pattern Mode uses incoming notes to auto-select key).

**Preset name in header**:
- Left-click: opens preset browser overlay.
- Right-click: Save, Save As, Rename, Delete, Reveal in folder, Reload from disk.

**A / B slots**:
- Left-click: recall.
- Shift+left-click: store current.
- Right-click: Store, Recall, Clear, Swap A↔B, Copy A to B, Copy B to A.

**History thumbnail**:
- Left-click: recall.
- Right-click: Recall, Save as preset..., Delete from history, Pin.
- Drag onto A/B slot: store there.
- Drag onto Export handle: render to WAV.

**Panic button**:
- Left-click: MIDI all-notes-off to every layer, resets voice envelopes, resets LFO phases, clears delay/reverb tails.
- Right-click: menu with "Reset all LFOs to phase 0", "Reset pattern to bar 1", "Full DSP reset".

**Lock / Dice icons**: same as Ascent/Descent.

**Help button**: opens help overlay. Giant close button top-right, plus Escape / click-outside / focus-loss dismissal. **This must be tested. A stuck help window fails QA.**

## MIDI Learn behavior
Same as Ascent/Descent:
- Right-click any control → MIDI Learn.
- Control glows, "Learning..." label appears.
- First CC received maps. Auto-dismiss on map, Escape, or 15-second timeout.
- Small colored dot with CC number on mapped controls.
- Manual mapping via Show MIDI mapping...: CC number, channel (1-16 or Omni), min/max, curve, invert, takeover mode.
- Global MIDI panel from header icon: table of all mappings, edit/remove per row, clear-all with confirm.
- Mappings persist per-preset plus global default file.

**Additional Wubforge-specific MIDI features**:
- **Note-to-parameter mapping**: right-click any parameter → "Map to note pitch" (parameter follows the pitch of incoming MIDI notes) or "Map to velocity" (parameter follows note velocity).
- **Aftertouch and mod wheel**: exposed as modulation matrix sources, no special mapping needed.
- **MIDI CC output**: optional. Plugin can send its internal LFO and sequencer values as MIDI CC to a specified channel/CC for driving external gear. Toggle in Preferences.
- **MIDI clock**: plugin syncs to host transport; no external MIDI clock needed inside the plugin.

## Preset system

### File format
JSON, extension **`.wfpreset`**. Same rationale as Ascent/Descent.

Structure:
```json
{
  "format": "wubforge-preset",
  "format_version": 1,
  "plugin_version": "1.0.0",
  "created_utc": "2026-09-17T12:34:56Z",
  "modified_utc": "2026-09-17T12:34:56Z",
  "name": "Riddim Wob 01",
  "author": "Factory",
  "category": "Wobble",
  "subcategory": "Riddim",
  "tags": ["riddim", "half-time", "growl", "aggressive"],
  "description": "Optional user notes.",
  "type": "wobble",
  "mode": "pattern",
  "seed": 8471293847,
  "length_bars": 2,
  "tempo_sync": true,
  "key": "F#",
  "scale": "phrygian",
  "macros": { "weight": 0.8, "grit": 0.7, "movement": 0.9, ... },
  "voice_layers": [
    {
      "type": "sub",
      "enabled": true,
      "gain_db": 0.0,
      "source": { "waveform": "sine", "octave": -2, ... },
      "filter": { ... },
      "amp_envelope": { ... },
      "fx_inserts": [ ... ]
    },
    ...
  ],
  "modulation": {
    "lfos": [
      { "shape": "sine", "rate": "1/8", "phase": 0, "depth": 0.85, "retrigger": true, "free_run": false, "drawn_shape": null },
      ...
    ],
    "step_sequencers": [ ... ],
    "envelopes": [ ... ],
    "matrix": [
      { "source": "lfo1", "destination": "global_filter.cutoff", "amount": 0.9, "curve": "exp", "bipolar": false },
      ...
    ]
  },
  "pattern": {
    "grid": "1/16",
    "length_bars": 2,
    "swing": 0.0,
    "humanize": 0.05,
    "notes": [
      { "pitch": 42, "start_step": 0, "length_steps": 4, "velocity": 100, "glide": false, "accent": true, "probability": 1.0 },
      ...
    ]
  },
  "canvas_lanes": {
    "filter_cutoff": { "handles": [...] },
    "wobble_depth": { "handles": [...] },
    ...
  },
  "randomization_space": { ... },
  "output_chain": { ... },
  "midi_mappings": [ ... ]
}
```

### Loading behavior
Validates format and format_version. Unknown fields preserved. Missing fields fall back to defaults with a debug log. Parse errors trigger a dismissible dialog with file path and error location.

Version migration functions run in order for older presets.

### File locations
Factory presets (read-only):
- **Windows**: `C:\Program Files\Common Files\VST3\Wubforge.vst3\Contents\Resources\Presets\Factory\`
- **macOS**: `/Library/Audio/Plug-Ins/VST3/Wubforge.vst3/Contents/Resources/Presets/Factory/`

User presets (writable, auto-created):
- **Windows**: `%USERPROFILE%\Documents\Wubforge\Presets\User\`
- **macOS**: `~/Documents/Wubforge/Presets/User/`

Additional folders registered via Preferences → Preset Folders → Add Folder.

Folder structure is arbitrary. Sub-folders become the browser tree.

Scanned on startup and on manual Refresh. File watchers optional and off by default.

### Preset browser overlay
Same as Ascent/Descent: left tree, center list, right preview, search box at top, bottom bar with Load/Cancel/Reveal/Delete/Duplicate. Preview includes an Audition button that triggers a bar of the sound using the currently selected key/scale/BPM.

Browser closes on preset load, Escape, or click-outside.

### Save / Save As
Same behavior. Save As dialog has fields for Name, Category, Subcategory, Author, Tags, Description, Destination Folder, plus checkboxes for "Include MIDI mappings", "Include pattern" (Pattern Mode only), and "Bake preview waveform" (renders 2 bars for the browser thumbnail).

###