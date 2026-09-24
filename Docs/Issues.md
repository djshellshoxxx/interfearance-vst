# Issue list

Severity: **S1** crash, data loss or unsafe level · **S2** wrong behaviour of a core feature · **S3** minor, cosmetic
or a documented limitation. Regression evidence names the automated test that guards the fix (see `QA.md`).

## Open

| # | Sev | Issue | Status / workaround |
|---|---|---|---|
| O1 | S2 | **No DAW load test.** The VST3 passes pluginval and the headless host tests, but it has not been loaded in a real DAW on this machine (none installed). | Release gate. Steps are in `QA.md` → *Unverified gates*. |
| O2 | S3 | **No empirical calibration.** The model ranges are listening-test starting points, not fitted to recordings. | Release gate for any "measured realism" claim. The product is described as research-informed. |
| O3 | S3 | **Real-time host soak and memory check not run.** The offline 1-hour soak passes: 60 min of 48 kHz stereo worst-case audio with continuous automation, rendered in 437 s, 5-min segments 33–48 s with no upward drift, peak −5.6 dBFS (`Perf.soakOneHourOfAudioNoDriftFiniteBounded`, see `QA.md`). Still unverified: an hour of real-time playback in a host, and process memory over that time. | Run a 1-hour playback in a DAW while watching memory in Task Manager; see `QA.md`. |
| O4 | S3 | Hosts that do not pass MIDI to audio effects cannot play gestures from notes. | By design: the GUI, the Trigger A/B automation and the Gesture Lane cover every MIDI function. |
| O5 | S3 | Offline *Render WAV* uses its own tempo field, not the host tempo map, and does not include host audio. | Documented export limitation. To process track audio, use the host's bounce or *Record*. |
| O6 | S3 | Bit-identical output is guaranteed only at the same sample rate and plug-in version. | Documented in the manual (Seed, Lock and New Take). |
| O7 | S3 | The crash log's stack trace is best effort: a badly corrupted process may only record the exception code. Fast-fail terminations (security-check failures, heap corruption detected by Windows) and crashes a host catches itself cannot be logged at all. | Documented in Help → Debug. |
| O8 | S2 | **Placeholder homepage and support e-mail.** Help, the troubleshooting file and the plug-in metadata use `https://interference-audio.example` and `support@interference-audio.example` (`Source/UI/HelpContent.h`, `CMakeLists.txt`). | Release gate: the publisher must supply the real address and URL before release. |
| O9 | S3 | *Render WAV* replays the lane and the slot gestures; it does not capture live illustration drags, knob moves or host automation. | Documented in the manual (Render and record). Program the gesture in the lane, or use *Record*. |
| O10 | S3 | The Details "through" meter shows the healthy reference R, not the contact-affected path, and a Cable slot's "electrical" meter includes its mechanical signal. | Needs per-slot C0 and electrical-only peaks from the engine telemetry. |
| O11 | S3 | No per-slot Solo / comparison audition. | Use A/B compare, or bypass the other slot with its On button. |
| O12 | S3 | **Environment models are research-informed, not measured.** Ground-loop currents, ground-voltage differences, interface rejection, dimmer rise times and 2G burst levels are starting points taken from the literature in `research/ElectricalInterference.md`, not fitted to recordings. The loop is modelled as one loop in series with the input, before both slots. Only the Loose Jack *sleeve* (plug position) and a damaged Cable Handling *shield* change its resistance: a sideways jack wiggle at a seated position does not. The 2G phone is 2G/GSM behaviour only. | Documented in the manual (Environment). Same calibration gate as O2 for any "measured" claim. |

## Fixed (1.0.0 development)

| # | Sev | Issue | Repro | Fix | Regression evidence |
|---|---|---|---|---|---|
| F1 | S2 | Bypass and slot bypass were not bit-exact after switching. | Toggle bypass, compare with the dry input. | Fixed-length 10 ms linear declick ramps that land exactly on 0 or 1 (d456172). | `Engine.pluginBypassIsDry`, `Engine.bothSlotsBypassedIsExactIdentity`, `Engine.oneSlotBypassedIsIdentityForThatSlot` |
| F2 | S2 | Studio off did not return exactly C. | Studio on → off, then compare W with C. | Exact-at-1 crossfade, and C is used directly when Studio is off (d456172). | `Engine.studioBypassReturnsExactlyC` |
| F3 | S1 | Resonator feedback could build excessive level. | High Q, feedback 0.9. | Modes normalised to 0 dB peak; output gain min(Q, 8) through a soft clip; loop gain includes it (d456172). | `Studio.resonatorRingsThenDecaysNoSelfOscillation`, `Engine.extremeParameterFuzzIsBoundedAndFinite` |
| F4 | S2 | A dirty pot held still could scratch without vibration. | Dirt 50 %, knob still. | Only Dirt > 70 % with Vibration stays intermittent while still (d456172). | `DirtyPot.heldStillStopsScratch`, `DirtyPot.severeDirtUnderVibrationMayStayIntermittent` |
| F5 | S2 | New Take and bar re-keying moved worn arcs and weak spots. | New Take, look at the pot illustration. | Fault maps derive from the Seed only (fecf43a). | `Pattern.evolveKeepsPositions`, `Engine.lockedSeedBounceIsNullIdentical` |
| F6 | S2 | A MIDI one-shot (note 62) started at the block start, not on its note-on sample. | Note 62 in the middle of a large block. | Sample-accurate start (fecf43a). | `Pattern.midiOneShotPatternAndNoteOffPolicy` |
| F7 | S3 | The default lane division was 1/16T instead of 1/16. | New instance. | Default index 3 (fecf43a). | `Core.paramTableIsConsistent` |
| F8 | S3 | The GUI reset test failed on log-skewed parameters (float normalisation round trip). | `Host.resetRandomizeAndAB`. | Relative tolerance (780f1f6). | `Host.resetRandomizeAndAB` |
| F9 | S3 | Output strip controls spilled outside the editor at the minimum size of 1000×700. | Resize to the minimum. | Compact output-strip layout (780f1f6). | `Gui.editorOpensRendersAndResizes` |
| F10 | S3 | Compiler warnings C4458/C4459/C4244 (shadowing, conversions). | Build. | Renamed and explicitly cast (5cd5da2). | Warning-free Release build (`QA.md`) |
| F11 | S3 | Help → Debug was a placeholder dialog. | Press Debug…. | Real debug window, crash log, troubleshooting export and hard reset. | `Gui.debugToolsCrashLogHardResetAndSecretPixel` |
