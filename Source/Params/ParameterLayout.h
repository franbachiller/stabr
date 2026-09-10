#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace stabr
{
    /*  Construye el layout completo de parametros. Cada parametro define rango,
        skew, unidad y un formateador legible: el host tiene que mostrar
        "220 ms" y "-6.0 dB", nunca "0.42".
    */
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
