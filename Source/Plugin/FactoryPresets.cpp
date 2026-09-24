#include "FactoryPresets.h"

namespace ifx
{
namespace FactoryPresets
{

namespace
{
    struct Builder
    {
        FactoryPreset p;
        Builder (const char* name, const char* category, const char* description, SourceType a, SourceType b = SourceType::None)
        {
            p.name = name;
            p.category = category;
            p.description = description;
            p.state = initState();
            p.state.presetName = name;
            p.state.category = category;
            p.state.description = description;
            p.state.slotA = a;
            p.state.slotB = b;
        }
        Builder& set (int param, float v) { p.state.params[param] = StateIO::sanitise (param, v); return *this; }
        Builder& seed (uint64_t s) { p.state.seed = s; return *this; }
        Builder& step (int lane, int index, float intensity = 0.7f, int gesture = 3, int accent = 0, int ratchet = 1,
                       float probability = 1.0f, float target = -1.0f, int choke = 0, float send = 0.5f, float duration = 1.0f)
        {
            auto& s = p.state.pattern.lanes[lane].steps[index];
            s.active = 1;
            s.intensity = intensity;
            s.gesture = (uint8_t) gesture;
            s.accent = (uint8_t) accent;
            s.ratchet = (uint8_t) ratchet;
            s.probability = probability;
            s.target = target;
            s.choke = (uint8_t) choke;
            s.send = send;
            s.duration = duration;
            return *this;
        }
        Builder& length (int lane, int n) { p.state.pattern.lanes[lane].length = n; return *this; }
    };

    juce::Array<FactoryPreset> build()
    {
        juce::Array<FactoryPreset> bank;
        auto add = [&bank] (Builder& b) { bank.add (b.p); };

        // ---------------------------------------------------------------- Repair Faults
        {
            Builder b ("Worn Guitar Jack", "Repair Faults",
                       "Passive pickup through a jack with a small worn spot. Occasional crackle when the plug moves; seat it fully to cure it.",
                       SourceType::LooseJack);
            b.set (P::inputMode, 1).set (P::cableLoad, 700).set (P::jkInsert, 0.93f).set (P::jkWiggle, 0.12f)
             .set (P::jkWear, 0.35f).set (P::jkWearPos, 0.9f).set (P::hum, 0.08f).set (P::jkShield, 0.85f);
            add (b);
        }
        {
            Builder b ("Scratchy Volume Pot", "Repair Faults",
                       "A dirty level pot. Automate Turn Knob across the middle of its travel to hear the worn arc; holding still stops the scratch.",
                       SourceType::DirtyPot);
            b.set (P::ptDirt, 0.55f).set (P::ptWearPos, 0.5f).set (P::ptWearWidth, 0.1f).set (P::ptScratch, 0.6f).set (P::ptTurn, 0.8f).set (P::hum, 0.0f);
            add (b);
        }
        {
            Builder b ("Crackly Patch Cable", "Repair Faults",
                       "A cable with broken strands near its middle. Bend it over the weak spot or shake it to find the dropout.",
                       SourceType::CableHandling);
            b.set (P::cbDamage, 0.55f).set (P::cbShieldDmg, 0.25f).set (P::cbWeakPos, 0.45f).set (P::cbBend, 0.42f).set (P::cbShake, 0.1f).set (P::hum, 0.1f);
            add (b);
        }
        {
            Builder b ("Ungrounded Input", "Repair Faults",
                       "An exposed, unterminated input. Touch the tip (drag on the illustration) to couple 50 Hz hum in.",
                       SourceType::PlugTouch);
            b.set (P::tcExposure, 0.85f).set (P::tcShield, 0.1f).set (P::tcTouch, 0.0f).set (P::hum, 0.4f).set (P::tcPop, 0.25f);
            add (b);
        }
        {
            Builder b ("Tired Tone Knob", "Repair Faults",
                       "A dirty tone pot on a passive pickup. Turning changes the treble cut; the worn arc crackles on the way.",
                       SourceType::DirtyPot, SourceType::LooseJack);
            b.set (P::inputMode, 1).set (P::ptCircuit, 1).set (P::ptTaper, 1).set (P::ptDirt, 0.45f).set (P::ptWearPos, 0.35f)
             .set (P::ptTurn, 0.7f).set (P::jkInsert, 1.0f).set (P::jkWiggle, 0.05f).set (P::jkWear, 0.15f).set (P::hum, 0.05f);
            add (b);
        }

        // ---------------------------------------------------------------- Subtle Texture
        {
            Builder b ("Barely Seated", "Subtle Texture",
                       "Mostly transparent. A slightly loose plug adds a rare resistive flicker and faint hum on the gaps.",
                       SourceType::LooseJack);
            b.set (P::jkInsert, 0.9f).set (P::jkWiggle, 0.08f).set (P::jkWear, 0.3f).set (P::jkWearPos, 0.88f)
             .set (P::jkIntermit, 0.2f).set (P::hum, 0.06f).set (P::mix, 0.85f);
            add (b);
        }
        {
            Builder b ("Studio Hum Bed", "Subtle Texture",
                       "A lightly shielded input picks up quiet ambient mains hum. Hum follows the untouched coupling only.",
                       SourceType::PlugTouch);
            b.set (P::tcExposure, 0.45f).set (P::tcShield, 0.5f).set (P::hum, 0.3f).set (P::tcBody, 0.25f).set (P::tcPop, 0.0f);
            add (b);
        }
        {
            Builder b ("Old Console Channel", "Subtle Texture",
                       "Dirty fader plus a slightly worn jack: level wobble and very occasional crackle on a mix bus.",
                       SourceType::DirtyPot, SourceType::LooseJack);
            b.set (P::ptDirt, 0.25f).set (P::ptTurn, 0.82f).set (P::ptVibration, 0.0f).set (P::jkInsert, 0.95f)
             .set (P::jkWiggle, 0.05f).set (P::jkWear, 0.2f).set (P::hum, 0.04f).set (P::stereo, 0.3f);
            add (b);
        }
        {
            Builder b ("Microphonic Lead", "Subtle Texture",
                       "A cable with a microphonic path: gentle handling rub tied to its motion. No mic, no rub.",
                       SourceType::CableHandling);
            b.set (P::cbMic, 0.35f).set (P::cbShake, 0.15f).set (P::cbDamage, 0.05f).set (P::hum, 0.0f);
            add (b);
        }

        // ---------------------------------------------------------------- Rhythmic
        {
            Builder b ("Wiggle Every Beat", "Rhythmic",
                       "Play mode: one jack wiggle per beat, lighter cable shakes on the offbeats. Lock Seed for identical bounces.",
                       SourceType::LooseJack, SourceType::CableHandling);
            b.set (P::mode, 1).set (P::laneOn, 1).set (P::jkInsert, 0.78f).set (P::jkWear, 0.6f).set (P::jkWearPos, 0.72f)
             .set (P::cbDamage, 0.5f).set (P::cbWeakPos, 0.5f).set (P::cbBend, 0.5f).set (P::hum, 0.15f)
             .set (P::gestureA, 0).set (P::gestureB, 3);
            for (int i = 0; i < 16; i += 4) b.step (0, i, 0.85f, 0);
            for (int i = 2; i < 16; i += 4) b.step (1, i, 0.45f, 1);
            add (b);
        }
        {
            Builder b ("Pot Sweeps 8ths", "Rhythmic",
                       "Dirty pot swept across its worn arc on eighth notes; the level dips and scratches in time.",
                       SourceType::DirtyPot);
            b.set (P::mode, 1).set (P::laneOn, 1).set (P::ptDirt, 0.7f).set (P::ptWearPos, 0.55f).set (P::ptTurn, 0.7f).set (P::ptScratch, 0.7f)
             .set (P::hum, 0.0f);
            for (int i = 0; i < 16; i += 2) b.step (0, i, 0.7f, 1, i % 8 == 0 ? 1 : 0);
            add (b);
        }
        {
            Builder b ("Touch Pulse", "Rhythmic",
                       "Rhythmic finger taps on an exposed tip: hum bursts on the quarter notes, held on the last beat.",
                       SourceType::PlugTouch);
            b.set (P::mode, 1).set (P::laneOn, 1).set (P::hum, 0.6f).set (P::tcExposure, 0.9f).set (P::tcShield, 0.05f).set (P::tcPop, 0.3f).set (P::mix, 1.0f);
            b.step (0, 0, 0.8f, 0).step (0, 4, 0.8f, 0).step (0, 8, 0.8f, 0).step (0, 12, 0.9f, 1, 1, 1, 1.0f, -1.0f, 0, 0.5f, 3.0f);
            add (b);
        }

        // ---------------------------------------------------------------- One-Shots
        {
            Builder b ("Touch and Turn", "One-Shots",
                       "Create mode: modest noise excitation reveals a finger touch and a dirty pot turn. Render two seconds to WAV.",
                       SourceType::PlugTouch, SourceType::DirtyPot);
            b.set (P::mode, 2).set (P::excType, 1).set (P::excLevel, -30).set (P::hum, 0.5f).set (P::tcExposure, 0.9f)
             .set (P::ptDirt, 0.7f).set (P::ptWearPos, 0.5f).set (P::gestureA, 4).set (P::gestureB, 1).set (P::monitor, 0);
            b.step (0, 0, 0.9f, 3).step (1, 4, 0.8f, 3).length (0, 16).length (1, 16);
            add (b);
        }
        {
            Builder b ("Jack Pull Pop", "One-Shots",
                       "A charged input: a contact break discharges it as a bounded pop. Play one-shot pattern (or MIDI note 38) "
                       "pulls the plug out; Trigger A wiggles it in the worn zone.",
                       SourceType::LooseJack);
            b.set (P::mode, 2).set (P::excType, 3).set (P::excLevel, -24).set (P::jkPop, 0.7f).set (P::jkWear, 0.6f).set (P::jkInsert, 0.9f)
             .set (P::gestureA, 0).set (P::hum, 0.2f).set (P::jkShield, 0.4f);
            b.step (0, 0, 0.9f, 2); // lane gesture 2 of a Loose Jack = Pull (played by the one-shot)
            add (b);
        }
        {
            Builder b ("Cable Knock", "One-Shots",
                       "Tap a microphonic cable: a short knock with a resonant decay. Mic is on, so the handling layer is audible.",
                       SourceType::CableHandling);
            b.set (P::mode, 2).set (P::cbMic, 0.8f).set (P::gestureA, 2).set (P::cbDamage, 0.3f).set (P::hum, 0.0f);
            add (b);
        }

        // ---------------------------------------------------------------- Wild (Expressive)
        {
            Builder b ("Stomped Cable", "Wild",
                       "Expressive policy: dense cable faults under hard shaking, Studio Drive on the artifacts only.",
                       SourceType::CableHandling, SourceType::LooseJack);
            b.set (P::policy, 1).set (P::cbShake, 0.8f).set (P::cbDamage, 0.9f).set (P::cbShieldDmg, 0.6f).set (P::cbMic, 0.4f)
             .set (P::jkInsert, 0.75f).set (P::jkWear, 0.7f).set (P::hum, 0.4f).set (P::studioOn, 1).set (P::studioSrc, 2)
             .set (P::drOn, 1).set (P::drPre, 18).set (P::drPost, -9).set (P::ceilOn, 1);
            add (b);
        }
        {
            Builder b ("Broken Amp Input", "Wild",
                       "Expressive: a half-inserted jack on an exposed input, heavy hum and crackle, crushed Studio artifacts.",
                       SourceType::LooseJack, SourceType::PlugTouch);
            b.set (P::policy, 1).set (P::jkInsert, 0.62f).set (P::jkWiggle, 0.6f).set (P::jkWear, 0.9f).set (P::jkShield, 0.2f)
             .set (P::tcExposure, 1.0f).set (P::tcTouch, 0.3f).set (P::hum, 0.7f).set (P::buzz, 0.3f)
             .set (P::studioOn, 1).set (P::studioSrc, 0).set (P::crOn, 1).set (P::crBits, 6).set (P::crRate, 6000).set (P::ceilOn, 1);
            add (b);
        }

        {
            Builder b ("Hum Storm", "Wild",
                       "Expressive: a passive pickup into a floating input and a cable with a torn shield under a fluttering finger; the hum bursts feed a synced Studio echo.",
                       SourceType::PlugTouch, SourceType::CableHandling);
            b.set (P::policy, 1).set (P::inputMode, 1).set (P::hum, 0.8f).set (P::buzz, 0.25f).set (P::tcExposure, 1.0f).set (P::tcShield, 0.0f).set (P::tcPressure, 0.1f)
             .set (P::tcBody, 0.8f).set (P::cbShieldDmg, 0.9f).set (P::cbShake, 0.4f).set (P::cbDamage, 0.4f)
             .set (P::mode, 1).set (P::laneOn, 1).set (P::studioOn, 1).set (P::studioSrc, 0).set (P::ecOn, 1).set (P::ecMix, 0.45f).set (P::ecFeedback, 0.5f)
             .set (P::ceilOn, 1);
            for (int i = 0; i < 16; i += 3) b.step (0, i, 0.7f, i % 2 == 0 ? 0 : 1);
            b.step (1, 6, 0.8f, 1).step (1, 14, 0.8f, 1);
            add (b);
        }

        // ---------------------------------------------------------------- Machine Pattern
        {
            Builder b ("Broken Snare", "Machine Pattern",
                       "Snare bus: jack wiggles on 2 and 4, sparse cable bends as fills. Studio high-pass, Drive and Crush on the artifacts; the clean snare stays dry.",
                       SourceType::LooseJack, SourceType::CableHandling);
            b.set (P::mode, 1).set (P::laneOn, 1).set (P::jkInsert, 0.8f).set (P::jkWear, 0.7f).set (P::jkWearPos, 0.75f)
             .set (P::cbDamage, 0.6f).set (P::cbWeakPos, 0.5f).set (P::hum, 0.1f)
             .set (P::studioOn, 1).set (P::studioSrc, 2).set (P::eqOn, 1).set (P::eqHP, 300).set (P::drOn, 1).set (P::drPre, 15).set (P::drPost, -8)
             .set (P::crOn, 1).set (P::crBits, 10).set (P::crRate, 12000).set (P::ceilOn, 1);
            b.step (0, 4, 0.9f, 0, 1).step (0, 12, 0.9f, 0, 1);
            b.step (1, 14, 0.6f, 0, 0, 1, 0.5f).step (1, 7, 0.5f, 0, 0, 1, 0.35f);
            add (b);
        }
        {
            Builder b ("Machine Percussion", "Machine Pattern",
                       "Silent channel: filtered buzz excitation, pot sweeps over the damaged arc on 16ths and microphonic cable taps. The Resonator makes the strikes ring.",
                       SourceType::DirtyPot, SourceType::CableHandling);
            b.set (P::mode, 2).set (P::laneOn, 1).set (P::excType, 2).set (P::excLevel, -26).set (P::ptDirt, 0.85f).set (P::ptWearPos, 0.5f)
             .set (P::ptTurn, 0.6f).set (P::ptScratch, 0.8f).set (P::cbMic, 0.7f).set (P::hum, 0.0f)
             .set (P::studioOn, 1).set (P::studioSrc, 2).set (P::rsOn, 1).set (P::rsFreq, 110).set (P::rsDecay, 0.35f).set (P::rsMix, 0.6f).set (P::ceilOn, 1);
            for (int i = 0; i < 16; ++i) b.step (0, i, i % 4 == 0 ? 0.85f : 0.5f, 1, i % 4 == 0 ? 1 : 0, 1, i % 4 == 0 ? 1.0f : 0.6f);
            b.step (1, 3, 0.8f, 2).step (1, 11, 0.8f, 2).step (1, 14, 0.6f, 2, 0, 2);
            add (b);
        }
        {
            Builder b ("Breakdown", "Machine Pattern",
                       "Bass track: staggered jack pulls and touch holds. Local feedback grows on accents, then the Rhythm Gate chokes it; the bass dry path stays intact.",
                       SourceType::LooseJack, SourceType::PlugTouch);
            b.set (P::mode, 1).set (P::laneOn, 1).set (P::jkInsert, 0.85f).set (P::jkWear, 0.6f).set (P::tcExposure, 0.8f).set (P::hum, 0.45f)
             .set (P::mix, 1.0f).set (P::studioOn, 1).set (P::studioSrc, 0).set (P::drOn, 1).set (P::drPre, 12).set (P::fbAmount, 0.35f).set (P::fbTime, 90)
             .set (P::gtOn, 1).set (P::gtMode, 2).set (P::gtChokeFb, 1).set (P::ceilOn, 1);
            b.step (0, 0, 0.9f, 2, 1).step (0, 8, 0.7f, 2).step (0, 15, 0.8f, 2, 0, 1, 1.0f, -1.0f, 1);
            b.step (1, 4, 0.8f, 1, 0, 1, 1.0f, -1.0f, 0, 0.5f, 3.0f).step (1, 12, 0.9f, 1, 1, 1, 1.0f, -1.0f, 0, 0.8f, 2.0f);
            add (b);
        }
        {
            Builder b ("Controlled Chaos", "Machine Pattern",
                       "Two lanes of 16 and 12 steps with high-probability variations and Evolve. Seed locked: every export is identical; New Take changes the micro-events only.",
                       SourceType::CableHandling, SourceType::DirtyPot);
            b.set (P::mode, 1).set (P::laneOn, 1).set (P::evolve, 0.5f).set (P::cbDamage, 0.7f).set (P::cbShake, 0.1f)
             .set (P::ptDirt, 0.7f).set (P::hum, 0.2f).set (P::humanize, 0.2f).set (P::swing, 0.08f).seed (0xC4A05ull);
            for (int i = 0; i < 16; ++i) if (i % 3 != 2) b.step (0, i, 0.4f + 0.05f * (float) (i % 5), i % 3, i % 4 == 0 ? 1 : 0, 1, 0.75f);
            b.length (1, 12);
            for (int i = 0; i < 12; i += 2) b.step (1, i, 0.6f, i % 3, 0, 1, 0.8f);
            add (b);
        }

        // ---------------------------------------------------------------- Environment (appended in 1.0.0:
        // new presets go at the end so existing host program numbers stay the same)
        {
            Builder b ("Breathing Ground Loop", "Environment",
                       "A 2-prong amp leaks current through 30 m of foil-shielded cable: quiet, real-world loop hum that dips and flutters "
                       "as the broken braid (slot A) is shaken. MIDI note 50 pulls the jack in slot B out and breaks the loop.",
                       SourceType::CableHandling, SourceType::LooseJack);
            b.set (P::glGear, 3).set (P::glRun, 30.0f).set (P::glShield, 0).set (P::glIface, 0).set (P::hum, 0.0f)
             .set (P::cbShieldDmg, 0.6f).set (P::cbDamage, 0.0f).set (P::cbWeakPos, 0.5f).set (P::cbBend, 0.5f).set (P::cbShake, 0.25f)
             .set (P::cbMic, 0.0f).set (P::jkInsert, 1.0f).set (P::jkWear, 0.15f).set (P::jkWiggle, 0.0f);
            add (b);
        }
        {
            Builder b ("Loop Fixed: Isolator", "Environment",
                       "The same ground loop through a balanced input with an isolator box: about 70 dB less hum at mains, less "
                       "on the upper harmonics. At this level the residue is far below hearing: the fix sounds like silence. The "
                       "Environment tab readout shows what remains. Compare it with Breathing Ground Loop (A/B).",
                       SourceType::CableHandling, SourceType::LooseJack);
            b.set (P::glGear, 3).set (P::glRun, 30.0f).set (P::glShield, 0).set (P::glIface, 2).set (P::hum, 0.0f)
             .set (P::cbShieldDmg, 0.6f).set (P::cbDamage, 0.0f).set (P::cbWeakPos, 0.5f).set (P::cbBend, 0.5f).set (P::cbShake, 0.25f)
             .set (P::cbMic, 0.0f).set (P::jkInsert, 1.0f).set (P::jkWear, 0.15f).set (P::jkWiggle, 0.0f);
            add (b);
        }
        {
            Builder b ("Dimmer Wah", "Environment",
                       "A cheap wall dimmer buzzes into a touched, exposed input. Automate Dimmer (Environment tab): the rasp swells "
                       "to a peak at half brightness and vanishes fully on or off, always locked to the mains.",
                       SourceType::PlugTouch);
            b.set (P::buzz, 0.8f).set (P::buzzType, 1).set (P::dimBright, 0.5f).set (P::dimKind, 0).set (P::hum, 0.15f)
             .set (P::tcTouch, 0.5f).set (P::tcPressure, 0.8f).set (P::tcExposure, 0.9f).set (P::tcShield, 0.05f).set (P::tcPop, 0.0f)
             .set (P::mix, 1.0f);
            add (b);
        }
        {
            Builder b ("Phone Rings Nearby", "Environment",
                       "A finger on an exposed input hums softly while a 2G phone 25 cm away gets an incoming call: its dit-dit handshake "
                       "clusters come and go. Set 2G Phone to In Call for the steady burst, or move it away.",
                       SourceType::PlugTouch);
            b.set (P::gsm, 2).set (P::gsmDist, 0.25f).set (P::hum, 0.2f).set (P::tcTouch, 0.35f).set (P::tcPressure, 0.8f)
             .set (P::tcExposure, 0.7f).set (P::tcShield, 0.2f).set (P::tcPop, 0.1f);
            add (b);
        }
        {
            Builder b ("Cable-TV Hum Monster", "Wild",
                       "Expressive: gear grounded through a cable-TV line drives a huge round loop hum through 30 m of foil shield; "
                       "a worn jack stutters it and Studio Drive grits it up. The Ceiling keeps peaks safe.",
                       SourceType::LooseJack);
            b.set (P::policy, 1).set (P::glGear, 4).set (P::glOutlets, 3).set (P::glRun, 30.0f).set (P::glShield, 0).set (P::glIface, 0)
             .set (P::hum, 0.2f).set (P::jkInsert, 0.8f).set (P::jkWear, 0.6f).set (P::jkWearPos, 0.78f).set (P::jkWiggle, 0.3f)
             .set (P::studioOn, 1).set (P::studioSrc, 0).set (P::drOn, 1).set (P::drPre, 9).set (P::drPost, -10).set (P::ceilOn, 1);
            add (b);
        }
        return bank;
    }
}

const juce::StringArray& categories()
{
    static const juce::StringArray c { "Repair Faults", "Rhythmic", "One-Shots", "Subtle Texture", "Wild", "Machine Pattern", "Environment" };
    return c;
}

PluginState initState()
{
    PluginState s; // parameter defaults, Loose Jack in slot A, empty pattern
    s.presetName = "Init";
    return s;
}

const juce::Array<FactoryPreset>& all()
{
    static const juce::Array<FactoryPreset> bank = build();
    return bank;
}

} // namespace FactoryPresets
} // namespace ifx
