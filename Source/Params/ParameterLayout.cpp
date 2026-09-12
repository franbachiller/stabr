#include "ParameterLayout.h"
#include "ParameterIDs.h"
#include "DiscreteIntParameter.h"
#include "../Chord/ChordTypes.h"
#include "../DSP/VoiceManager.h"
#include "../Common/Utf8.h"

namespace stabr
{
using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
    juce::ParameterID makeID (const char* id) noexcept
    {
        return { id, pid::kVersionHintV1 };
    }

    /*  Rango de tiempo con centro musical: el skew hace que la mitad del
        recorrido del control caiga en 'centre', que es donde vive el uso real.
        Sin esto, un rango 0-2000 ms deja todo lo util aplastado en el 5% inicial.
    */
    juce::NormalisableRange<float> timeRange (float min, float max, float centre)
    {
        juce::NormalisableRange<float> r { min, max };
        r.setSkewForCentre (centre);
        return r;
    }

    juce::String msText (float v, int)
    {
        if (v < 10.0f)   return juce::String (v, 2) + " ms";
        if (v < 1000.0f) return juce::String (v, 1) + " ms";
        return juce::String (v / 1000.0f, 2) + " s";
    }

    juce::String dbText (float v, int)
    {
        return v <= -59.9f ? juce::String ("-inf dB")
                           : juce::String (v, 1) + " dB";
    }

    juce::String percentText (float v, int)
    {
        return juce::String (juce::roundToInt (v * 100.0f)) + " %";
    }

    juce::String semitoneText (float v, int)
    {
        const auto n = juce::roundToInt (v);
        return juce::String (n) + (std::abs (n) == 1 ? " semitono" : " semitonos");
    }

    /*  Los nombres viven en Chord/ChordTypes.h, que es C++ puro y no conoce
        JUCE. Aca se convierten una sola vez, para que el layout de parametros
        y la GUI no puedan desincronizarse de las tablas del motor. */
    template <size_t N>
    juce::StringArray toStringArray (const std::array<const char*, N>& names)
    {
        juce::StringArray a;

        for (auto* n : names)
            a.add (utf8 (n));

        return a;
    }
}

APVTS::ParameterLayout createParameterLayout()
{
    APVTS::ParameterLayout layout;

    // --- Envolvente de amplitud ---------------------------------------------
    // Defaults deliberadamente "stab": ataque casi nulo, decay corto, sustain
    // en cero. El plugin tiene que sonar corto y seco apenas se carga.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::ampAttack), "Amp Attack",
        timeRange (0.0f, 200.0f, 8.0f), 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (msText)
                                             .withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::ampDecay), "Amp Decay",
        timeRange (5.0f, 2000.0f, 250.0f), 220.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (msText)
                                             .withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::ampSustain), "Amp Sustain",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::ampRelease), "Amp Release",
        timeRange (5.0f, 2000.0f, 200.0f), 120.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (msText)
                                             .withLabel ("ms")));

    // --- Oscilador ------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::osc1Level), "Osc 1 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.8f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    // --- Motor de acordes -----------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::chordType), "Chord Type",
        toStringArray (chord::chordTypeNames),
        (int) chord::ChordType::min7));       // el acorde de house por excelencia

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::scaleLock), "Scale Lock",
        toStringArray (chord::scaleLockNames),
        (int) chord::ScaleLock::diatonic));   // ver DECISIONS.md D-011

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::scaleKey), "Key",
        toStringArray (chord::keyNames), 9));  // La

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::scaleMode), "Scale",
        toStringArray (chord::scaleModeNames),
        (int) chord::ScaleMode::naturalMinor));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::noteMode), "Note Mode",
        toStringArray (chord::noteModeNames),
        (int) chord::NoteMode::chord));       // ver DECISIONS.md D-012

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::chordVoicing), "Voicing",
        toStringArray (chord::voicingNames),
        (int) chord::VoicingType::closed));

    layout.add (std::make_unique<DiscreteIntParameter> (
        makeID (pid::chordInversion), "Inversion", 0, 3, 0));

    layout.add (std::make_unique<DiscreteIntParameter> (
        makeID (pid::chordSpread), "Spread", 0, 3, 0,
        juce::AudioParameterIntAttributes{}.withStringFromValueFunction (
            [] (int v, int) { return v == 0 ? juce::String ("Off")
                                            : juce::String (v) + " oct"; })));

    layout.add (std::make_unique<DiscreteIntParameter> (
        makeID (pid::chordDensity), "Density",
        chord::kMinDensity, chord::kMaxDensity, 4,
        juce::AudioParameterIntAttributes{}.withStringFromValueFunction (
            [] (int v, int) { return juce::String (v) + " voces"; })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::chordOctaveDouble), "Octave Down", false));

    // --- Global ---------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::masterGain), "Master Gain",
        juce::NormalisableRange<float> { -60.0f, 6.0f, 0.1f }, -6.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (dbText)
                                             .withLabel ("dB")));

    /*  Polifonia = disparos simultaneos (acordes en modo Chord, notas sueltas
        en Passthrough), no voces. Con un min9 de 5 notas, "8 voces" no
        significaba nada para el usuario; "cuantos stabs se pueden superponer"
        si. Ver DECISIONS.md D-020. */
    layout.add (std::make_unique<DiscreteIntParameter> (
        makeID (pid::polyphony), "Polyphony", 1, VoiceManager::kMaxPolyphony, 6,
        juce::AudioParameterIntAttributes{}.withStringFromValueFunction (
            [] (int v, int) { return juce::String (v) + (v == 1 ? " acorde" : " acordes"); })));

    layout.add (std::make_unique<DiscreteIntParameter> (
        makeID (pid::pitchBendRange), "Pitch Bend Range", 0, 24, 2,
        juce::AudioParameterIntAttributes{}.withStringFromValueFunction (
            [] (int v, int) { return semitoneText ((float) v, 0); })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::hqMode), "HQ Mode", false));

    return layout;
}
}
