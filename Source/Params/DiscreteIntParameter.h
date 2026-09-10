#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace stabr
{
/*  juce::AudioParameterInt con isDiscrete() correcto.

    EL PROBLEMA: AudioParameterInt sobreescribe getNumSteps() pero NO
    isDiscrete(), que queda en el false de la clase base. El wrapper VST3 de
    JUCE decide el stepCount asi (juce_audio_plugin_client_VST3.cpp):

        if (! param.isDiscrete())
            return 0;

    O sea que todo AudioParameterInt se exporta al host como parametro
    CONTINUO. El valor se redondea bien puertas adentro, pero el host dibuja un
    control continuo y automatiza en rampa suave donde deberia haber escalones.
    En un parametro como Polyphony (1..16) eso se ve y se siente mal.

    Detectado leyendo el log de pluginval: reportaba
    "numSteps - 2147483647, isDiscrete - 0" para Polyphony y Pitch Bend Range.
    pluginval no lo marca como fallo porque es legal, pero es un defecto.

    Esta subclase solo agrega el override que falta. No cambia el ID, ni el
    rango, ni el mapeo de valores: es seguro cambiar un AudioParameterInt
    existente por este sin romper estados guardados.
*/
class DiscreteIntParameter final : public juce::AudioParameterInt
{
public:
    using juce::AudioParameterInt::AudioParameterInt;

    bool isDiscrete() const override { return true; }
};
}
