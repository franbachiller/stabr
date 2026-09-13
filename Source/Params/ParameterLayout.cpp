#include "ParameterLayout.h"
#include "ParameterIDs.h"
#include "DiscreteIntParameter.h"
#include "../Chord/ChordTypes.h"
#include "../DSP/VoiceManager.h"
#include "../Common/Utf8.h"
#include "../DSP/Voice.h"
#include "../DSP/Oscillators/Oscillator.h"
#include "../DSP/Filters/SvfTpt.h"
#include "../FX/FxChain.h"
#include "../Sequencer/Patterns.h"

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

    //==========================================================================
    // CADENA DE EFECTOS
    //==========================================================================
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::fxOrder), "FX Order",
        toStringArray (fx::fxOrderNames), (int) fx::FxOrder::satChorusDelay));

    // --- Saturacion -----------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::satEnabled), "Sat On", true));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::satType), "Sat Type",
        toStringArray (fx::saturationTypeNames), (int) fx::SaturationType::tape));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::satDrive), "Sat Drive",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::satBits), "Bit Depth",
        juce::NormalisableRange<float> { 2.0f, 16.0f, 1.0f }, 12.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v)) + " bits"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::satRateReduce), "Rate Reduce",
        juce::NormalisableRange<float> { 1.0f, 50.0f, 1.0f }, 1.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (
            [] (float v, int) { const auto n = juce::roundToInt (v);
                                return n <= 1 ? juce::String ("Off")
                                              : juce::String ("1/") + juce::String (n); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::satMix), "Sat Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    // --- Chorus ---------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::chorusEnabled), "Chorus On", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::chorusEnsemble), "Ensemble", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::chorusRate), "Chorus Rate",
        timeRange (0.02f, 10.0f, 0.8f), 0.6f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 2) + " Hz"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::chorusDepth), "Chorus Depth",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::chorusWidth), "Chorus Width",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.8f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::chorusMix), "Chorus Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    // --- Delay: el bloque mas importante del plugin ---------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::delayEnabled), "Delay On", true));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::delayMode), "Delay Mode",
        toStringArray (fx::delayModeNames), (int) fx::DelayMode::stereo));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::delaySync), "Delay Sync", true));

    // 1/8. = 3/16: el tiempo de delay del dub techno.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::delayDivision), "Delay Time",
        toStringArray (fx::divisionNames), (int) fx::Division::n1_8D));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::delayFreeMs), "Delay Time (ms)",
        timeRange (10.0f, 4000.0f, 400.0f), 375.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (msText)
                                             .withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::delayOffset), "Delay Offset",
        juce::NormalisableRange<float> { -1.0f, 1.0f }, 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (bipolarText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::delayFeedback), "Feedback",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.45f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::delayFreeze), "Freeze", false));

    /*  Los filtros del lazo son lo que hace que el delay se pueda usar en una
        mezcla: sin ellos, cada repeticion mantiene todo su contenido y a las
        cuatro repeticiones hay barro. */
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::delayHighpass), "Delay HP",
        timeRange (20.0f, 2000.0f, 200.0f), 180.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (hzText)
                                             .withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::delayLowpass), "Delay LP",
        timeRange (400.0f, 20000.0f, 4000.0f), 6000.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (hzText)
                                             .withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::delayModDepth), "Wow & Flutter",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.15f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::delayGlide), "Time Change",
        juce::StringArray { "Tape", "Digital" },
        (int) fx::DelayLine::GlideMode::tape));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::delayDucking), "Delay Duck",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.4f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::delayMix), "Delay Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.3f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    // --- Reverb ---------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::reverbEnabled), "Reverb On", true));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::reverbMode), "Reverb Mode",
        toStringArray (fx::reverbModeNames), (int) fx::ReverbMode::plate));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::reverbSize), "Reverb Size",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::reverbDecay), "Reverb Decay",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::reverbDamping), "Reverb Damping",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::reverbModDepth), "Reverb Mod",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.3f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::reverbPreDelaySync), "Pre-Delay Sync", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::reverbPreDelayDiv), "Pre-Delay Div",
        toStringArray (fx::divisionNames), (int) fx::Division::n1_16));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::reverbPreDelayMs), "Pre-Delay",
        timeRange (0.0f, 500.0f, 40.0f), 20.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (msText)
                                             .withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::reverbLowCut), "Reverb Low Cut",
        timeRange (20.0f, 1000.0f, 200.0f), 150.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (hzText)
                                             .withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::reverbDucking), "Reverb Duck",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.35f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::reverbMix), "Reverb Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.2f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    // --- Pump -----------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::pumpEnabled), "Pump On", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::pumpDivision), "Pump Rate",
        toStringArray (fx::divisionNames), (int) fx::Division::n1_4));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::pumpDepth), "Pump Depth",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::pumpShape), "Pump Shape",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::pumpAttack), "Pump Attack",
        timeRange (1.0f, 100.0f, 15.0f), 8.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (msText)
                                             .withLabel ("ms")));

    // --- Salida ---------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::outputWidth), "Width",
        juce::NormalisableRange<float> { 0.0f, 2.0f }, 1.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));

    /*  Graves en mono: requisito de club, no un extra. Ver StereoOutput.h. */
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::outputMonoBelow), "Mono Below",
        timeRange (20.0f, 500.0f, 120.0f), 120.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (hzText)
                                             .withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::outputLimiter), "Limiter", true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::outputGain), "Output",
        juce::NormalisableRange<float> { -24.0f, 12.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (dbText)
                                             .withLabel ("dB")));

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

    //==========================================================================
    // SECUENCIADOR
    //==========================================================================
    /*  Apagado por defecto: un instrumento que arranca a tocar solo cuando se
        aprieta play, sin que nadie le haya mandado MIDI, es una sorpresa. */
    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::seqEnabled), "Seq On", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::seqPattern), "Seq Pattern",
        toStringArray (seq::patternNames), (int) seq::PatternId::offbeat8));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        makeID (pid::seqRate), "Seq Rate",
        toStringArray (seq::rateNames), (int) seq::StepRate::n1_16));

    layout.add (std::make_unique<DiscreteIntParameter> (
        makeID (pid::seqSteps), "Seq Length", 1, seq::kNumSteps, seq::kNumSteps,
        juce::AudioParameterIntAttributes{}.withStringFromValueFunction (
            [] (int v, int) { return juce::String (v) + (v == 1 ? " paso" : " pasos"); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::seqSwing), "Swing",
        juce::NormalisableRange<float> { 0.5f, 0.75f }, 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::seqGate), "Seq Gate",
        juce::NormalisableRange<float> { 0.05f, 1.0f }, 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    /*  Los pasos editables arrancan con el contratiempo de corcheas, sacado de
        la tabla de fabrica: pasar a 'Custom' por primera vez no deja un patron
        vacio, y los defaults no pueden desincronizarse de la tabla. */
    const auto& startingPattern = seq::factoryPattern (seq::PatternId::offbeat8);

    for (size_t i = 0; i < (size_t) seq::kNumSteps; ++i)
    {
        const auto n = juce::String ((int) i + 1).paddedLeft ('0', 2);
        const auto& s = startingPattern[i];

        layout.add (std::make_unique<juce::AudioParameterBool> (
            makeID (pid::seqStepOn[i]), "Step " + n + " On", s.on));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeID (pid::seqStepVelocity[i]), "Step " + n + " Vel",
            juce::NormalisableRange<float> { 0.0f, 1.0f }, s.on ? s.velocity : 0.8f,
            juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeID (pid::seqStepProbability[i]), "Step " + n + " Prob",
            juce::NormalisableRange<float> { 0.0f, 1.0f }, s.probability,
            juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<DiscreteIntParameter> (
            makeID (pid::seqStepRatchet[i]), "Step " + n + " Ratchet", 1, seq::kMaxRatchet, s.ratchet,
            juce::AudioParameterIntAttributes{}.withStringFromValueFunction (
                [] (int v, int) { return v <= 1 ? juce::String ("Off")
                                                : juce::String ("x") + juce::String (v); })));
    }

    return layout;
}
}
