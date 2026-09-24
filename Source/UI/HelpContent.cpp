#include "HelpContent.h"

namespace ifx
{
namespace HelpContent
{

namespace
{
    juce::Array<Section> build()
    {
        juce::Array<Section> s;
        const juce::String nl = "\n";

        s.add ({ "Overview",
            juce::String ("Interference ") + IFX_VERSION_STRING + nl + nl
            + "Interference turns faulty analog signal paths into an audio effect and a playable sound source. "
              "Four hardware sources are modeled: Loose Jack, Exposed Plug Touch, Cable Handling and Dirty Pot. "
              "Load any one source, or any two different sources in slots A and B.\n\n"
              "Every audible fault has a cause: movement, wear, pressure, touch or an explicitly enabled environment "
              "(hum) or excitation. A healthy, still, shielded circuit with no hum is silent. Controls describe what you "
              "do to the hardware (Move Plug, Touch, Shake Cable, Turn Knob) rather than abstract noise amounts.\n\n"
              "The models are research-informed and tunable, not measurements of specific devices: numeric limits are "
              "listening-test starting points (see the manual's research notes).\n\n"
              "Three modes share the same engine:\n"
              "  Hardware - an insert effect: subtle hum, resistive loss, scratch or an occasional fault.\n"
              "  Play - MIDI notes, the GUI, automation or the Gesture Lane disturb the hardware in time.\n"
              "  Create - an internal excitation reveals faults on a silent track; render one-shots and loops to WAV." });

        s.add ({ "Quick start",
            "Hardware (subtle coloration)\n"
            "  1. Insert Interference on a track, choose Loose Jack (click it in the source row).\n"
            "  2. Details > Source circuit: Passive Pickup for guitar, Buffered/Line for everything else.\n"
            "  3. Keep Move Plug near 90-100 %, a little Wiggle and Wear. Seat the plug fully (100 %) to cure the faults.\n\n"
            "Play (rhythmic faults)\n"
            "  1. Load Loose Jack in A and Cable Handling in B, select Play.\n"
            "  2. In the Gesture Lane, click steps to add gestures (drag up/down for intensity), turn Lane On.\n"
            "  3. Start the host transport. Gestures start on the grid; the crackles inside them stay physical.\n"
            "  4. Lock the Seed (header) and bounce twice: the two files are identical.\n\n"
            "Create (one-shots and loops)\n"
            "  1. Select Create, pick an Excitation (Noise, Buzz, Bias Impulse or Test Tone).\n"
            "  2. Play the slots with Play buttons, MIDI, or the lane; monitor 'Injected Artifacts' in Routing to hear only the faults.\n"
            "  3. Render WAV... chooses bars or seconds, format and file. New Take keeps timing but changes the micro-events." });

        s.add ({ "The window",
            "Header: LED (output level: dark = silence, white = 0 dB, red = over 0 dBFS), menu (Save, Save As, Open, Options, "
            "Open preset folder), preset browser with a modified marker (*), A/B compare, Reset, Randomize, Help (?) and the gear (Options).\n\n"
            "Mode row: Hardware / Play / Create, the four sources (the row shows which slot each is in and the two-source limit), "
            "Seed, Lock and New Take, Natural / Expressive.\n\n"
            "Slots: cards A and B (source, one-line physical cause, activity meter, gesture, enable, disturbance, Play, Why quiet?). "
            "Below each card is its illustration. Illustrations draw the engine's actual state and are controls:\n"
            "  Loose Jack - drag the plug left/right (Move Plug); move up/down to wiggle it.\n"
            "  Exposed Plug Touch - press and hold on the finger; contact grows while held, drag down to press harder; release returns.\n"
            "  Cable Handling - drag along the cable to set the bend point; fast drags shake it.\n"
            "  Dirty Pot - drag to turn; double-click to show or hide the interior (track, wiper, worn arc).\n"
            "Every illustration has redundant labelled knobs in the context panel on the right (keyboard: arrows adjust, Space plays the gesture).\n\n"
            "Context panel: the selected slot's controls. Details opens circuit mode, input stage, stereo, calibration and diagnostic meters "
            "(through signal, electrical injection, mechanical injection).\n\n"
            "Bottom tabs: Gesture Lane, Studio chain, Create, Routing. Output strip: Dry/Wet, Output, Monitor, Ceiling, Bypass and meters." });

        s.add ({ "Loose Jack",
            "A TS plug in a worn jack (one tip/sleeve relationship; switching, TRS or powered jacks behave differently).\n"
            "  Move Plug - insertion depth. 100 % = seated; the unstable zone is part way in; 0 % = withdrawn (input open).\n"
            "  Wiggle - lateral movement. Faults need movement through a worn or partly engaged zone.\n"
            "  Pressure - spring pressure: high = stable. Wear / Wear Position - how worn, and where along the travel.\n"
            "  Intermittency - clustering of eligible faults (cannot create faults on a healthy still contact).\n"
            "  Recovery - how quickly the contact snaps back. Short to Sleeve - interruptions that ground (quiet) instead of floating (hum).\n"
            "  Pop - bias/charge in the circuit; transitions discharge it. With zero Pop only the interrupted signal clicks.\n"
            "  Shield - how well an open input is shielded from hum.\n"
            "Contact changes gate the real signal: an open contact removes the through audio." });

        s.add ({ "Exposed Plug Touch",
            "An artistic, simplified input-coupling model (not an exact human-body circuit): a finger on a floating high-impedance "
            "tip couples the local mains field into the input.\n"
            "  Touch - contact area. Pressure - steadiness. Body - hum colour (round to bright), never a pitch.\n"
            "  Circuit Exposure - how unterminated the input is. Shield - shielding. Both really reduce pickup.\n"
            "  Pop - small charge click on touch and release. Hum (global) - strength of the mains field; with Hum off, touch only clicks." });

        s.add ({ "Cable Handling",
            "Two layers: electrical faults (conductor and shield) driven by flexing over a weak spot, and optional mechanical rub/taps "
            "heard only through an enabled microphonic path.\n"
            "  Bend - where the cable is flexed. Shake Cable - continuous shaking. Damage - broken conductor strands (interrupts signal).\n"
            "  Shield Damage - broken braid (admits hum while the conductor stays live). Mic - handling noise pickup; 0 = a sealed cable is silent.\n"
            "  Weak Spot - location of the fault; bend over it repeatedly to find the same crackle." });

        s.add ({ "Dirty Pot",
            "A rotary pot on a finite track, used as a Level or Tone control.\n"
            "  Turn Knob - wiper position (level/tone follows the Taper even with no dirt).\n"
            "  Taper - Linear, Log (A) or Reverse Log (C). Circuit - Level, or Tone: 250 k pot + 22 nF cap treble cut.\n"
            "  Dirt - roughness of the track. Wear Position / Worn Arc Width - where the dirty arc is (drawn on the illustration).\n"
            "  Scratch - prominence of scratch while the wiper moves over dirt. Holding still stops the scratch.\n"
            "  Circuit Bias - DC across the pot: makes scratch audible on silence (a powered circuit).\n"
            "  Vibration - only a severely dirty pot (Dirt > 70 %) stays intermittent while held still." });

        s.add ({ "Two slots, order and linking",
            "Slots A and B are processed in series: A then B, or B then A with Swap Order (Details). A downstream open contact "
            "interrupts the upstream source's artifacts; a downstream pot filters them. Two sources in series can interact, but this "
            "is not a physically exact model of one combined device.\n"
            "Picking a third source asks which slot to replace; nothing is dropped silently. Choosing the other slot's source in a slot "
            "moves it (the two swap). Bypassing a slot keeps its settings; its physical time keeps running silently, so un-bypass does not "
            "replay old events.\n"
            "Linked Gesture sends one movement to both slots; each keeps its own contacts, wear and random stream. Both slots share one "
            "mains phase and a global noise budget, so two exposed paths never make two independent full-scale hums." });

        s.add ({ "Gestures, MIDI and automation",
            "Gestures: Wiggle, Slow Turn, Tap, Shake, Touch and Release (per slot), plus source-aware lane gestures "
            "(Jack Wiggle/Seat/Pull, Touch Tap/Hold/Release, Cable Bend/Shake/Tap, Pot Turn-to-Angle/Sweep/Hold).\n\n"
            "MIDI map (note numbers; names use middle C = C3; velocity = intensity):\n"
            "  36-39 (C1-D#1): slot A lane gesture 1, 2, 3, preset gesture.\n"
            "  48-51 (C2-D#2): slot B lane gesture 1, 2, 3, preset gesture.\n"
            "  62 (D3): play the lane once (one-shot); note-off stops it at once.\n"
            "  63 (D#3): Fill while held.\n"
            "  Any other note: both slots' preset gestures (and the Resonator note with Follow MIDI).\n"
            "  CC1 (mod wheel): continuous motion. Channel / poly aftertouch: pressure. CC120/123: release all.\n"
            "  Note-off releases held gestures (Touch and Release, Hold).\n\n"
            "Every parameter is automatable. Right-click any control for MIDI learn (then move a controller), clear mapping, reset or "
            "typing an exact value. Host automation arrives at block rate; the engine smooths it and keeps gestures sample-accurate.\n"
            "If a host does not send MIDI to effects, the GUI and automation provide the same controls." });

        s.add ({ "Gesture Lane / Machine Pattern",
            "Two lanes (A drives slot A, B drives slot B), 1-64 steps each; lengths can differ (A: 16 against B: 12). "
            "Each step: gesture, intensity, accent, ratchet (2-4 retriggers, coalesced with 'motion limit' if too fast), probability, "
            "duration, position target (e.g. aim at the worn region), microtiming, Studio send and choke.\n"
            "The lane schedules gesture STARTS only; the crackles inside stay physical. Swing and Humanize move starts; Density scales "
            "gesture probability; Evolve varies gesture choice per bar; Fill (or note 63) is a temporary denser variation.\n"
            "Keys: arrows move, Space toggles, +/- intensity, A accent, R ratchet, G gesture, P probability, C choke, Ctrl+C/V copy/paste, "
            "Shift+arrows shift, Ctrl+Shift+arrows rotate, Delete clear, Ctrl+Z/Y undo/redo.\n"
            "Timing follows the host position (bars, loops and seeks are deterministic). Without host transport press Run: the internal "
            "clock uses Internal BPM and the clock display says so." });

        s.add ({ "Seed, Lock and New Take",
            "Every random decision is seeded from the Seed, the Take, the source and the absolute musical position. "
            "With Lock on (default), the same host position and state produce identical audio for any buffer size, bounce or loop. "
            "Unlocked, each transport start draws a new take. New Take keeps positions, timing and the fault maps but changes the "
            "micro-events. The worn arcs and weak spots depend on the Seed only." });

        s.add ({ "Studio chain and routing",
            "Studio processing happens after the modeled hardware and is labelled Studio (never a cable property). "
            "Studio source: Artifacts (A, injected hum/pops/handling), Breaks (D, the fault's change to the signal) or Both.\n"
            "Fixed order: EQ > Contour > Drive > Crush > Resonator > Rhythm Gate > Echo, each with on/off and wet. "
            "Feedback is a bounded local loop from after the Resonator back into Drive, zero by default, with Kill Feedback. "
            "Rhythm Gate never gates the dry signal or the unaffected hardware path.\n"
            "Raw Hardware bypasses Studio, feedback and excitation (keep excitation with Keep Excitation).\n"
            "Named buses: x dry input, R healthy reference, C0 contacts without injection, C full core, D = C0 - R, A = C - C0, "
            "P Studio output, W = R + U + P (the wet signal), y final output. Monitor chooses what leaves the plugin; it never changes "
            "the model. Latency is always 0 samples." });

        s.add ({ "Render and record",
            "Render WAV... renders the internal excitation / gesture path with a cloned engine and a fixed tempo (no host audio): "
            "1-16 bars or 0.1-30 s, 44.1/48/96 kHz, 16/24-bit PCM or 32-bit float, mono/stereo, optional tail and peak normalisation. "
            "A silent result is explained before anything is written; existing files are never overwritten without confirmation. "
            "File names follow Interference_<preset>_<seed>_<length>_<bpm>bpm.wav. An optional sidecar .txt stores seed and tempo.\n"
            "Record captures the chosen Capture Tap (Routing) while you play, through a lock-free buffer written by a background thread.\n"
            "For processing incoming track audio, the simplest route is the host's own bounce or record." });

        s.add ({ "Presets",
            "Factory categories: Repair Faults, Rhythmic, One-Shots, Subtle Texture, Wild (Expressive) and Machine Pattern. "
            "The menu offers Save, Save As, Open and 'Open preset folder'. A * after the name means the sound was changed since loading.\n"
            "User presets are .ifxpreset files in:\n"
            "  Windows: Documents\\Interference\\Presets\n"
            "  macOS: ~/Documents/Interference/Presets\n"
            "Copy .ifxpreset files into that folder to see them in the browser (or use Open from anywhere). "
            "Presets store all parameters, seed, lock, take, sources, the lane and the Natural/Expressive label; never paths or audio. "
            "Older presets load safely: missing values take defaults, unknown values are ignored, bad values are repaired.\n"
            "Reset returns every sound setting to default (MIDI mappings are kept). Randomize resets, then picks new safe settings." });

        s.add ({ "Natural and Expressive",
            "Natural (default) applies strict physical caps. Expressive allows faster gestures, denser bursts and louder excitation - an "
            "artistic extension, still causal, never measured hardware behaviour. Switching never rewrites your knob values; effective "
            "limits are shown on the controls." });

        s.add ({ "Options",
            "The gear opens Options: tooltips on/off, audio and MIDI devices (stand-alone app only; in a DAW the host owns the devices), "
            "MIDI mappings, and folder shortcuts ('open location in Explorer'). Options are stored per user, never in presets." });

        s.add ({ "Install, uninstall, troubleshooting",
            "Windows install: copy Interference.vst3 to C:\\Program Files\\Common Files\\VST3\\ and rescan plugins in your DAW. "
            "The stand-alone app (Interference.exe) can live anywhere.\n"
            "Uninstall: delete C:\\Program Files\\Common Files\\VST3\\Interference.vst3. Optional: delete Documents\\Interference (your presets) "
            "and %APPDATA%\\Interference (options).\n\n"
            "If the plugin does not appear: check it is in the VST3 folder above (not the VST2 folder), that the host scans that folder, "
            "rescan or clear the host's plugin cache, and that you use a 64-bit host. The plugin needs no installer, runtime or internet.\n"
            "If presets do not appear: they must be .ifxpreset files directly in Documents\\Interference\\Presets (use Options > Open preset folder).\n"
            "If there is no sound: use 'Why quiet?' on each slot. If a host denies MIDI to effects, use the GUI or automation.\n"
            "Debug tools (Help > Debug...):\n"
            "- Live data: every internal value, bus peaks, clock, contact states and events as you work, plus obvious "
            "misconfigurations (bypass on, no active source, Dry/Wet 0 %, lane without a clock...). Freeze pauses it.\n"
            "- Create log file on crash: off every time the plug-in loads. When ticked, a time-stamped .log file is written to "
            "Documents\\Interference\\Support; it starts with a troubleshooting file, records events while you work and, if the "
            "plug-in or host crashes, the error and a stack trace.\n"
            "- Export troubleshooting file: a light self-test plus every setting, audio and MIDI setup, version, licence and host "
            "(DAW) information, saved as a .txt file in the same folder. It helps you and support with minor problems.\n"
            "- Reset all settings to default (hard reset): much stronger than Reset - also clears MIDI mappings, A/B and options and "
            "deletes the options (cache) file. Presets and audio are kept.\n"
            "If the plug-in keeps crashing, send BOTH the newest crash .log and a troubleshooting file to "
            + juce::String (Branding::supportEmail) + " with a detailed description of what you were doing." });

        s.add ({ "About, licence and support",
            juce::String (Branding::productName) + " " + IFX_VERSION_STRING + nl
            + "Format: VST3 and stand-alone, Windows x64 (macOS/AU build supported by the source, untested).\n"
            + Branding::licenseSummary + nl
            + "Homepage: " + Branding::homepage + nl
            + "Support: " + Branding::supportEmail + nl
            + "No network access, telemetry or copy protection: everything runs offline.\n"
              "Research references (connector intermittency, cable capacitance, potentiometer laws, no-input mixing, antiderivative "
              "antialiasing) are listed in the manual; they motivate the model structure, not measured values." });
        return s;
    }
}

const juce::Array<Section>& sections()
{
    static const juce::Array<Section> s = build();
    return s;
}

} // namespace HelpContent
} // namespace ifx
