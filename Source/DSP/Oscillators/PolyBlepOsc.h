#pragma once

#include <cmath>
#include <algorithm>

namespace stabr
{
/*  Oscilador PolyBLEP.

    Fase 1 implementa solo sierra. Fase 3 agrega pulso con PWM y triangulo
    (integrando el pulso); la forma tipo organo/M1 NO va aca: es una suma fija
    de armonicos y necesita wavetable band-limited. Ver DECISIONS.md D-004.

    Diseño en SoA-friendly: la fase es un miembro simple y el proceso no toca
    estado compartido, para poder SIMDizar las copias de unisono en Fase 3 sin
    reescribir nada.
*/
class PolyBlepOsc
{
public:
    void setSampleRate (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        setFrequency (frequency);
    }

    void setFrequency (float hz) noexcept
    {
        // Se clampea bajo Nyquist: un phaseInc >= 0.5 rompe el PolyBLEP y
        // produce basura, no aliasing "musical".
        frequency = std::clamp (hz, 0.0f, (float) (sampleRate * 0.5) - 1.0f);
        phaseInc  = (float) (frequency / sampleRate);
    }

    /*  phase01 en 0..1. Fase fija = transitorio consistente entre notas (clave
        para que un stab suene igual cada vez que lo disparas). Fase aleatoria =
        unisono mas gordo. Los dos modos se exponen en Fase 3. */
    void reset (float phase01 = 0.0f) noexcept
    {
        phase = phase01 - std::floor (phase01);
    }

    float getPhase() const noexcept { return phase; }

    inline float processSaw() noexcept
    {
        const float value = 2.0f * phase - 1.0f;          // sierra ingenua
        const float corrected = value - polyBlep (phase); // correccion del salto

        advance();
        return corrected;
    }

private:
    inline void advance() noexcept
    {
        phase += phaseInc;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }

    /*  Residuo PolyBLEP de 2 muestras. Suaviza la discontinuidad del salto de
        la sierra, que es de donde sale casi todo el aliasing. */
    inline float polyBlep (float t) const noexcept
    {
        const float dt = phaseInc;

        if (dt <= 0.0f)
            return 0.0f;

        if (t < dt)                 // justo despues del salto
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }

        if (t > 1.0f - dt)          // justo antes del salto
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }

        return 0.0f;
    }

    double sampleRate = 44100.0;
    float frequency   = 440.0f;
    float phaseInc    = 0.0f;
    float phase       = 0.0f;
};
}
