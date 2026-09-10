#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace stabr
{
/*  Forzador de silencio exacto.

    El test de silencio del brief pide salida exactamente 0.0. Las colas de
    delay y reverb decaen asintoticamente y nunca llegan a cero: hay que
    detectar la cola y cortarla a mano. Esto tambien ahorra CPU cuando el
    plugin esta en reposo.

    Uso por bloque:
        gate.setHasSound (voiceManager.hasActiveVoices());
        if (gate.processAndShouldClear (buffer)) buffer.clear();
*/
class SilenceGate
{
public:
    void prepare (double sampleRate) noexcept
    {
        // Cuantos bloques consecutivos por debajo del umbral antes de cortar.
        // ~200 ms de margen para no cortar una cola todavia audible.
        blocksToConfirm = juce::jmax (2, (int) (0.2 * sampleRate / 256.0));
        reset();
    }

    void reset() noexcept
    {
        quietBlocks = 0;
        silent      = false;
    }

    void setHasSound (bool hasSound) noexcept
    {
        if (hasSound)
        {
            quietBlocks = 0;
            silent      = false;
        }
    }

    /*  Devuelve true si el buffer debe forzarse a cero exacto. */
    bool processAndShouldClear (const juce::AudioBuffer<float>& buffer) noexcept
    {
        if (silent)
            return true;

        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));

        if (peak > kThreshold)
        {
            quietBlocks = 0;
            return false;
        }

        if (++quietBlocks >= blocksToConfirm)
        {
            silent = true;
            return true;
        }

        return false;
    }

    bool isSilent() const noexcept { return silent; }

private:
    // -120 dBFS. Por debajo de esto no hay nada audible ni en el peor sistema.
    static constexpr float kThreshold = 1.0e-6f;

    int blocksToConfirm = 40;
    int quietBlocks     = 0;
    bool silent         = false;
};
}
