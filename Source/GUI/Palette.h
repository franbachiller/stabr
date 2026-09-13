#pragma once

#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <cmath>

/*  Identidad visual "Maquina" (Fase 6, direccion A): hardware de estudio.
    Grafito, un solo acento naranja de LED, texto calido. Ver DECISIONS.md D-042.

    Todos los colores viven aca. Un color escrito a mano en un componente es un
    color que el test de contraste no ve. */
namespace stabr::gui
{
namespace colours
{
    inline const juce::Colour background { 0xff1b1b1d };
    inline const juce::Colour panel      { 0xff242427 };
    inline const juce::Colour raised     { 0xff2f2f34 };
    inline const juce::Colour line       { 0xff3c3c42 };
    inline const juce::Colour text       { 0xffebe8e1 };
    inline const juce::Colour textMuted  { 0xff9d9a92 };
    inline const juce::Colour accent     { 0xffff7a1a };
    inline const juce::Colour accentSoft { 0xffffb36b };
    inline const juce::Colour knobBody   { 0xff34343a };
    inline const juce::Colour knobEdge   { 0xff4a4a52 };
    inline const juce::Colour ledOff     { 0xff4b3a2c };
}

/*  Contraste WCAG 2.x. El test de accesibilidad lo usa sobre esta paleta. */
inline double relativeLuminance (juce::Colour c) noexcept
{
    auto channel = [] (juce::uint8 v)
    {
        const double s = v / 255.0;
        return s <= 0.03928 ? s / 12.92 : std::pow ((s + 0.055) / 1.055, 2.4);
    };

    return 0.2126 * channel (c.getRed()) + 0.7152 * channel (c.getGreen()) + 0.0722 * channel (c.getBlue());
}

inline double contrastRatio (juce::Colour a, juce::Colour b) noexcept
{
    const double la = relativeLuminance (a) + 0.05;
    const double lb = relativeLuminance (b) + 0.05;
    return std::max (la, lb) / std::min (la, lb);
}

/*  Fuentes del sistema, no embebidas: no hay nada que licenciar ni que
    redistribuir. Ver LICENSING.md. */
namespace fonts
{
    inline juce::Font label (float height)
    {
        return juce::FontOptions (height);
    }

    inline juce::Font value (float height)
    {
        return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain);
    }

    inline juce::Font title (float height)
    {
        return juce::FontOptions (height, juce::Font::bold).withKerningFactor (0.12f);
    }
}
}
