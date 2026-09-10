#include "ParameterLayout.h"
#include "ParameterIDs.h"

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

    // --- Global ---------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        makeID (pid::masterGain), "Master Gain",
        juce::NormalisableRange<float> { -60.0f, 6.0f, 0.1f }, -6.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction (dbText)
                                             .withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        makeID (pid::polyphony), "Polyphony", 1, 16, 8));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        makeID (pid::pitchBendRange), "Pitch Bend Range", 0, 24, 2,
        juce::AudioParameterIntAttributes{}.withStringFromValueFunction (
            [] (int v, int) { return semitoneText ((float) v, 0); })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        makeID (pid::hqMode), "HQ Mode", false));

    return layout;
}
}
