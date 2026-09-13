#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

namespace stabr::gui::sp92
{
/*  Las paginas del LCD. Cada tecla de funcion abre una pagina; apretarla otra
    vez pasa a la subpagina siguiente, como en un sampler. Cada subpagina tiene
    6 casilleros, uno por encoder. Ver DECISIONS.md D-049.

    Entre las paginas y los controles fijos del panel estan TODOS los
    parametros: hay un test que lo verifica. */
struct LcdSlot
{
    const char* paramID = nullptr;   // nullptr = casillero vacio
    const char* label = nullptr;     // hasta 6 caracteres
};

using LcdSubPage = std::array<LcdSlot, 6>;

struct LcdPage
{
    const char* name;
    std::vector<LcdSubPage> subPages;
};

const std::vector<LcdPage>& lcdPages();

/*  Valor de un parametro en 5 o 6 caracteres de LCD: "2.4K", "35%", "NMIN". */
juce::String lcdValue (juce::RangedAudioParameter&);
}
