#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

#include "../Sequencer/Patterns.h"

namespace stabr::gui
{
/*  IDs de los 4 parametros de cada paso, separados por coma. Lo usa el test
    que verifica que todo parametro tenga control. */
inline const juce::Identifier stepParamIdsProperty { "stabrStepParamIDs" };

/*  Los 16 pasos del secuenciador.

    Clic enciende o apaga; arrastrar vertical ajusta la velocity; clic derecho
    abre ratchet y probabilidad. Con teclado: espacio enciende, flechas arriba
    y abajo mueven la velocity, R cicla el ratchet, izquierda y derecha pasan
    de paso.

    Si el patron elegido es de fabrica, la grilla lo MUESTRA, y la primera
    edicion lo copia a los pasos editables y pasa a Custom (D-039): editar
    nunca parte de un patron vacio ni pisa uno de fabrica.
*/
class StepGrid final : public juce::Component
{
public:
    explicit StepGrid (juce::AudioProcessorValueTreeState&);
    ~StepGrid() override;

    void resized() override;
    void tick (int playingStep);

    seq::Step getDisplayedStep (int index) const;
    bool isCustomPattern() const;
    int getLength() const;

    void toggleStep (int index);
    void setVelocity (int index, float velocity);
    void setProbability (int index, float probability);
    void setRatchet (int index, int ratchet);

    void focusStep (int index);

private:
    class Cell;

    void makeCustomIfNeeded();
    void setPlain (const char* paramID, float plainValue);

    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::unique_ptr<Cell>, (size_t) seq::kNumSteps> cells;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGrid)
};
}
