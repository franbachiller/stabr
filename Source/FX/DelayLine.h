#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace stabr::fx
{
/*  Linea de retardo con interpolacion Lagrange de 3er orden y puntero de
    lectura que se desliza. Ver DECISIONS.md D-015.

    POR QUE PROPIA Y NO juce::dsp::DelayLine, dos motivos:

    1. Hace falta un punto de insercion DENTRO del lazo de realimentacion
       (filtros HP/LP, saturacion, pasabanda del modo dub). La clase de JUCE no
       lo ofrece; habria que hacer pop/push a mano, y en ese momento ya
       escribiste tu propia linea.

    2. El comportamiento de CINTA necesita que el puntero de lectura se mueva
       CONTINUAMENTE hacia el tiempo nuevo. Eso es lo que produce el pitch
       drift: si la cabeza se aleja, el audio baja de tono. juce::dsp::DelayLine
       no expone el puntero, solo setDelay().

    INTERPOLACION: Lagrange de 3er orden, no lineal.
    La interpolacion lineal es un pasabajos cuya frecuencia de corte depende de
    la parte fraccionaria. Con el puntero en movimiento (wow & flutter, o un
    cambio de tiempo) eso modula el brillo, y se escucha. Allpass tiene estado y
    hace ruido cuando el puntero se mueve rapido. Lagrange-3 es el punto justo
    para punteros en movimiento.
*/
class DelayLine
{
public:
    /*  Tope de retardo. 8 s cubre 4 compases a 120 BPM y 2 compases a 60 BPM.
        Mas que eso son ~12 MB por canal a 192 kHz para algo que casi nadie usa;
        las divisiones que no entran se clampean y se documenta. */
    static constexpr float kMaxDelaySeconds = 8.0f;

    enum class GlideMode
    {
        tape = 0,   // el puntero se desliza: cambia el tono, como una cinta
        digital,    // salto con crossfade: no cambia el tono
        numModes
    };

    /*  Reserva memoria: solo desde prepareToPlay, nunca desde el hilo de audio. */
    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;

        const int needed = (int) std::ceil (kMaxDelaySeconds * sampleRate) + 8;
        buffer.assign ((size_t) needed, 0.0f);
        size = needed;

        // Constante de tiempo del deslizamiento en modo cinta. 100 ms da un
        // glissando audible pero que no se queda colgado.
        glideCoeff = 1.0f - std::exp (-1.0f / (float) (0.100 * sampleRate));

        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
        currentDelay = targetDelay;
        crossfade = 1.0f;
        previousDelay = targetDelay;
    }

    void setGlideMode (GlideMode m) noexcept { glideMode = m; }

    /*  Tiempo de retardo objetivo, en segundos. */
    void setDelaySeconds (float seconds) noexcept
    {
        const auto clamped = std::clamp (seconds, 0.0f, kMaxDelaySeconds);
        const auto samples = clamped * (float) sampleRate;

        if (std::abs (samples - targetDelay) < 0.001f)
            return;

        if (glideMode == GlideMode::digital)
        {
            // Crossfade desde la posicion vieja a la nueva: sin cambio de tono,
            // a costa de un breve solapamiento de las dos lecturas.
            previousDelay = currentDelay;
            crossfade = 0.0f;
            currentDelay = samples;
        }

        targetDelay = samples;
    }

    /*  Fijar el retardo de una, sin deslizamiento (al preparar o al resetear). */
    void snapToTarget() noexcept
    {
        currentDelay = targetDelay;
        previousDelay = targetDelay;
        crossfade = 1.0f;
    }

    float getCurrentDelaySamples() const noexcept { return currentDelay; }

    //==========================================================================
    /*  Escribe una muestra y avanza. */
    inline void write (float x) noexcept
    {
        buffer[(size_t) writeIndex] = x;
        writeIndex = (writeIndex + 1) % size;
    }

    /*  Avanza el deslizamiento del puntero. Se llama UNA vez por muestra,
        antes de leer. */
    inline void advanceGlide() noexcept
    {
        if (glideMode == GlideMode::tape)
        {
            /*  Un polo, sin limite duro de velocidad: un cambio grande arranca
                con un glissando marcado y se asienta suave. Eso ES el sonido de
                cinta; limitar la velocidad lo volveria un portamento lineal, que
                suena a sintetizador, no a maquina. */
            currentDelay += (targetDelay - currentDelay) * glideCoeff;
        }
        else
        {
            currentDelay = targetDelay;

            if (crossfade < 1.0f)
                crossfade = std::min (1.0f, crossfade + crossfadeStep);
        }
    }

    /*  Lee con un desplazamiento extra (modulacion de wow & flutter), en
        muestras. */
    inline float read (float modulationSamples = 0.0f) const noexcept
    {
        const float delay = std::clamp (currentDelay + modulationSamples,
                                        1.0f, (float) (size - 4));

        const float current = readAt (delay);

        if (glideMode == GlideMode::digital && crossfade < 1.0f)
        {
            const float old = readAt (std::clamp (previousDelay + modulationSamples,
                                                  1.0f, (float) (size - 4)));
            return old + (current - old) * crossfade;
        }

        return current;
    }

private:
    /*  Lagrange de 3er orden sobre cuatro puntos: y(-1), y(0), y(1), y(2). */
    inline float readAt (float delaySamples) const noexcept
    {
        float readPos = (float) writeIndex - delaySamples;

        while (readPos < 0.0f)
            readPos += (float) size;

        const int i1 = (int) readPos;
        const float frac = readPos - (float) i1;

        const int i0 = (i1 - 1 + size) % size;
        const int i2 = (i1 + 1) % size;
        const int i3 = (i1 + 2) % size;

        const float y0 = buffer[(size_t) i0];
        const float y1 = buffer[(size_t) (i1 % size)];
        const float y2 = buffer[(size_t) i2];
        const float y3 = buffer[(size_t) i3];

        // Forma de Horner de los polinomios de Lagrange para d en 0..1.
        const float d1 = frac - 1.0f;
        const float d2 = frac - 2.0f;
        const float dp1 = frac + 1.0f;

        const float c0 = -d1 * d2 * frac / 6.0f;
        const float c1 = dp1 * d1 * d2 * 0.5f;
        const float c2 = -dp1 * frac * d2 * 0.5f;
        const float c3 = dp1 * frac * d1 / 6.0f;

        return y0 * c0 + y1 * c1 + y2 * c2 + y3 * c3;
    }

    std::vector<float> buffer;
    int size = 0;
    int writeIndex = 0;

    double sampleRate = 44100.0;

    float targetDelay   = 0.0f;
    float currentDelay  = 0.0f;
    float previousDelay = 0.0f;
    float glideCoeff    = 0.001f;

    float crossfade = 1.0f;
    static constexpr float crossfadeStep = 1.0f / 1024.0f;

    GlideMode glideMode = GlideMode::tape;
};
}
