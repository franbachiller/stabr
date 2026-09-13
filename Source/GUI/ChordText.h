#pragma once

#include <juce_core/juce_core.h>
#include "../Chord/ChordNaming.h"

namespace stabr::gui
{
inline juce::String noteName (int midiNote)
{
    const int n = juce::jlimit (0, 127, midiNote);
    return juce::String (chord::keyNames[(size_t) (n % 12)]) + juce::String (n / 12 - 1);
}

/*  Nombre del acorde que SUENA (ver Chord/ChordNaming.h). Si no tiene nombre
    conocido (una nota sola, un cluster), las notas: es lo honesto. */
inline juce::String chordNameFor (const chord::ChordResult& notes)
{
    if (notes.count <= 0)
        return {};

    const auto name = chord::nameChord (notes);

    if (name.valid)
    {
        juce::String s = juce::String (chord::keyNames[(size_t) name.root]) + juce::String (name.quality);

        if (name.bass != name.root)
            s << "/" << chord::keyNames[(size_t) name.bass];

        return s;
    }

    juce::String s;

    for (int i = 0; i < juce::jmin (notes.count, 4); ++i)
        s << (i > 0 ? " " : "") << chord::keyNames[(size_t) (((notes[i] % 12) + 12) % 12)];

    return s;
}
}
