#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

#include "../Chord/ChordTypes.h"

namespace stabr
{
struct GuiEvent
{
    enum class Type
    {
        onset,          // hubo al menos un disparo en el bloque
        notesChanged    // cambiaron las notas sonando sin disparo (se solto el acorde)
    };

    Type type = Type::onset;
    chord::ChordResult notes;
    float velocity = 0.0f;
};

//==============================================================================
/*  Lo unico por donde la GUI se entera de lo que pasa en el hilo de audio.

    EVENTOS: cola lock-free de un productor (audio) y un consumidor (GUI). Si la
    GUI no consume (editor cerrado), la cola se llena y los eventos nuevos se
    DESCARTAN: el hilo de audio nunca espera ni asigna memoria por la GUI.

    ESTADO CONTINUO: atomicos. Son valores que solo importa leer "el ultimo",
    y una cola para eso seria ruido.
*/
class GuiFeed
{
public:
    static constexpr int kCapacity = 128;

    bool push (const GuiEvent& e) noexcept
    {
        const auto scope = fifo.write (1);

        if (scope.blockSize1 + scope.blockSize2 == 0)
            return false;

        buffer[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)] = e;
        return true;
    }

    bool pop (GuiEvent& e) noexcept
    {
        const auto scope = fifo.read (1);

        if (scope.blockSize1 + scope.blockSize2 == 0)
            return false;

        e = buffer[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)];
        return true;
    }

    // Tiempos REALES del delay, con el glide de cinta incluido, y el tempo.
    std::atomic<float> bpm        { 120.0f };
    std::atomic<float> delayTimeL { 0.375f };
    std::atomic<float> delayTimeR { 0.375f };

private:
    juce::AbstractFifo fifo { kCapacity };
    std::array<GuiEvent, (size_t) kCapacity> buffer {};
};
}
