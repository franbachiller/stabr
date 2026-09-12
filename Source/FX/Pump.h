#pragma once

#include "TempoSync.h"
#include <cmath>
#include <algorithm>

namespace stabr::fx
{
struct PumpParams
{
    bool enabled = false;
    Division division = Division::n1_4;   // el bombo de cuatro por cuatro
    float depth = 0.5f;                   // 0..1
    float shape = 0.5f;                   // 0..1, de curva suave a golpe seco
    float attackMs = 8.0f;                // cuanto tarda en cerrar
};

//==============================================================================
/*  Sidechain interno sincronizado al tempo.

    POR QUE EXISTE: el stab tiene que respirar con el bombo. Rutear un sidechain
    de verdad implica un canal aparte, un compresor, y saber configurarlo — o
    sea, exactamente lo que alguien que quiere un stab en 30 segundos no va a
    hacer. Esto lo da gratis y sincronizado.

    SE ALINEA AL TRANSPORTE, no a un LFO libre: la fase sale de la posicion en
    pulsos que reporta el host. Asi el pump cae SIEMPRE sobre el tiempo fuerte,
    aunque el usuario arranque a reproducir desde el compas 37. Un LFO libre se
    desfasaria y habria que resincronizarlo a mano.
*/
class Pump
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        freePhase = 0.0;
        currentGain = 1.0f;
    }

    void setParams (const PumpParams& p) noexcept
    {
        params = p;
        beatsPerCycle = divisionInBeats (p.division);
    }

    /*  Posicion del transporte al comienzo del bloque. Si el host no la
        informa, se cae a un contador libre para que el efecto siga andando. */
    void setTransport (double ppqPosition, double bpm, bool isPlaying, bool positionValid) noexcept
    {
        hasPosition = positionValid && isPlaying;
        blockPpq = ppqPosition;
        beatsPerSample = bpm > 0.0 ? (bpm / 60.0) / sampleRate : 0.0;

        if (hasPosition)
            freePhase = ppqPosition;
    }

    /*  Devuelve la ganancia de este sample: 1 = sin duckear. */
    inline float processSample() noexcept
    {
        if (! params.enabled || params.depth <= 0.0001f)
            return 1.0f;

        // Fase dentro del ciclo, en 0..1.
        const double cycles = freePhase / beatsPerCycle;
        const float phase = (float) (cycles - std::floor (cycles));

        freePhase += beatsPerSample;

        /*  Fraccion del ciclo que ocupa el ataque.
            sampleRate * beatsPerSample = pulsos por segundo; dividido por los
            pulsos que dura un ciclo, da ciclos por segundo. */
        const float cyclesPerSecond = (float) (sampleRate * beatsPerSample / beatsPerCycle);
        const float attackPhase = std::clamp (params.attackMs * 0.001f * cyclesPerSecond,
                                              0.0f, 0.5f);

        /*  La curva tiene DOS tramos, y el primero no es opcional:

            CIERRE (0 -> attackPhase): baja de 1 a 0. Sin esta rampa el ducking
            arrancaria de golpe y sonaria a click, no a compresor.

            APERTURA (attackPhase -> 1): vuelve a 1 con una potencia. shape en 0
            da una recuperacion casi lineal (compresor lento); en 1 da una muy
            exponencial, que es el "pumping" de cuatro por cuatro. */
        float envelope;

        if (attackPhase > 0.0f && phase < attackPhase)
        {
            envelope = 1.0f - phase / attackPhase;
        }
        else
        {
            const float span = std::max (1.0e-6f, 1.0f - attackPhase);
            const float t = std::clamp ((phase - attackPhase) / span, 0.0f, 1.0f);
            const float exponent = 0.4f + 4.0f * std::clamp (params.shape, 0.0f, 1.0f);
            envelope = std::pow (t, exponent);
        }

        currentGain = 1.0f - params.depth * (1.0f - envelope);
        return currentGain;
    }

    float getCurrentGain() const noexcept { return currentGain; }

private:
    PumpParams params;

    double sampleRate = 44100.0;
    double beatsPerCycle = 1.0;
    double beatsPerSample = 0.0;
    double freePhase = 0.0;
    double blockPpq = 0.0;
    bool hasPosition = false;

    float currentGain = 1.0f;
};
}
