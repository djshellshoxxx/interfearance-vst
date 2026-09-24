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
            "The button between the cards shows the processing order (A -> B or B -> A) and swaps it. "
            "Below each card is its illustration. Illustrations draw the engine's actual state and are controls "
            "(hover for a tooltip, right-click for MIDI learn / reset / type a value of the main control). "
            "'motion limit reached' appears when a gesture could not complete in time and was merged with the next:\n"
            "  Loose Jack - drag the plug left/right (Move Plug); move up/down to wiggle it.\n"
            "  Exposed Plug Touch - press and hold on the finger; contact grows while held, drag down to press harder; release returns.\n"
            "  Cable Handling - drag along the cable to set the bend point; fast drags shake it.\n"
            "  Dirty Pot - drag to turn; double-click to show or hide the interior (track, wiper, worn arc).\n"
            "Every illustration has redundant labelled knobs in the context panel on the right (keyboard: arrows adjust, Space plays the gesture).\n"
            "Keyboard: Tab / Shift+Tab move focus in a fixed order - header, modes and sources, slot cards, illustrations, context "
            "panel, bottom tabs and the active panel, output strip - and stay inside an open overlay until it closes.\n"
            "Right-click a mode button or a slot's Play button for MIDI learn / reset / set value (Mode, Trigger A/B).\n\n"
            "Context panel: the selected slot's controls. A control that asks for more than the circuit or range policy allows shows a "
            "short yellow 'Limited' reason above it (in its tooltip while you adjust it) and, where a value is clamped, a yellow tick on "
            "its arc at the effective value; the stored value is never changed. Details opens the source circuit, input stage, trims, "
            "stereo, chaos, range policy and gesture linking, and diagnostic meters (healthy-reference level, electrical injection, "
            "mechanical injection) with the contact state; the drawer scrolls when the window is small. Mains, Buzz and Interference "
            "are on the Environment tab; the processing order is the button between the cards.\n\n"
            "Bottom tabs: Gesture Lane, Studio chain, Create, Routing, Environment. While an internal excitation is feeding the circuit, an "
            "'Excitation on' badge sits next to the tabs (click it to open Create). "
            "Output strip: Dry/Wet, Output, Monitor, Ceiling, Bypass and meters." });

        s.add ({ "Loose Jack",
            "A TS plug in a worn jack (one tip/sleeve relationship; switching, TRS or powered jacks behave differently).\n"
            "  Move Plug - insertion depth. 100 % = seated; the unstable zone is part way in; 0 % = withdrawn (input open).\n"
            "  Wiggle - lateral movement. Faults need movement through a worn or partly engaged zone.\n"
            "  Pressure - spring pressure: high = stable. Wear / Wear Position - how worn, and where along the travel.\n"
            "  Intermittency - clustering of eligible faults (cannot create faults on a healthy still contact).\n"
            "  Recovery - how quickly the contact snaps back. Short to Sleeve - interruptions that ground (quiet) instead of floating (hum).\n"
            "  Pop - bias/charge in the circuit; transitions discharge it. With zero Pop only the interrupted signal clicks.\n"
            "  Shield - how well an open input is shielded from hum.\n"
            "Contact changes gate the real signal: an open contact removes the through audio.\n"
            "A fully seated plug on a healthy spot is stable: Wiggle and Intermittency then show 'Contact stable here'. "
            "'Move toward worn area' (context panel with Details closed, or the Why quiet? dialog) slowly moves the plug to Wear "
            "Position. It stops at once if you or host automation move the plug." });

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
            "  Weak Spot - location of the fault; bend over it repeatedly to find the same crackle.\n"
            "A buffered / line source (Details > Source circuit) holds the conductor at low impedance, so a broken shield picks up "
            "about 34 dB less hum than with Passive Pickup.\n"
            "Handling noise (Mic above 0 only) has resonant rub and tap sounds plus a triboelectric part: friction and impact "
            "separate charge in the cable. On a Passive Pickup source it is loud and more Cable Load makes it darker; on "
            "Buffered/Line it is about 34 dB lower and Cable Load does not change it." });

        s.add ({ "Dirty Pot",
            "A rotary pot on a finite track, used as a Level or Tone control.\n"
            "  Turn Knob - wiper position (level/tone follows the Taper even with no dirt).\n"
            "  Taper - Linear, Log (A) or Reverse Log (C). Circuit - Level, or Tone: 250 k pot + 22 nF cap treble cut.\n"
            "  Dirt - roughness of the track. Wear Position / Worn Arc Width - where the dirty arc is (drawn on the illustration).\n"
            "  Scratch - prominence of scratch while the wiper moves over dirt. Holding still stops the scratch.\n"
            "  Circuit Bias - DC across the pot: makes scratch audible on silence (a powered circuit).\n"
            "  Vibration - only a severely dirty pot (Dirt > 70 %) stays intermittent while held still." });

        s.add ({ "Two slots, order and linking",
            "Slots A and B are processed in series: A then B, or B then A with Swap Order (the order button between the slot cards). "
            "A downstream open contact "
            "interrupts the upstream source's artifacts; a downstream pot filters them. Two sources in series can interact, but this "
            "is not a physically exact model of one combined device.\n"
            "Picking a third source asks which slot to replace; nothing is dropped silently. Choosing the other slot's source in a slot "
            "moves it (the two swap). Bypassing a slot keeps its settings; its physical time keeps running silently, so un-bypass does not "
            "replay old events.\n"
            "Linked Gesture sends one movement to both slots; each keeps its own contacts, wear and random stream. Both slots share one "
            "mains phase and a global noise budget, so two exposed paths never make two independent full-scale hums." });

        s.add ({ "Environment: hum, ground loop, dimmer, phone",
            "The Environment tab sets the rig's electrical surroundings, shared by both slots (one mains phase). All of it is off or "
            "quiet by default. Greyed controls do nothing in the current setting; the tooltip says why.\n\n"
            "Mains and Hum: the 50/60 Hz field. Only exposed paths pick it up (open or partly inserted jack, touched tip, broken "
            "shield). Resistive / magnetic pickup keeps a round hum; capacitive pickup (finger, floating tip) is brighter. Never "
            "retuned to the song.\n"
            "Buzz Band and Buzz Source: Supply = rectifier pulses twice per mains cycle (100/120 Hz buzz). Dimmer = a light dimmer's "
            "switching spikes twice per cycle, loudest near half brightness, silent fully on or off. Dimmer Kind: Cheap Wall "
            "switches in microseconds (harsh), Stage and TV Studio chokes are softer and darker.\n"
            "Interference: the RF field strength at the gear, picked up through exposed paths like hum. The audio circuit detects "
            "it by a square law, so -10 dB of RF is -20 dB of noise (50 % is about 12 dB quieter than 100 %).\n"
            "Transmitter (greyed while Interference is 0; switching fades): AM Station = a distant broadcast, a continuous carrier "
            "with slow fading and unintelligible whispered syllables at 2.5-7 per second (up to about 4.5 kHz). Two-Way Radio "
            "(default) = keyed bursts of 30-250 ms with key-down / key-up thumps (voice band, about 3 kHz). Motor Sparks = "
            "commutator crackle at 60-250 sparks per second (brighter, about 7 kHz).\n"
            "Ground loop: Other Gear makes current flow through the cable shield; current x shield resistance is hum in series with "
            "the signal, entering before the slots. 2-prong gear leaks a small current (slightly buzzy hum); Grounded gear is driven "
            "by the ground-voltage difference set by Outlets (round hum). Cable Run and Cable Shield set the resistance. Interface: "
            "Unbalanced passes it all, Balanced removes about 30 dB, an Isolator about 70 dB at mains (a thin high buzz remains), an "
            "Input Transformer about 100 dB. The readout is the measured loop hum after the interface. Cable Run is a slider; the "
            "ground-loop readout sits at the right of the ground-loop heading.\n"
            "2G Phone: a GSM phone's 217 Hz bursts detected by the circuit. Check-in = a few bursts, Incoming = handshake clusters, "
            "In Call = continuous. Phone Distance: falls off steeply beyond a metre or two. (2G only: modern networks sound different.)\n\n"
            "Playing it:\n"
            "  With a ground loop on, shake or bend a cable with a damaged shield, or move a jack in and out at the sleeve (Seat / "
            "Pull): that contact resistance is part of the loop, so the hum breathes with the movement (a sideways wiggle of a "
            "seated plug does not touch the sleeve); pull the plug out and the loop breaks.\n"
            "  Sweep Dimmer (automation or MIDI learn) with Buzz Source = Dimmer: a mains-locked wah, peaking at half brightness; the "
            "pitch never moves.\n"
            "  Automate Interface: an honest filter on the loop noise, from full hum to a thin isolator buzz.\n"
            "  The phone's inner rhythm is fixed by the network timing; its state decides when it bursts (switch to Incoming on a bar "
            "line, automate Distance for swells)." });

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
            "typing an exact value. Host automation arrives once per audio block: levels and gains (Dry/Wet, Dry Trim, Output, "
            "Studio Level, Ceiling, Studio stage amounts, Drive gains) are smoothed over about 10 ms; knob positions (Move Plug, "
            "Turn Knob, Bend, Touch) move the modelled hardware at its own finite speed, so fault events are never smeared; "
            "gestures and MIDI events land on their exact sample.\n"
            "If a host does not send MIDI to effects, the GUI and automation provide the same controls." });

        s.add ({ "Gesture Lane / Machine Pattern",
            "Two lanes (A drives slot A, B drives slot B), 1-64 steps each; lengths can differ (A: 16 against B: 12). "
            "Each step: gesture, intensity, accent, ratchet (2-4 retriggers, coalesced with 'motion limit' if too fast), probability, "
            "duration, position target (e.g. aim at the worn region), microtiming, Studio send and choke. "
            "'Gap' (step menu, Loose Jack and Dirty Pot lanes) pulls the jack open or turns the pot to minimum for one step "
            "(Dirty Pot with the Level circuit only: with the Tone circuit the minimum just darkens the sound): "
            "a real gesture, not a hidden gate (use the Studio Rhythm Gate for hard gating).\n"
            "The two lanes share one clock, so A: 16 against B: 12 repeats every 48 steps; the clock line shows the position "
            "in that combined cycle (at most 64 x 63 = 4032 steps).\n"
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
            "Stages:\n"
            "  EQ - high-pass, low-pass, tilt around 1 kHz and a broad peak on the Studio signal.\n"
            "  Contour - boosts or cuts transients (clicks, pops) separately from sustained material (hum body).\n"
            "  Drive - antialiased saturator: Soft, Hard or Asymmetric curve, input Drive, Drive Output, tone and wet; DC is blocked.\n"
            "  Digital Crush - bit depth and sample-and-hold rate (intentional aliasing), optional seeded dither.\n"
            "  Auto Match (Drive page) - slow, bounded gain compensation for Drive and Crush.\n"
            "  Resonator - two tuned, damped modes (frequency or MIDI note, ratio, decay, bounded feedback); never rings from silence.\n"
            "  Rhythm Gate - Through, Pulse (keyed by steps, MIDI notes, sidechain or the signal's envelope) or Choke (closes on "
            "choke steps; can also kill feedback and clear Echo). It only gates the Studio branch.\n"
            "  Echo - short filtered delay, free or tempo-synced, with bounded repeats and Kill Tail.\n"
            "  Feedback - Resonator output back into Drive through HP/LP filters and a delay; its amount is clamped for stability "
            "(the panel shows 'Limited by loop stability').\n"
            "Each stage has a small meter under its button: upper bar = input peak, lower bar = output peak. Studio controls that do "
            "nothing in the current setting are greyed, with the reason in their tooltip.\n"
            "Raw Hardware bypasses Studio, feedback, excitation and the hidden extra (keep excitation with Keep Excitation), so the "
            "wet signal is exactly the modelled hardware C.\n"
            "Ceiling (output strip) is an explicit peak ceiling: an instant-attack, stereo-linked gain rider with about 45 ms release "
            "and a final clamp at the ceiling level. It is audible limiting when it acts; off means no limiting.\n"
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
            "Factory categories: Repair Faults, Rhythmic, One-Shots, Subtle Texture, Wild (Expressive), Machine Pattern and Environment (ground loop, dimmer and phone). "
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
            "artistic extension, still causal, never measured hardware behaviour. Switching never rewrites your knob values. "
            "When a setting asks for more than the policy or circuit allows, the control says so: e.g. Excitation Level above the "
            "policy cap shows 'Limited by Natural cap' and a yellow tick at the level actually used (the badge next to the bottom "
            "tabs adds '(capped)'); Feedback above the stable loop gain shows 'Limited by loop stability'; a healthy seated jack "
            "shows 'Contact stable here'." });

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
            "plug-in or host crashes, the error and a stack trace. Some crashes cannot be logged: fast-fail terminations "
            "(security-check failures, heap corruption detected by Windows) end the process before any handler runs, and some "
            "hosts catch plug-in crashes themselves. In those cases the log simply stops; send it anyway with your description.\n"
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
