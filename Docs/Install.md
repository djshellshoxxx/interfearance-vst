# Install, update, uninstall

Interference has no installer, runtime, licence server or internet dependency. The release ZIP contains the
plug-in bundle, the stand-alone app, these documents and the third-party licence texts. The C++ runtime is linked
statically, so no Visual C++ Redistributable needs to be installed. `SIGNATURE.txt` in the ZIP says whether the
binaries are code-signed, and by whom.

## Requirements

- Windows 10 or 11, 64-bit, and an x64 CPU with SSE2. This is the tested build; see `QA.md` for what was verified.
- A 64-bit VST3 host, or the stand-alone app.
- The macOS/AU build is supported by the source (`CMakeLists.txt`) but has **not been built or tested** for 1.0.

## Install (Windows)

1. Windows may block files from a downloaded ZIP. Right-click the ZIP, choose Properties, tick *Unblock*, then OK.
   Then unzip `Interference-1.0.0-win64.zip`.
2. Copy the **folder** `Interference.vst3` to `C:\Program Files\Common Files\VST3\` (administrator rights needed).
   Copy the whole bundle folder, not just the DLL inside it.
3. Start your DAW and rescan plug-ins. Interference appears as an audio effect by *Interference Audio*.
4. Optional: put `Interference.exe` (stand-alone) anywhere, for example `C:\Program Files\Interference\`. Its
   Options page chooses the audio interface and MIDI inputs.

## Update

Close the DAW, replace `Interference.vst3` with the new version, then reopen. Projects and presets keep working:
parameter IDs never change, and older states are migrated on load (see `CHANGELOG.md`). A locked seed sounds the same
in any PATCH release.

## Uninstall

1. Delete `C:\Program Files\Common Files\VST3\Interference.vst3` (and `Interference.exe` if you installed it).
2. Optional: delete your data.
   - `Documents\Interference\Presets\`: your user presets (`.ifxpreset`).
   - `Documents\Interference\Support\`: crash logs and troubleshooting files.
   - `%APPDATA%\Interference\Interference Options.settings`: the plug-in's options (tooltips, editor size, last
     render folder).
   - `%APPDATA%\Interference\Interference.settings`: the stand-alone app's audio / MIDI device setup (only if you
     used the stand-alone app).
3. Rescan plug-ins in the DAW, or clear its plug-in cache.

Rendered and recorded WAV files are saved wherever you chose (default: your Music folder) and are never deleted by
the plug-in.

## Where files live

| What | Where | Written when |
|---|---|---|
| User presets | `Documents\Interference\Presets\*.ifxpreset` | Save / Save As |
| Options (plug-in and stand-alone) | `%APPDATA%\Interference\Interference Options.settings` | Changing Options, closing the editor; deleted by Hard reset |
| Stand-alone device setup | `%APPDATA%\Interference\Interference.settings` | Only by the stand-alone app (audio interface, MIDI inputs); not touched by Hard reset |
| Renders / recordings | chosen in the file dialog (remembered) | Render WAV…, Record |
| Crash logs | `Documents\Interference\Support\Interference_CrashLog_<date>.log` | Only while *Create log file on crash* is ticked |
| Troubleshooting files | `Documents\Interference\Support\Interference_Troubleshooting_<date>.txt` | Only when you press *Export troubleshooting file* |

Presets contain no paths, device names or audio. Nothing is written during ordinary processing.

`Documents` means your Windows Documents folder wherever it really is. If Windows redirects it to OneDrive (for
example `C:\Users\<you>\OneDrive\Documents`), the Interference folders follow it there.

**Moving the data folder (portable use, tests).** If the environment variable `IFX_DATA_ROOT` is set to an absolute
folder, presets go to `<IFX_DATA_ROOT>/Presets` and support files to `<IFX_DATA_ROOT>/Support`, and the options move to
`<IFX_DATA_ROOT>/Options/Interference Options.settings`, instead of the locations above. The test suites and CI use this so
they never touch your Documents folder.

## Troubleshooting

**The plug-in does not appear in the DAW**
- Check the bundle is `C:\Program Files\Common Files\VST3\Interference.vst3` (a folder), not inside a VST2 folder.
- Check the host scans that folder, then rescan or clear its plug-in cache. The host must be 64-bit.
- Windows may block files unzipped from the internet: right-click the ZIP → Properties → *Unblock* before unzipping
  (see Install, step 1).

**Presets do not show in the browser**
They must be `.ifxpreset` files directly inside `Documents\Interference\Presets` (use *Options → Open preset folder in
Explorer*). Or load one from anywhere with *Menu → Open*.

**No sound or no faults**
Use *Why quiet?* on each slot, and see *Help → Debug*, which lists obvious misconfigurations. A healthy, still
circuit with no hum is silent by design.

**Crashes**
Open *Help → Debug…*, tick **Create log file on crash**, then reproduce the problem. Afterwards, send the newest
`.log` file, a troubleshooting file (*Export troubleshooting file*) and a description of what you were doing to
support. If the plug-in keeps misbehaving, **Reset all settings to default (hard reset)** clears MIDI mappings, A/B
and options and deletes the plug-in's options file (`Interference Options.settings`). Presets, audio and the
stand-alone app's device setup are kept.

The log catches access violations and other unhandled exceptions, and aborts. It chains to the host's own handlers,
so the host still sees the crash. Some crashes cannot be logged. Fast-fail terminations (security-check failures,
heap corruption detected by Windows) end the process before any handler runs, and some hosts catch plug-in crashes
themselves. Then the log simply stops at the last recorded event: send it anyway.
