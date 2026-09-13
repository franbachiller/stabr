#pragma once

#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <cmath>

/*  Contraste WCAG 2.x. El test de accesibilidad lo usa sobre las paletas de
    los tres acabados del SP-92. */
namespace stabr::gui
{
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
}
