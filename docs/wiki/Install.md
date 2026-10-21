# Install, Update, Uninstall

Interference has no installer, runtime, licence server or internet dependency. The C++ runtime is linked statically, so no Visual C++ Redistributable is needed.

## Requirements

- Windows 10 or 11, 64-bit, x64 CPU with SSE2. This is the only tested build.
- A 64-bit VST3 host, or the stand-alone app.
- macOS (AU) is in the CMake source but has not been built or tested for 1.0.

## Install (Windows)

1. If Windows blocks the ZIP, right-click it, Properties, tick *Unblock*, OK. Unzip `Interference-1.0.0-win64.zip`.
2. Copy the **folder** `Interference.vst3` to `C:\Program Files\Common Files\VST3\` (admin rights needed). Copy the whole bundle, not just the DLL inside.
3. Rescan plug-ins in your DAW. It shows as an audio effect by *Interference Audio*.
4. Optional: put `Interference.exe` (stand-alone) anywhere. Its Options page picks the audio interface and MIDI inputs.

## Update

Close the DAW, replace `Interference.vst3`, reopen. Parameter IDs never change, and older states migrate on load (see `CHANGELOG.md`).

## Uninstall

1. Delete `C:\Program Files\Common Files\VST3\Interference.vst3` (and `Interference.exe` if installed).
2. Optional data cleanup: `Documents\Interference\Presets\` (your presets), `Documents\Interference\Support\` (crash logs and troubleshooting), `%APPDATA%\Interference\Interference Options.settings`, and `%APPDATA%\Interference\Interference.settings` (stand-alone device setup).
3. Rescan plug-ins or clear the DAW plug-in cache.

Rendered and recorded WAVs are saved where you chose and are never deleted by the plug-in.

## Where files live

| What | Where | Written when |
|---|---|---|
| User presets | `Documents\Interference\Presets\*.ifxpreset` | Save / Save As |
| Plug-in options | `%APPDATA%\Interference\Interference Options.settings` | Changing Options, closing the editor. Deleted by Hard reset |
| Stand-alone device setup | `%APPDATA%\Interference\Interference.settings` | Stand-alone app only. Not touched by Hard reset |
| Renders and recordings | Chosen in the file dialog (remembered) | Render WAV, Record |
| Crash logs | `Documents\Interference\Support\Interference_CrashLog_<date>.log` | Only while *Create log file on crash* is ticked |
| Troubleshooting files | `Documents\Interference\Support\Interference_Troubleshooting_<date>.txt` | Only on *Export troubleshooting file* |

If Windows redirects Documents to OneDrive, the Interference folders follow it there.
