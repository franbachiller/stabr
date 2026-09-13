#pragma once

#include <juce_graphics/juce_graphics.h>

/*  Tipografias del SP-92.

    Las definitivas son Michroma (placa del modelo), Archivo Narrow (serigrafia)
    y Reenie Beanie (lo escrito a mano), las tres de Google Fonts bajo SIL OFL
    1.1, que permite embeberlas en software comercial. Si estan en
    Assets/Fonts, CMake las embebe y define STABR_HAS_EMBEDDED_FONTS.

    Sin ellas se usan equivalentes de Windows: Bahnschrift (una DIN condensada
    que viene con Windows 10 y 11) y Segoe Print. Asi la interfaz nunca queda
    con una fuente por defecto. Ver LICENSING.md. */
namespace stabr::gui::sp92::fonts
{
juce::Font display (float height);                   // placa del modelo, STAB
juce::Font label (float height, bool bold = true);   // serigrafia del panel
juce::Font hand (float height);                      // etiquetas y cinta

bool usingEmbeddedFonts() noexcept;
}
