#pragma once

#include <cmath>
#include <algorithm>

namespace stabr
{
/*  Oscilador PolyBLEP: sierra, pulso con PWM y triangulo.

    La forma tipo organo/M1 NO va aca: es una suma fija de armonicos y necesita
    wavetable band-limited. Ver OrganTable.h y DECISIONS.md D-004.

    Diseno SoA-friendly: la fase es un miembro simple y el proceso no toca
    estado compartido, para poder SIMDizar las copias de unisono sin reescribir.
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

    float getFrequency() const noexcept { return frequency; }

    /*  phase01 en 0..1. Fase fija = transitorio consistente entre notas (clave
        para que un stab suene igual cada vez que lo disparas). Fase aleatoria =
        unisono mas gordo. */
    void reset (float phase01 = 0.0f) noexcept
    {
        phase = phase01 - std::floor (phase01);
    }

    float getPhase() const noexcept { return phase; }

    //==========================================================================
    inline float processSaw() noexcept
    {
        const float value = 2.0f * phase - 1.0f;          // sierra ingenua
        const float corrected = value - polyBlep (phase); // correccion del salto

        advance();
        return corrected;
    }

    /*  pulseWidth en 0..1. Se clampea para que el pulso nunca desaparezca:
        con pw en 0 o 1 la salida seria DC constante. */
    inline float processPulse (float pulseWidth) noexcept
    {
        const float pw = std::clamp (pulseWidth, 0.02f, 0.98f);

        float value = phase < pw ? 1.0f : -1.0f;

        // Flanco de subida en 0 (salto +2) y de bajada en pw (salto -2).
        value += polyBlep (phase);

        float fallingEdge = phase - pw;
        if (fallingEdge < 0.0f)
            fallingEdge += 1.0f;

        value -= polyBlep (fallingEdge);

        advance();
        return value;
    }

    /*  Triangulo ingenuo, sin correccion de banda limitada.

        A DIFERENCIA de sierra y pulso, el triangulo no tiene saltos de valor:
        solo de pendiente. Sus armonicos caen como 1/n^2 en vez de 1/n, asi que
        su aliasing arranca muchisimo mas abajo. Corregirlo requeriria polyBLAMP
        (otra familia de residuos, mas caros).

        Hay un test que MIDE su aliasing contra el de la sierra con PolyBLEP: si
        alguna vez queda por encima, se justifica agregar polyBLAMP. Mientras
        este por debajo, no.
    */
    inline float processTriangle() noexcept
    {
        const float value = phase < 0.5f ? (4.0f * phase - 1.0f)
                                         : (3.0f - 4.0f * phase);
        advance();
        return value;
    }

private:
    inline void advance() noexcept
    {
        phase += phaseInc;

        if (phase >= 1.0f)
            phase -= 1.0f;
    }

    /*  Residuo PolyBLEP de 2 muestras, escalado para un salto de -2 (el de la
        sierra). Suaviza la discontinuidad, que es de donde sale casi todo el
        aliasing. */
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
