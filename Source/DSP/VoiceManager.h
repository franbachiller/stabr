#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>

#include "Voice.h"
#include "../Chord/ChordTypes.h"

namespace stabr
{
/*  Pool fijo de voces con robo POR ACORDE.

    Ver DECISIONS.md D-001. La unidad de asignacion no es la nota: es el
    acorde. Robarle la septima a un min7 para meter la fundamental del acorde
    siguiente no suena a "se acabo la polifonia", suena a bug. Cuando hace
    falta lugar se roba un acorde entero, con su fade, y el acorde nuevo entra
    completo o no entra.

    "Polifonia" es la cantidad de DISPAROS simultaneos: acordes en modo Chord,
    notas sueltas en modo Passthrough. Un acorde soltado deja de contar para el
    limite pero sus voces siguen sonando hasta que la envolvente termina, que
    es lo que hace que los stabs encadenados se superpongan de forma natural.
*/
class VoiceManager
{
public:
    static constexpr int kMaxChords  = 8;
    static constexpr int kMaxPolyphony = kMaxChords;

    // Holgura sobre el maximo teorico: los acordes soltados siguen ocupando
    // voces mientras decaen, y los robados mientras dura su fade.
    static constexpr int kPoolSize = kMaxChords * chord::kMaxChordNotes
                                   + 2 * chord::kMaxChordNotes;

    void prepare (double sampleRate, int maximumBlockSize);
    void reset() noexcept;

    void setMaxPolyphony (int n) noexcept;
    void setEnvelopeParameters (float attackMs, float decayMs, float sustain, float releaseMs) noexcept;
    void setLevel (float linearGain) noexcept;
    void setPitchBendSemitones (float semis) noexcept;

    /*  Dispara un acorde completo. triggerNote es la tecla que lo genero: es
        la que hay que pasarle despues a noteOff. */
    void noteOnChord (int triggerNote, const chord::ChordResult& notes, float velocity) noexcept;

    /*  Atajo para una sola nota (modo passthrough y tests). */
    void noteOnSingle (int midiNote, float velocity) noexcept;

    void noteOff (int triggerNote) noexcept;
    void allNotesOff (bool allowTailOff) noexcept;

    void renderAdd (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

    int  getActiveVoiceCount() const noexcept;
    bool hasActiveVoices() const noexcept;
    int  getSoundingChordCount() const noexcept;

    /*  Notas que estan sonando ahora mismo, para el visualizador de acordes de
        la Fase 6. Lee estado del hilo de audio, asi que en la GUI se consume a
        traves de un FIFO, nunca llamando esto directo. */
    chord::ChordResult getSoundingNotes() const noexcept;

private:
    struct ChordSlot
    {
        int triggerNote = -1;
        int chordId     = -1;
        std::uint64_t stamp = 0;
        bool sounding   = false;   // false = soltado, pero sus voces pueden seguir
    };

    Voice* findFreeVoice() noexcept;
    ChordSlot* findFreeSlot() noexcept;
    ChordSlot* findOldestSoundingSlot() noexcept;
    ChordSlot* findSlotForTrigger (int triggerNote) noexcept;
    void stealChord (ChordSlot& slot) noexcept;
    void releaseChord (ChordSlot& slot) noexcept;
    bool hasVoicesFor (int chordId) const noexcept;
    void reapFinishedSlots() noexcept;

    std::array<Voice, (size_t) kPoolSize> voices;
    std::array<ChordSlot, (size_t) kMaxChords * 2> slots;   // holgura para soltados
    juce::AudioBuffer<float> scratch;   // canal derecho cuando la salida es mono

    double sampleRate   = 44100.0;
    int maxPolyphony    = 6;
    std::uint64_t stampCounter = 0;
    int nextChordId     = 0;
};
}
