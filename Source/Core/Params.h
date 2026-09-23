#pragma once

// Single source of truth for every host parameter. IDs are stable and versioned
// (never renumber or rename a shipped ID; append new ones). The DSP reads a plain
// float snapshot indexed by P::xxx, so Core/DSP never depend on JUCE.
//
// Kinds: F = continuous float, B = bool, C = choice (index), I = integer.
// Skew:  0 = linear, 1 = logarithmic (frequencies, capacitance, time).

namespace ifx
{

enum class ParamKind { Float, Bool, Choice, Int };

#define IFX_PARAMS(X) \
/* --- global / mode --- */ \
X(mode,        "mode",        "Mode",              C, 0, 2, 0, 0, "", "Hardware|Play|Create", "Hardware: subtle faults on a track. Play: gestures, MIDI and the lane disturb the hardware. Create: internal excitation lets a silent track make sounds.") \
X(policy,      "policy",      "Range Policy",      C, 0, 1, 0, 0, "", "Natural|Expressive", "Natural keeps strict physical caps. Expressive allows faster gestures, denser bursts and louder excitation - still causal, never measured hardware.") \
X(bypass,      "bypass",      "Bypass",            B, 0, 1, 0, 0, "", "", "Plugin bypass: time-aligned dry input with a short declick.") \
X(slotAOn,     "slotA_on",    "Slot A Enable",     B, 0, 1, 1, 0, "", "", "Enable slot A. Bypassed slots are exact identity and keep their settings.") \
X(slotBOn,     "slotB_on",    "Slot B Enable",     B, 0, 1, 1, 0, "", "", "Enable slot B. Bypassed slots are exact identity and keep their settings.") \
X(slotALevel,  "slotA_level", "Slot A Disturbance",F, 0, 1, 1, 0, "%", "", "How strongly gestures disturb slot A. Does not change whether an event is physically possible.") \
X(slotBLevel,  "slotB_level", "Slot B Disturbance",F, 0, 1, 1, 0, "%", "", "How strongly gestures disturb slot B. Does not change whether an event is physically possible.") \
X(swapOrder,   "swap",        "Swap Order",        B, 0, 1, 0, 0, "", "", "Process slot B before slot A. The downstream source can interrupt or filter the upstream source's artifacts.") \
X(linkGesture, "link",        "Linked Gesture",    B, 0, 1, 0, 0, "", "", "Send one movement envelope to both slots; each keeps its own contacts, wear and random stream.") \
X(gestureA,    "slotA_gest",  "Slot A Gesture",    C, 0, 4, 0, 0, "", "Wiggle|Slow Turn|Tap|Shake|Touch and Release", "Gesture played by the slot A trigger, MIDI and plain lane steps. The loaded source decides how it acts.") \
X(gestureB,    "slotB_gest",  "Slot B Gesture",    C, 0, 4, 2, 0, "", "Wiggle|Slow Turn|Tap|Shake|Touch and Release", "Gesture played by the slot B trigger, MIDI and plain lane steps. The loaded source decides how it acts.") \
X(trigA,       "trigA",       "Trigger A",         B, 0, 1, 0, 0, "", "", "Momentary: play slot A's gesture now.") \
X(trigB,       "trigB",       "Trigger B",         B, 0, 1, 0, 0, "", "", "Momentary: play slot B's gesture now.") \
/* --- circuit / environment --- */ \
X(inputMode,   "inmode",      "Source Circuit",    C, 0, 1, 0, 0, "", "Buffered/Line|Passive Pickup", "Buffered/Line: low source impedance, cable load is negligible. Passive Pickup: a simplified pickup + cable RLC resonance.") \
X(cableLoad,   "cableload",   "Cable Load",        F, 100, 2000, 600, 1, "pF", "", "Cable capacitance loading a passive pickup: more capacitance lowers and softens the resonant peak. Minimal effect in Line mode.") \
X(preOn,       "pre_on",      "Input Stage Drive", B, 0, 1, 0, 0, "", "", "Optional overdriven input stage (antialiased). Off = clean.") \
X(preDrive,    "pre_drive",   "Input Drive",       F, 0, 24, 6, 0, "dB", "", "Gain into the input stage.") \
X(preHeadroom, "pre_head",    "Headroom",          F, 0, 18, 6, 0, "dB", "", "Input stage headroom before saturation.") \
X(inTrim,      "intrim",      "Input Trim",        F, -24, 12, 0, 0, "dB", "", "Level into the modeled hardware (wet path only).") \
X(mains,       "mains",       "Mains",             C, 0, 1, 0, 0, "", "50 Hz|60 Hz", "Local mains frequency for all hum pickup.") \
X(hum,         "hum",         "Hum",               F, 0, 1, 0.15f, 0, "%", "", "Strength of the mains field around the rig. Only exposed or unshielded paths pick it up.") \
X(buzz,        "buzz",        "Buzz Band",         F, 0, 1, 0, 0, "%", "", "Optional low supply/dimmer buzz in the environment (off by default).") \
X(rf,          "rf",          "Interference",      F, 0, 1, 0, 0, "%", "", "Optional sparse RF-like chatter from a nearby transmitter. A separate environmental source, not a property of touch.") \
X(stereo,      "stereo",      "Stereo Difference", F, 0, 1, 0.2f, 0, "%", "", "Small electrical differences between channels. Mechanical contact state is always shared.") \
X(chaos,       "chaos",       "Chaos",             F, 0, 1, 0.2f, 0, "%", "", "Variability of gesture shape within physical limits. Never changes physical laws.") \
X(humanize,    "humanize",    "Humanize",          F, 0, 1, 0.1f, 0, "%", "", "Seeded offset of gesture starts within a fraction of a step.") \
/* --- excitation --- */ \
X(excType,     "exc_type",    "Excitation",        C, 0, 4, 0, 0, "", "Off|Noise|Buzz|Bias Impulse|Test Tone", "Internal studio source that feeds the modeled circuit to reveal faults on a silent track. It is not a claim that a cable makes sound unaided.") \
X(excLevel,    "exc_level",   "Excitation Level",  F, -60, -6, -30, 0, "dB", "", "Internal excitation level (capped by the range policy).") \
X(keepExc,     "keep_exc",    "Keep Excitation",   B, 0, 1, 0, 0, "", "", "Keep internal excitation when Raw Hardware is on.") \
/* --- Loose Jack --- */ \
X(jkInsert,    "jk_insert",   "Move Plug",         F, 0, 1, 1, 0, "%", "", "How far the plug is inserted. 100% is fully seated; the unstable zone is part way in; 0% is withdrawn.") \
X(jkWiggle,    "jk_wiggle",   "Wiggle",            F, 0, 1, 0.1f, 0, "%", "", "Lateral movement of the plug. Movement through a worn or partly engaged zone causes crackle.") \
X(jkPressure,  "jk_press",    "Pressure",          F, 0, 1, 0.6f, 0, "%", "", "Spring pressure on the tip: high pressure = stable contact.") \
X(jkWear,      "jk_wear",     "Wear",              F, 0, 1, 0.25f, 0, "%", "", "How worn the contact surfaces are in the worn zone.") \
X(jkWearPos,   "jk_wearpos",  "Wear Position",     F, 0, 1, 0.8f, 0, "%", "", "Where along the insertion travel the worn zone sits.") \
X(jkIntermit,  "jk_intermit", "Intermittency",     F, 0, 1, 0.3f, 0, "%", "", "Clustering and chatter of eligible faults. Cannot create a fault on a healthy still contact.") \
X(jkRecovery,  "jk_recovery", "Recovery",          F, 0, 1, 0.6f, 0, "%", "", "How quickly a disturbed contact snaps back.") \
X(jkShort,     "jk_short",    "Short to Sleeve",   F, 0, 1, 0.2f, 0, "%", "", "Chance an interruption is a tip-to-sleeve short (goes quiet) instead of an open (can hum).") \
X(jkPop,       "jk_pop",      "Pop",               F, 0, 1, 0, 0, "%", "", "Bias/charge in the circuit. Transitions discharge it as a pop; with zero bias only the signal interruption clicks.") \
X(jkShield,    "jk_shield",   "Shield",            F, 0, 1, 0.8f, 0, "%", "", "How well the open input is shielded from hum.") \
/* --- Exposed Plug Touch --- */ \
X(tcTouch,     "tc_touch",    "Touch",             F, 0, 1, 0, 0, "%", "", "Finger contact area on the exposed tip.") \
X(tcPressure,  "tc_press",    "Pressure",          F, 0, 1, 0.5f, 0, "%", "", "Finger pressure: high pressure = steady coupling, low = fluttering contact.") \
X(tcBody,      "tc_body",     "Body",              F, 0, 1, 0.4f, 0, "%", "", "Coupling colour: low = round fundamental hum, high = bright harmonic buzz.") \
X(tcShield,    "tc_shield",   "Shield",            F, 0, 1, 0.2f, 0, "%", "", "Shielding of the exposed conductor.") \
X(tcExposure,  "tc_expose",   "Circuit Exposure",  F, 0, 1, 0.7f, 0, "%", "", "How high-impedance / unterminated the input is. Properly terminated inputs pick up little.") \
X(tcPop,       "tc_pop",      "Pop",               F, 0, 1, 0.2f, 0, "%", "", "Charge click when the finger makes or breaks contact.") \
/* --- Cable Handling --- */ \
X(cbBend,      "cb_bend",     "Bend",              F, 0, 1, 0.5f, 0, "%", "", "Where the cable is flexed along its length. Bend over the weak spot to find the crackle.") \
X(cbShake,     "cb_shake",    "Shake Cable",       F, 0, 1, 0, 0, "%", "", "Continuous shaking of the cable.") \
X(cbDamage,    "cb_damage",   "Damage",            F, 0, 1, 0.2f, 0, "%", "", "Broken strands of the signal conductor at the weak spot (interrupts the signal).") \
X(cbShieldDmg, "cb_shielddmg","Shield Damage",     F, 0, 1, 0.1f, 0, "%", "", "Broken shield braid at the weak spot (admits hum while the conductor stays live).") \
X(cbMic,       "cb_mic",      "Mic",               F, 0, 1, 0, 0, "%", "", "Microphonic pickup of handling noise. Zero = a sealed cable makes no handling sound.") \
X(cbWeakPos,   "cb_weakpos",  "Weak Spot",         F, 0, 1, 0.5f, 0, "%", "", "Location of the weak spot along the cable.") \
X(cbIntermit,  "cb_intermit", "Intermittency",     F, 0, 1, 0.4f, 0, "%", "", "Clustering of eligible faults at the weak spot.") \
/* --- Dirty Pot --- */ \
X(ptTurn,      "pt_turn",     "Turn Knob",         F, 0, 1, 0.75f, 0, "%", "", "Wiper position. Level/tone follows the taper; turning across the worn arc scratches.") \
X(ptTaper,     "pt_taper",    "Taper",             C, 0, 2, 1, 0, "", "Linear|Log (A)|Reverse Log (C)", "Nominal wiper law of the pot.") \
X(ptCircuit,   "pt_circuit",  "Circuit",           C, 0, 1, 0, 0, "", "Level|Tone", "Level: volume pot. Tone: guitar-style pot + capacitor treble cut.") \
X(ptDirt,      "pt_dirt",     "Dirt",              F, 0, 1, 0.3f, 0, "%", "", "Roughness and intermittent conductance of the dirty track.") \
X(ptWearPos,   "pt_wearpos",  "Wear Position",     F, 0, 1, 0.5f, 0, "%", "", "Centre of the worn arc on the track.") \
X(ptWearWidth, "pt_wearwidth","Worn Arc Width",    F, 0.02f, 0.4f, 0.12f, 0, "", "", "Width of the worn arc.") \
X(ptScratch,   "pt_scratch",  "Scratch",           F, 0, 1, 0.5f, 0, "%", "", "Prominence of scratch while the wiper moves over dirt.") \
X(ptBias,      "pt_bias",     "Circuit Bias",      F, 0, 1, 0, 0, "%", "", "DC across the pot. With bias the scratch is audible even on silence (labelled powered circuit).") \
X(ptVibration, "pt_vibe",     "Vibration",         F, 0, 1, 0, 0, "%", "", "External vibration. Only a severely dirty pot stays intermittent while held still.") \
/* --- Gesture lane / Machine Pattern --- */ \
X(laneOn,      "lane_on",     "Gesture Lane",      B, 0, 1, 0, 0, "", "", "Run the Gesture Lane / Machine Pattern. Off by default.") \
X(laneDiv,     "lane_div",    "Step Division",     C, 0, 6, 4, 0, "", "1/4|1/8|1/8T|1/16|1/16T|1/32|1/32T", "Length of one lane step.") \
X(swing,       "swing",       "Swing",             F, 0, 0.5f, 0, 0, "%", "", "Delays offbeat gesture starts. Crackles themselves are never quantized.") \
X(density,     "density",     "Density",           F, 0, 1, 1, 0, "%", "", "Probability scale of eligible gestures. Never unconditional noise.") \
X(evolve,      "evolve",      "Evolve",            F, 0, 1, 0, 0, "%", "", "Per-bar seeded variation of gesture choice and probability.") \
X(fill,        "fill",        "Fill",              B, 0, 1, 0, 0, "", "", "Momentary denser variation; returns on release.") \
X(patLevel,    "pat_level",   "Pattern Level",     F, 0, 1, 1, 0, "%", "", "Scales the force of every lane gesture.") \
X(studioSend,  "studio_send", "Pattern Studio Send",F, 0, 1, 1, 0, "%", "", "Scales per-step Studio sends.") \
X(bpm,         "bpm",         "Internal BPM",      F, 40, 240, 120, 0, "BPM", "", "Tempo used when the host provides none.") \
X(runInternal, "run_int",     "Run",               B, 0, 1, 0, 0, "", "", "Run the internal clock when the host transport is stopped or absent.") \
X(react,       "react",       "Sidechain React",   F, 0, 1, 0, 0, "%", "", "Sidechain transients trigger both slots' gestures (control only, never audio).") \
X(reactThresh, "react_thr",   "React Threshold",   F, -60, 0, -24, 0, "dB", "", "Sidechain onset threshold.") \
/* --- Studio chain --- */ \
X(studioOn,    "studio_on",   "Studio",            B, 0, 1, 0, 0, "", "", "Enable the Studio processing chain (post-hardware, labelled Studio).") \
X(studioSrc,   "studio_src",  "Studio Source",     C, 0, 2, 0, 0, "", "Artifacts|Breaks|Both", "Artifacts = injected A bus, Breaks = fault difference D, Both = A + D.") \
X(studioLevel, "studio_lvl",  "Studio Level",      F, -24, 12, 0, 0, "dB", "", "Output level of the Studio branch.") \
X(eqOn,        "eq_on",       "EQ",                B, 0, 1, 0, 0, "", "", "Artifact EQ.") \
X(eqHP,        "eq_hp",       "High-pass",         F, 20, 2000, 20, 1, "Hz", "", "EQ high-pass.") \
X(eqLP,        "eq_lp",       "Low-pass",          F, 1000, 20000, 20000, 1, "Hz", "", "EQ low-pass.") \
X(eqTilt,      "eq_tilt",     "Tilt",              F, -12, 12, 0, 0, "dB", "", "Tilt around 1 kHz.") \
X(eqPkFreq,    "eq_pkf",      "Peak Freq",         F, 100, 8000, 1000, 1, "Hz", "", "Broad peak frequency.") \
X(eqPkGain,    "eq_pkg",      "Peak Gain",         F, -12, 12, 0, 0, "dB", "", "Broad peak gain.") \
X(eqMix,       "eq_mix",      "EQ Wet",            F, 0, 1, 1, 0, "%", "", "EQ wet amount.") \
X(ctOn,        "ct_on",       "Contour",           B, 0, 1, 0, 0, "", "", "Transient contour.") \
X(ctAttack,    "ct_attack",   "Transient",         F, -12, 12, 0, 0, "dB", "", "Boost or cut transients.") \
X(ctSustain,   "ct_sustain",  "Sustain",           F, -24, 12, 0, 0, "dB", "", "Boost or cut sustained material (hum body).") \
X(ctMix,       "ct_mix",      "Contour Wet",       F, 0, 1, 1, 0, "%", "", "Contour wet amount.") \
X(drOn,        "dr_on",       "Drive",             B, 0, 1, 0, 0, "", "", "Studio saturator (antialiased). A Studio alteration, never a cable property.") \
X(drCurve,     "dr_curve",    "Drive Curve",       C, 0, 2, 0, 0, "", "Soft|Hard|Asymmetric", "Saturation curve.") \
X(drPre,       "dr_pre",      "Drive",             F, 0, 36, 12, 0, "dB", "", "Gain into the saturator.") \
X(drPost,      "dr_post",     "Drive Output",      F, -24, 6, -6, 0, "dB", "", "Gain after the saturator.") \
X(drTone,      "dr_tone",     "Drive Tone",        F, 0, 1, 0.5f, 0, "%", "", "Post-drive tone.") \
X(drMix,       "dr_mix",      "Drive Wet",         F, 0, 1, 1, 0, "%", "", "Drive wet amount.") \
X(drAuto,      "dr_auto",     "Auto Match",        B, 0, 1, 0, 0, "", "", "Bounded gain compensation for Drive and Crush.") \
X(crOn,        "cr_on",       "Digital Crush",     B, 0, 1, 0, 0, "", "", "Bit depth and sample-and-hold rate reduction (intentional aliasing).") \
X(crBits,      "cr_bits",     "Bits",              F, 2, 16, 8, 0, "bit", "", "Quantization depth.") \
X(crRate,      "cr_rate",     "Hold Rate",         F, 200, 48000, 8000, 1, "Hz", "", "Sample-and-hold rate.") \
X(crDither,    "cr_dither",   "Dither",            F, 0, 1, 0, 0, "%", "", "Seeded dither before quantization.") \
X(crMix,       "cr_mix",      "Crush Wet",         F, 0, 1, 1, 0, "%", "", "Crush wet amount.") \
X(rsOn,        "rs_on",       "Resonator",         B, 0, 1, 0, 0, "", "", "Two tuned damped modes excited by the Studio signal.") \
X(rsFreq,      "rs_freq",     "Res Freq",          F, 40, 4000, 220, 1, "Hz", "", "First mode frequency.") \
X(rsMidi,      "rs_midi",     "Follow MIDI",       B, 0, 1, 0, 0, "", "", "Tune the resonator to incoming MIDI notes.") \
X(rsRatio,     "rs_ratio",    "Mode Ratio",        F, 1, 4, 1.5f, 0, "x", "", "Second mode frequency ratio.") \
X(rsDecay,     "rs_decay",    "Res Decay",         F, 0.01f, 3, 0.4f, 1, "s", "", "Ring time.") \
X(rsFeedback,  "rs_fb",       "Res Feedback",      F, 0, 0.9f, 0, 0, "%", "", "Bounded extra ring. Never self-oscillates from silence.") \
X(rsMix,       "rs_mix",      "Res Blend",         F, 0, 1, 0.5f, 0, "%", "", "Resonator blend.") \
X(gtOn,        "gt_on",       "Rhythm Gate",       B, 0, 1, 0, 0, "", "", "Rhythmic gate on the Studio branch only; never gates the dry or hardware path.") \
X(gtMode,      "gt_mode",     "Gate Mode",         C, 0, 2, 1, 0, "", "Through|Pulse|Choke", "Through = open, Pulse = rhythmic, Choke = closes on choke steps.") \
X(gtKey,       "gt_key",      "Gate Key",          C, 0, 3, 0, 0, "", "Steps|MIDI|Sidechain|Envelope", "What opens the gate.") \
X(gtDiv,       "gt_div",      "Gate Division",     C, 0, 6, 3, 0, "", "1/4|1/8|1/8T|1/16|1/16T|1/32|1/32T", "Pulse length.") \
X(gtAttack,    "gt_attack",   "Gate Attack",       F, 0.5f, 50, 2, 1, "ms", "", "Gate opening time.") \
X(gtRelease,   "gt_release",  "Gate Release",      F, 5, 500, 60, 1, "ms", "", "Gate closing time.") \
X(gtDepth,     "gt_depth",    "Gate Depth",        F, 0, 1, 1, 0, "%", "", "How far the gate closes.") \
X(gtChokeFb,   "gt_chokefb",  "Choke Kills Feedback",B,0, 1, 0, 0, "", "", "A choke step also ramps the local feedback down.") \
X(gtChokeEcho, "gt_chokeecho","Choke Clears Echo", B, 0, 1, 0, 0, "", "", "A choke step also fades the Echo tail.") \
X(ecOn,        "ec_on",       "Echo",              B, 0, 1, 0, 0, "", "", "Short filtered Studio echo with a finite tail.") \
X(ecSync,      "ec_sync",     "Echo Sync",         B, 0, 1, 1, 0, "", "", "Tempo-synced echo time.") \
X(ecTime,      "ec_time",     "Echo Time",         F, 10, 1500, 250, 1, "ms", "", "Free echo time.") \
X(ecDiv,       "ec_div",      "Echo Division",     C, 0, 7, 3, 0, "", "1/32|1/16T|1/16|1/8T|1/8|1/8D|1/4|1/2", "Synced echo time.") \
X(ecFeedback,  "ec_fb",       "Echo Feedback",     F, 0, 0.9f, 0.35f, 0, "%", "", "Echo repeats (bounded).") \
X(ecTone,      "ec_tone",     "Echo Tone",         F, 0, 1, 0.5f, 0, "%", "", "Darker to brighter repeats.") \
X(ecMix,       "ec_mix",      "Echo Wet",          F, 0, 1, 0.3f, 0, "%", "", "Echo wet amount.") \
X(fbAmount,    "fb_amount",   "Feedback",          F, 0, 1, 0, 0, "%", "", "Local Studio feedback: Resonator output back into Drive. Zero by default, bounded, killable.") \
X(fbTime,      "fb_time",     "Feedback Delay",    F, 1, 500, 30, 1, "ms", "", "Delay in the local feedback loop.") \
X(fbHP,        "fb_hp",       "Feedback HP",       F, 20, 2000, 80, 1, "Hz", "", "High-pass in the loop.") \
X(fbLP,        "fb_lp",       "Feedback LP",       F, 500, 16000, 6000, 1, "Hz", "", "Low-pass in the loop.") \
X(killFb,      "kill_fb",     "Kill Feedback",     B, 0, 1, 0, 0, "", "", "Ramp the local feedback return to zero and fade the loop.") \
X(killTail,    "kill_tail",   "Kill Tail",         B, 0, 1, 0, 0, "", "", "Fade out Echo and Resonator tails.") \
X(rawHw,       "raw_hw",      "Raw Hardware",      B, 0, 1, 0, 0, "", "", "Disable Studio chain, feedback and excitation: hear the modeled hardware only.") \
/* --- output --- */ \
X(mix,         "mix",         "Dry/Wet",           F, 0, 1, 1, 0, "%", "", "Linear dry/wet mix (0% = time-aligned dry input).") \
X(mixLaw,      "mix_law",     "Mix Law",           C, 0, 1, 0, 0, "", "Linear|Constant Power", "Dry/wet law.") \
X(dryTrim,     "dry_trim",    "Dry Trim",          F, -24, 6, 0, 0, "dB", "", "Dry path compensation.") \
X(outTrim,     "outtrim",     "Output",            F, -24, 12, 0, 0, "dB", "", "Output level after the mix.") \
X(monitor,     "monitor",     "Monitor",           C, 0, 6, 0, 0, "", "Full Output|Full Core|Injected Artifacts|Fault Difference|Studio Only|Full Wet|Dry Input", "Which named bus leaves the plugin. Changes nothing inside the model.") \
X(ceilOn,      "ceil_on",     "Ceiling",           B, 0, 1, 0, 0, "", "", "Optional zero-latency output ceiling (audible limiting).") \
X(ceiling,     "ceiling",     "Ceiling Level",     F, -24, 0, -0.3f, 0, "dB", "", "Output ceiling level.")

enum P : int
{
#define IFX_ENUM(e, id, name, kind, mn, mx, def, skew, unit, choices, tip) e,
    IFX_PARAMS (IFX_ENUM)
#undef IFX_ENUM
    kNumParams
};

struct ParamInfo
{
    const char* id;
    const char* name;
    ParamKind kind;
    float min, max, def;
    int skew;
    const char* unit;
    const char* choices; // '|' separated
    const char* tooltip;
};

inline const ParamInfo& paramInfo (int index) noexcept
{
#define IFX_KIND_F ParamKind::Float
#define IFX_KIND_B ParamKind::Bool
#define IFX_KIND_C ParamKind::Choice
#define IFX_KIND_I ParamKind::Int
#define IFX_INFO(e, id, name, kind, mn, mx, def, skew, unit, choices, tip) \
    { id, name, IFX_KIND_##kind, (float) (mn), (float) (mx), (float) (def), skew, unit, choices, tip },
    static const ParamInfo table[kNumParams] = { IFX_PARAMS (IFX_INFO) };
#undef IFX_INFO
    return table[index];
}

/** A plain snapshot of every parameter value (real units / choice index). */
struct ParamSnapshot
{
    float v[kNumParams];

    ParamSnapshot() noexcept { setDefaults(); }
    void setDefaults() noexcept
    {
        for (int i = 0; i < kNumParams; ++i)
            v[i] = paramInfo (i).def;
    }
    float operator[] (int i) const noexcept { return v[i]; }
    float& operator[] (int i) noexcept { return v[i]; }
    bool on (int i) const noexcept { return v[i] >= 0.5f; }
    int choice (int i) const noexcept { return (int) (v[i] + 0.5f); }
};

} // namespace ifx
