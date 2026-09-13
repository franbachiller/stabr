#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "Finish.h"

namespace stabr::gui::sp92
{
/*  Todo lo que no se mueve del aparato: plastico con textura, franja
    superior, placa del modelo, marcos de las pantallas, serigrafia, pozo de
    los pads, placa de serie y tornillos.

    Se pinta una vez y queda en una imagen (setBufferedToImage): cambia solo
    con el acabado o la escala, nunca en la animacion.

    Medidas y tipografia salen del CSS de la maqueta aprobada: y es el borde
    superior de una caja de line-height 1, como en la maqueta. */
class ChassisLayer final : public juce::Component
{
public:
    ChassisLayer();

    void setFinish (const Finish&);

    /*  Serigrafia: 11 px .16 em, o 9 px .14 em en gris si small. */
    void addSilk (const juce::String& text, float x, float y, bool centred = true, bool small = false);
    void addHead (const juce::String& text, juce::Rectangle<float> area);
    void addBezel (juce::Rectangle<float> area, const juce::String& tinyText = {});
    void addWell (juce::Rectangle<float> area);

    void paint (juce::Graphics&) override;

private:
    struct Silk { juce::String text; float x, y; bool centred, small; };
    struct Head { juce::String text; juce::Rectangle<float> area; };
    struct Bezel { juce::Rectangle<float> area; juce::String tiny; };

    const Finish* fin;
    std::vector<Silk> silks;
    std::vector<Head> heads;
    std::vector<Bezel> bezels;
    std::vector<juce::Rectangle<float>> wells;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChassisLayer)
};
}
