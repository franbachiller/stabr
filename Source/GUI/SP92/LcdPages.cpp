#include "LcdPages.h"
#include "../../Params/ParameterIDs.h"

namespace stabr::gui::sp92
{
const std::vector<LcdPage>& lcdPages()
{
    using namespace pid;

    static const std::vector<LcdPage> pages
    {
        // Etiquetas de 5 caracteres como maximo: cada casillero mide 31 puntos,
        // y 6 caracteres (35 puntos) se pisan con el vecino.
        { "CHORD", {
            {{ { chordType, "TYPE" }, { scaleKey, "KEY" }, { scaleMode, "SCALE" }, { chordVoicing, "VOICE" }, { chordInversion, "INV" }, { chordDensity, "DENS" } }},
            {{ { scaleLock, "LOCK" }, { noteMode, "NOTES" }, { chordSpread, "SPRD" }, { chordOctaveDouble, "OCTDN" }, { polyphony, "POLY" }, { pitchBendRange, "BEND" } }},
        }},
        { "OSC", {
            {{ { osc1Wave, "WAVE1" }, { osc1Level, "LVL1" }, { osc2Wave, "WAVE2" }, { osc2Level, "LVL2" }, { osc2Detune, "TUNE2" }, { osc2Octave, "OCT2" } }},
            {{ { subWave, "SUB" }, { subLevel, "SUBLV" }, { subOctave, "SUBOC" }, { pulseWidth, "PW" }, { unisonCount, "UNI" }, { unisonDetune, "UNIDT" } }},
            {{ { unisonWidth, "WIDTH" }, { unisonRandomPhase, "PHASE" }, { hqMode, "HQ" }, { masterGain, "MSTR" }, {}, {} }},
        }},
        { "FILTER", {
            {{ { filterType, "TYPE" }, { filterSlope, "SLOPE" }, { filterKeyTrack, "KTRK" }, { filterVelAmount, "VELO" }, { filterPeakComp, "TAME" }, { filtEnvDecay, "FDEC" } }},
            {{ { filtEnvAttack, "FATK" }, { filtEnvSustain, "FSUS" }, { filtEnvRelease, "FREL" }, { ampAttack, "AATK" }, { ampSustain, "ASUS" }, { ampRelease, "AREL" } }},
        }},
        { "DELAY", {
            {{ { delayEnabled, "DELAY" }, { delaySync, "SYNC" }, { delayFreeMs, "TIME" }, { delayOffset, "OFFST" }, { delayFreeze, "FREEZ" }, { delayGlide, "GLIDE" } }},
            {{ { delayHighpass, "HPF" }, { delayLowpass, "LPF" }, { delayModDepth, "WOW" }, { delayDucking, "DUCK" }, {}, {} }},
        }},
        { "REVERB", {
            {{ { reverbEnabled, "VERB" }, { reverbMode, "MODE" }, { reverbSize, "SIZE" }, { reverbDecay, "DECAY" }, { reverbDamping, "DAMP" }, { reverbMix, "MIX" } }},
            {{ { reverbModDepth, "MOD" }, { reverbPreDelaySync, "PSYNC" }, { reverbPreDelayDiv, "PDIV" }, { reverbPreDelayMs, "PRE" }, { reverbLowCut, "LOCUT" }, { reverbDucking, "DUCK" } }},
        }},
        { "FX", {
            {{ { satEnabled, "SAT" }, { satType, "TYPE" }, { satDrive, "DRIVE" }, { satBits, "BITS" }, { satRateReduce, "RATE" }, { satMix, "MIX" } }},
            {{ { chorusEnabled, "CHOR" }, { chorusEnsemble, "ENSEM" }, { chorusRate, "RATE" }, { chorusDepth, "DEPTH" }, { chorusWidth, "WIDTH" }, { chorusMix, "MIX" } }},
            {{ { fxOrder, "ORDER" }, {}, {}, {}, {}, {} }},
        }},
        { "PUMP", {
            {{ { pumpEnabled, "PUMP" }, { pumpDivision, "RATE" }, { pumpDepth, "DEPTH" }, { pumpShape, "SHAPE" }, { pumpAttack, "ATK" }, { outputLimiter, "LIMIT" } }},
            {{ { outputWidth, "WIDTH" }, { outputMonoBelow, "MONO" }, { seqSteps, "STEPS" }, {}, {}, {} }},
        }},
    };

    return pages;
}

//==============================================================================
namespace
{
    juce::String abbreviate (juce::String name)
    {
        name = name.toUpperCase().trim();

        static const juce::StringPairArray known = []
        {
            juce::StringPairArray a;
            a.set ("NATURAL MINOR", "NMIN"); a.set ("HARMONIC MINOR", "HMIN"); a.set ("MAJOR", "MAJ");
            a.set ("MINOR", "MIN");          a.set ("DORIAN", "DOR");          a.set ("PHRYGIAN", "PHRY");
            a.set ("DIATONIC", "DIAT");      a.set ("LITERAL", "LIT");         a.set ("PASSTHROUGH", "PASS");
            a.set ("CLOSED", "CLSD");        a.set ("DROP 2", "DRP2");         a.set ("DROP 3", "DRP3");
            a.set ("LOW PASS", "LP");        a.set ("BAND PASS", "BP");        a.set ("HIGH PASS", "HP");
            a.set ("12 DB/OCT", "12DB");     a.set ("24 DB/OCT", "24DB");      a.set ("TRIANGLE", "TRI");
            a.set ("ORGAN", "ORGN");         a.set ("PULSE", "PULS");          a.set ("STEREO", "STER");
            a.set ("PING PONG", "PING");     a.set ("TRANSISTOR", "TRAN");     a.set ("BITCRUSH", "CRSH");
            a.set ("DIGITAL", "DIGI");       a.set ("POWER", "POWR");          a.set ("PLATE", "PLAT");
            a.set ("MIN11", "MN11");         a.set ("1 BAR", "1BAR");          a.set ("2 BARS", "2BAR");
            a.set ("3 BARS", "3BAR");        a.set ("4 BARS", "4BAR");
            return a;
        }();

        if (known.containsKey (name))
            return known[name];

        // Orden de efectos: "SAT > CHO > DLY" -> "SCD".
        if (name.contains (">"))
        {
            juce::String initials;

            for (auto part : juce::StringArray::fromTokens (name, ">", ""))
                initials << part.trim().substring (0, 1);

            return initials;
        }

        return name.removeCharacters (" ").substring (0, 5);
    }
}

juce::String lcdValue (juce::RangedAudioParameter& p)
{
    const float plain = p.convertFrom0to1 (p.getValue());
    const auto& id = p.getParameterID();

    if (auto* b = dynamic_cast<juce::AudioParameterBool*> (&p))
        return b->get() ? "ON" : "OFF";

    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&p))
        return abbreviate (c->getCurrentChoiceName());

    if (dynamic_cast<juce::AudioParameterInt*> (&p) != nullptr)
    {
        const int v = juce::roundToInt (plain);

        if ((id == pid::unisonCount && v <= 1) || (id == pid::chordSpread && v == 0))
            return "OFF";

        if (id == pid::osc2Octave || id == pid::subOctave)
            return (v > 0 ? "+" : "") + juce::String (v);

        return juce::String (v);
    }

    const auto unit = p.getLabel();

    if (unit == "ms")    return plain < 1000.0f ? juce::String (plain < 10.0f ? plain : (float) juce::roundToInt (plain), plain < 10.0f ? 1 : 0) + "MS"
                                                : juce::String (plain / 1000.0f, 2) + "S";
    if (unit == "Hz")    return plain >= 1000.0f ? juce::String (plain / 1000.0f, plain >= 10000.0f ? 1 : 2) + "K"
                                                 : juce::String (juce::roundToInt (plain));
    if (unit == "dB")    return juce::String (plain, 1);
    if (unit == "cents") return juce::String (juce::roundToInt (plain)) + "CT";

    if (id == pid::satBits)       return juce::String (juce::roundToInt (plain)) + "B";
    if (id == pid::satRateReduce) return plain <= 1.5f ? juce::String ("OFF") : "1/" + juce::String (juce::roundToInt (plain));
    if (id == pid::chorusRate)    return plain < 1.0f  ? juce::String (plain, 2)
                                       : plain < 10.0f ? juce::String (plain, 1) + "HZ"
                                                       : juce::String (juce::roundToInt (plain)) + "HZ";

    const auto range = p.getNormalisableRange();

    if (range.start < 0.0f && range.end > 0.0f)
        return (plain > 0.005f ? "+" : "") + juce::String (juce::roundToInt (plain * 100.0f));

    // Porcentajes: 0..1, swing 0.5..0.75, ancho 0..2.
    if (range.end <= 2.0f)
        return juce::String (juce::roundToInt (plain * 100.0f)) + "%";

    return juce::String (plain, 1);
}
}
