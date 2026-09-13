#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

/*  Los tres acabados del SP-92. Ver DECISIONS.md D-049.

    Cada acabado es una paleta completa: chasis, franja, serigrafia, pads, pad
    STAB y el color de la pantalla. El LCD cambia con el acabado a proposito:
    verde amarillento en el beige, ambar en el grafito, azul en el artico. */
namespace stabr::gui::sp92
{
struct Finish
{
    const char* name;

    juce::Colour chassis, chassisHi, chassisLo;
    juce::Colour strip, stripInk;
    juce::Colour silk, silkMuted;
    juce::Colour well;
    juce::Colour pad, padHi, padLo;
    juce::Colour stab, stabHi, stabLo;
    juce::Colour led;
    juce::Colour lcdBg1, lcdBg2, lcdOn, lcdOff, lcdGlow;
    juce::Colour key, keyHi, keyLo, keyInk;
    bool lcdGlows;
};

inline constexpr int kNumFinishes = 3;

inline const Finish& finish (int index)
{
    using C = juce::Colour;

    static const std::array<Finish, kNumFinishes> table
    {{
        { "Beige '87",
          C (0xffd8cfbc), C (0xffebe4d3), C (0xffb9af9a),
          C (0xff34312c), C (0xffebe3d1),
          C (0xff3b352c), C (0xff7d7363),
          C (0xffb4aa95),
          C (0xff4a4844), C (0xff605d58), C (0xff34322f),
          C (0xffe2531e), C (0xffff7a3d), C (0xffa83a12),
          C (0xffff6b1c),
          C (0xffb7cf46), C (0xff93ad2c), C (0xff1c2708), C::fromRGBA (28, 39, 8, 23), C::fromRGBA (0, 0, 0, 0),
          C (0xffcfc6b2), C (0xffe2dac9), C (0xffaea490), C (0xff3b352c),
          false },

        { "Graphite '92",
          C (0xff3a3c40), C (0xff4d5055), C (0xff2a2c2f),
          C (0xff1a1b1d), C (0xffe9e6df),
          C (0xffdcd8cf), C (0xff8f8b83),
          C (0xff2a2b2e),
          C (0xff2c2d30), C (0xff3d3f43), C (0xff1f2022),
          C (0xffd8401f), C (0xfff2663a), C (0xff8f2610),
          C (0xffff8a1f),
          C (0xff1c1408), C (0xff120c04), C (0xffffb22a), C::fromRGBA (255, 178, 42, 18), C::fromRGBA (255, 150, 20, 191),
          C (0xff2f3134), C (0xff43464a), C (0xff232427), C (0xffdcd8cf),
          true },

        { "Arctic '89",
          C (0xffc7cbd0), C (0xffdde1e5), C (0xffa8adb4),
          C (0xff243349), C (0xffe6edf6),
          C (0xff28313d), C (0xff6d7684),
          C (0xffa9aeb5),
          C (0xff585d64), C (0xff6f757d), C (0xff40444a),
          C (0xffee6420), C (0xffff8a4a), C (0xffb04312),
          C (0xffff7426),
          C (0xff2f62d9), C (0xff2046a8), C (0xffeef5ff), C::fromRGBA (238, 245, 255, 26), C::fromRGBA (210, 230, 255, 204),
          C (0xffd2d6db), C (0xffe6e9ec), C (0xffaeb3ba), C (0xff28313d),
          true },
    }};

    return table[(size_t) juce::jlimit (0, kNumFinishes - 1, index)];
}

/*  Teclas oscuras (disco, funciones, SEQ): iguales en los tres acabados, como
    las teclas negras de un aparato real. */
namespace darkKey
{
    inline const juce::Colour base   { 0xff3a3836 };
    inline const juce::Colour hi     { 0xff4d4a47 };
    inline const juce::Colour lo     { 0xff292826 };
    inline const juce::Colour ink    { 0xffece4d4 };
}
}
