#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Chord/ChordTypes.h"

namespace stabr::gui
{
/*  Visualizador de acorde (Fase 6, estilo de la direccion C).

    Las notas se ubican por ALTURA, no en un teclado: asi se ve el voicing, la
    separacion entre voces y que nota queda en el bajo, que es lo que cambian
    Voicing, Inversion y Spread. Arriba, el nombre del acorde detectado a
    partir de lo que suena (Chord/ChordNaming.h).

    Solo repinta mientras hay animacion: en reposo no consume nada.
*/
class ChordVisualizer final : public juce::Component
{
public:
    ChordVisualizer();

    void showNotes (const chord::ChordResult& notes, bool isOnset, float velocity, double now);
    void setDetail (const juce::String& detail);
    void tick (double now);

    void paint (juce::Graphics&) override;

    juce::String getDisplayedName() const { return nameFor (shown); }

    static juce::String nameFor (const chord::ChordResult&);
    static juce::String noteName (int midiNote);

private:
    chord::ChordResult shown;
    bool sounding = false;
    double onsetTime = -100.0;
    double releaseTime = -100.0;
    double now = 0.0;
    float velocity = 1.0f;
    juce::String detail;
    bool dirty = true;
    bool wasAnimating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordVisualizer)
};
}
