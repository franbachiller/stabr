#pragma once

#include <juce_core/juce_core.h>

namespace stabr
{
/*  Convierte un literal UTF-8 del codigo fuente en juce::String.

    POR QUE EXISTE: juce::String (const char*) NO interpreta UTF-8. Trata cada
    byte como un caracter Latin-1, asi que cualquier texto con acentos, simbolos
    o comillas tipograficas sale roto. El "·" (bytes C2 B7) se ve como "Â·",
    "Inversión" como "InversiÃ³n", y asi.

    Lo detectamos en la v0.1 con el titulo del editor, que en pantalla decia
    "STABR Â· fase 1". Es un bug silencioso: compila, no tira warning, y solo
    se nota mirando la GUI.

    Regla: TODO texto visible que no sea ASCII de 7 bits pasa por aca.
*/
inline juce::String utf8 (const char* literal)
{
    return juce::String (juce::CharPointer_UTF8 (literal));
}
}
