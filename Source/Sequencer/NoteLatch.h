#pragma once

#include <array>
#include "../Chord/ChordTypes.h"

namespace stabr::seq
{
/*  Lo que el secuenciador va a disparar. Ver DECISIONS.md D-017.

    LATCH: se aprieta una tecla, se suelta, y el secuenciador sigue disparando
    ese acorde. Sin esto habria que mantener la tecla apretada todo el tema, y
    no se cumple la promesa de sacar un stab en 30 segundos.

    La regla para armar el acorde latcheado es la de una mano en el teclado:
    mientras haya teclas apretadas, cada tecla nueva SE SUMA (se esta armando
    un acorde a mano); la primera tecla despues de soltar todo EMPIEZA uno
    nuevo. Soltar no borra nada.
*/
class NoteLatch
{
public:
    static constexpr int kMaxNotes = chord::kMaxChordNotes;

    void reset() noexcept
    {
        heldCount = 0;
        latchedCount = 0;
        lastNote = -1;
    }

    void noteOn (int note, float /*velocity*/) noexcept
    {
        if (heldCount == 0)
            latchedCount = 0;   // mano nueva: acorde nuevo

        addUnique (held, heldCount, note);
        addUnique (latched, latchedCount, note);
        lastNote = note;
    }

    void noteOff (int note) noexcept
    {
        for (int i = 0; i < heldCount; ++i)
        {
            if (held[(size_t) i] == note)
            {
                held[(size_t) i] = held[(size_t) (heldCount - 1)];
                --heldCount;
                return;
            }
        }
    }

    /*  All-notes-off del host: se olvidan las teclas APRETADAS, pero el acorde
        latcheado queda. Muchos DAWs mandan all-notes-off al parar el
        transporte, y borrar el latch en cada stop lo haria inservible. Lo que
        si hace falta es no quedar con teclas "apretadas" fantasma, que harian
        que la proxima tecla se sume en vez de empezar un acorde nuevo. */
    void releaseAllHeld() noexcept { heldCount = 0; }

    bool isEmpty() const noexcept     { return latchedCount == 0; }
    int  size() const noexcept        { return latchedCount; }
    int  getLastNote() const noexcept { return lastNote; }
    int  getHeldCount() const noexcept { return heldCount; }

    /*  Las notas latcheadas de grave a agudo, en el formato del motor de voces. */
    chord::ChordResult getLatchedNotes() const noexcept
    {
        chord::ChordResult r;

        for (int i = 0; i < latchedCount; ++i)
            r.notes[(size_t) i] = latched[(size_t) i];

        r.count = latchedCount;

        for (int i = 1; i < r.count; ++i)
        {
            const int key = r.notes[(size_t) i];
            int j = i - 1;

            while (j >= 0 && r.notes[(size_t) j] > key)
            {
                r.notes[(size_t) (j + 1)] = r.notes[(size_t) j];
                --j;
            }

            r.notes[(size_t) (j + 1)] = key;
        }

        return r;
    }

private:
    static void addUnique (std::array<int, (size_t) kMaxNotes>& list, int& count, int note) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (list[(size_t) i] == note)
                return;

        if (count < kMaxNotes)
            list[(size_t) count++] = note;
    }

    std::array<int, (size_t) kMaxNotes> held {};
    std::array<int, (size_t) kMaxNotes> latched {};
    int heldCount = 0;
    int latchedCount = 0;
    int lastNote = -1;
};
}
