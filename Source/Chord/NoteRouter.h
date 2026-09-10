#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

#include "ChordTypes.h"

namespace stabr
{
/*  Traduce el MIDI entrante a eventos de disparo ya resueltos.

    Existe por el modo Auto (DECISIONS.md D-012). Cuando llega la primera nota
    no se sabe todavia si el usuario esta tocando UNA tecla (que debe generar
    un acorde) o varias a la vez (que deben pasar tal cual). La unica forma
    honesta de saberlo es esperar una ventana corta, y esa espera es latencia
    que hay que declararle al host.

    Por eso Auto NO es el modo por defecto: 20 ms de latencia tocando en vivo
    es inaceptable. Chord y Passthrough resuelven en el acto, sin latencia.

    El router emite los eventos con su posicion exacta dentro del bloque, para
    que el procesador siga renderizando por sub-bloques. Cuantizar el disparo
    al tamano del buffer arruinaria el timing, que en un instrumento percusivo
    es literalmente el producto.
*/
class NoteRouter
{
public:
    static constexpr int kMaxEvents       = 512;
    static constexpr int kMaxPendingNotes = 8;
    static constexpr float kDefaultAutoWindowMs = 20.0f;

    struct Event
    {
        enum class Type
        {
            chordOn,        // generar acorde a partir de 'note'
            noteOn,         // pasar la nota tal cual
            noteOff,
            allNotesOff,
            allSoundOff,
            pitchBend
        };

        int   offset     = 0;
        Type  type       = Type::noteOn;
        int   note       = 0;
        float velocity   = 1.0f;
        int   pitchWheel = 8192;
    };

    struct EventList
    {
        std::array<Event, kMaxEvents> events {};
        int count = 0;

        void clear() noexcept { count = 0; }

        void add (const Event& e) noexcept
        {
            if (count < kMaxEvents)
                events[(size_t) count++] = e;
        }

        const Event& operator[] (int i) const noexcept { return events[(size_t) i]; }
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setNoteMode (chord::NoteMode m) noexcept;
    chord::NoteMode getNoteMode() const noexcept { return mode; }

    void setAutoWindowMs (float ms) noexcept;

    /*  Latencia a declarar al host: solo el modo Auto tiene. */
    int getLatencySamples() const noexcept;

    /*  Convierte el buffer MIDI del bloque en eventos resueltos y ordenados. */
    void process (const juce::MidiBuffer& midi, int numSamples, EventList& out) noexcept;

private:
    struct PendingNote
    {
        int note = 0;
        float velocity = 1.0f;
    };

    void firePending (int offset, EventList& out) noexcept;
    void flushPendingImmediately (int offset, EventList& out) noexcept;
    bool isPending (int note) const noexcept;

    double sampleRate = 44100.0;
    chord::NoteMode mode = chord::NoteMode::chord;
    float autoWindowMs = kDefaultAutoWindowMs;
    int autoWindowSamples = 0;

    std::array<PendingNote, (size_t) kMaxPendingNotes> pending {};
    int pendingCount = 0;

    // Muestras que faltan para que el grupo pendiente dispare, relativo al
    // comienzo del bloque actual. Puede quedar negativo entre bloques.
    int pendingCountdown = 0;
};
}
