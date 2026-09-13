#pragma once

#include <juce_graphics/juce_graphics.h>

/*  Tipografias del SP-92: Michroma (marca, modelo, STAB), Archivo Narrow 600 y
    700 (serigrafia y teclas) y Reenie Beanie (lo escrito a mano). Las tres de
    Google Fonts bajo SIL OFL 1.1, embebidas desde Resources/Fonts. Ver
    LICENSING.md.

    Los tamanos se piden como en el CSS del mockup: en px de em y con el
    letter-spacing en em. JUCE mide la altura como ascendente + descendente y el
    kerning como proporcion de esa altura; aca se convierte para que 11 px con
    .16 em se vean igual que en la maqueta aprobada. */
namespace stabr::gui::sp92::fonts
{
juce::Font display   (float px, float trackingEm = 0.0f);   // Michroma
juce::Font label     (float px, float trackingEm = 0.0f);   // Archivo Narrow 600
juce::Font labelBold (float px, float trackingEm = 0.0f);   // Archivo Narrow 700
juce::Font hand      (float px);                            // Reenie Beanie

bool usingEmbeddedFonts() noexcept;

/*  Ancho del texto incluido el espaciado, como lo mide un navegador. */
float textWidth (const juce::Font&, const juce::String&);

/*  Texto en una caja de line-height 1: centrado vertical en la caja, y en
    horizontal a la izquierda, centrado o a la derecha. */
void drawText (juce::Graphics&, const juce::String&, const juce::Font&,
               juce::Rectangle<float> box, juce::Justification);
}
