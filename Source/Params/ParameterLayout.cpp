#include "ParameterLayout.h"
#include "ParameterIDs.h"
#include "DiscreteIntParameter.h"
#include "../Chord/ChordTypes.h"
#include "../DSP/VoiceManager.h"
#include "../Common/Utf8.h"
#include "../DSP/Voice.h"
#include "../DSP/Oscillators/Oscillator.h"
#include "../DSP/Filters/SvfTpt.h"

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

    juce::String centsText (float v, int)
    {
        return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " ct";
    }

    juce::String octaveText (int v, int)
    {
        if (v == 0) return "0";
        return (v > 0 ? "+" : "") + juce::String (v) + (std::abs (v) == 1 ? " oct" : " oct");
    }

    juce::String hzText (float v, int)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
        return juce::String (v, 0) + " Hz";
    }

    /*  Bipolar: el signo importa tanto como la magnitud, asi que se muestra
        siempre en vez de dejar que el usuario adivine de que lado del centro
        esta el control. */
    juce::String bipolarText (float v, int)
    {
        const auto pct = juce::roundToInt (v * 100.0f);
        return (pct > 0 ? "+" : "") + juce::String (pct) + " %";
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

    // --- Envolvente de filtro -------------------------------------------------
    // Un poco mas corta que la de amplitud: el "click" tonal de apertura tiene
    // que terminar antes que el cuerpo de la nota, no al mismo tiempo.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::filtEnvAttack), "Filter Attack",
        timeRange (0.0f, 200.0f, 8.0f), 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (msText)
                                             .withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::filtEnvDecay), "Filter Decay",
        timeRange (5.0f, 2000.0f, 250.0f), 160.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (msText)
                                             .withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::filtEnvSustain), "Filter Sustain",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::filtEnvRelease), "Filter Release",
        timeRange (5.0f, 2000.0f, 200.0f), 120.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (msText)
                                             .withLabel ("ms")));

    // --- Osciladores ----------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::osc1Wave), "Osc 1 Wave",
        toStringArray (waveformNames), (int) Waveform::saw));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::osc1Level), "Osc 1 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.8f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::osc2Wave), "Osc 2 Wave",
        toStringArray (waveformNames), (int) Waveform::saw));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::osc2Level), "Osc 2 Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::osc2Detune), "Osc 2 Detune",
        juce::NormalisableRange<float> { -50.0f, 50.0f, 0.1f }, 7.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (centsText)
                                             .withLabel ("cents")));

    layout.add (std::make_unique<DiscreteIntParameter> (
        makeID (pid::osc2Octave), "Osc 2 Octave", -2, 2, 0,
        juce::AudioParameterIntAttributes{}.withStringFromValueFunction (octaveText)));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::subWave), "Sub Wave",
        toStringArray (waveformNames), (int) Waveform::sine));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::subLevel), "Sub Level",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<DiscreteIntParameter> (
        makeID (pid::subOctave), "Sub Octave", -2, 0, -1,
        juce::AudioParameterIntAttributes{}.withStringFromValueFunction (octaveText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::pulseWidth), "Pulse Width",
        juce::NormalisableRange<float> { 0.02f, 0.98f }, 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    // --- Unisono --------------------------------------------------------------
    layout.add (std::make_unique<DiscreteIntParameter> (
        makeID (pid::unisonCount), "Unison", 1, Voice::kMaxUnison, 1,
        juce::AudioParameterIntAttributes{}.withStringFromValueFunction (
            [] (int v, int) { return v == 1 ? juce::String ("Off")
                                            : juce::String (v) + " voces"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::unisonDetune), "Unison Detune",
        juce::NormalisableRange<float> { 0.0f, 50.0f, 0.1f }, 14.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (centsText)
                                             .withLabel ("cents")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::unisonWidth), "Unison Width",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.7f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::unisonRandomPhase), "Random Phase", false));

    // --- Filtro ---------------------------------------------------------------
    /*  Rango 20 Hz - 20 kHz con el centro del recorrido cerca de 1 kHz: sin el
        skew, todo lo musicalmente util quedaria aplastado en el primer 10% del
        control. */
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::filterCutoff), "Cutoff",
        timeRange (20.0f, 20000.0f, 1000.0f), 12000.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (hzText)
                                             .withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::filterResonance), "Resonance",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.15f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::filterType), "Filter Type",
        toStringArray (filterTypeNames), (int) FilterType::lowpass));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::filterSlope), "Slope",
        juce::StringArray { "12 dB/oct", "24 dB/oct" },
        (int) StereoLadder::Slope::db24));

    /*  Bipolar: positivo abre el filtro con el ataque (el "click" clasico),
        negativo lo cierra, que es mucho menos comun pero es lo que hace el
        efecto "invertido" de algunos stabs de techno. */
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::filterEnvAmount), "Filter Env Amount",
        juce::NormalisableRange<float> { -1.0f, 1.0f }, 0.55f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (bipolarText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::filterKeyTrack), "Key Track",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.3f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::filterVelAmount), "Velocity to Cutoff",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::filterPeakComp), "Reso Peak Tame", false));

    // --- Drive ----------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::driveAmount), "Drive",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
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
