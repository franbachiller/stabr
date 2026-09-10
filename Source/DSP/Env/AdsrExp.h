#pragma once

#include <cmath>
#include <algorithm>

namespace stabr
{
/*  ADSR exponencial de un polo por etapa.

    El ADSR de JUCE es lineal, y un decay lineal no suena a stab: el golpe
    percusivo vive en la curva. Este usa el esquema clasico de "un polo con
    target overshoot": cada etapa apunta a un valor mas alla del destino real,
    de forma que la curva es exponencial pero llega al destino en tiempo finito.

    IMPORTANTE para el test de silencio: el release apunta a -targetRatioDR
    (negativo), asi que cruza el cero en tiempo finito y la voz termina en
    Idle con salida exactamente 0.0f. Una exponencial que apunta a cero nunca
    llega y te deja denormals colgando para siempre.

    Todo es realtime-safe: sin asignaciones, sin ramas caras, sin locks.
*/
class AdsrExp
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        recalculate();
    }

    /*  Tiempos en milisegundos, sustain en 0..1. Se recalculan los coeficientes
        una vez por bloque (no por muestra) desde el procesador. */
    void setParameters (float attackMs, float decayMs, float sustainLevel, float releaseMs) noexcept
    {
        attackTimeMs  = std::max (0.0f, attackMs);
        decayTimeMs   = std::max (0.0f, decayMs);
        sustain       = std::clamp (sustainLevel, 0.0f, 1.0f);
        releaseTimeMs = std::max (0.0f, releaseMs);
        recalculate();
    }

    void noteOn() noexcept
    {
        stage = Stage::Attack;
        // No se resetea 'output': si la voz estaba sonando, el retrigger arranca
        // desde el nivel actual y no produce un click.
    }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }

    /*  Corta de una: solo para reset duro (prepareToPlay, allNotesOff panic).
        Nunca durante la reproduccion, hace click. */
    void reset() noexcept
    {
        stage  = Stage::Idle;
        output = 0.0f;
    }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release; }
    float getLevel() const noexcept { return output; }
    Stage getStage() const noexcept { return stage; }

    inline float getNextSample() noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                return 0.0f;

            case Stage::Attack:
                output = attackBase + output * attackCoef;
                if (output >= 1.0f)
                {
                    output = 1.0f;
                    stage = (sustain >= 1.0f) ? Stage::Sustain : Stage::Decay;
                }
                break;

            case Stage::Decay:
                output = decayBase + output * decayCoef;
                if (output <= sustain)
                {
                    output = sustain;
                    stage  = Stage::Sustain;
                    // Sustain en 0 significa que la voz ya termino: un stab con
                    // sustain 0 no debe quedar ocupando una voz del pool.
                    if (sustain <= 0.0f)
                    {
                        output = 0.0f;
                        stage  = Stage::Idle;
                    }
                }
                break;

            case Stage::Sustain:
                output = sustain;
                break;

            case Stage::Release:
                output = releaseBase + output * releaseCoef;
                if (output <= 0.0f)
                {
                    output = 0.0f;
                    stage  = Stage::Idle;
                }
                break;
        }

        return output;
    }

private:
    static float calcCoef (float rateSamples, float targetRatio) noexcept
    {
        if (rateSamples <= 0.0f)
            return 0.0f;   // instantaneo

        return std::exp (-std::log ((1.0f + targetRatio) / targetRatio) / rateSamples);
    }

    void recalculate() noexcept
    {
        const auto msToSamples = [this] (float ms)
        {
            return (float) (ms * 0.001 * sampleRate);
        };

        const auto aSamples = msToSamples (attackTimeMs);
        const auto dSamples = msToSamples (decayTimeMs);
        const auto rSamples = msToSamples (releaseTimeMs);

        attackCoef = calcCoef (aSamples, targetRatioA);
        attackBase = (1.0f + targetRatioA) * (1.0f - attackCoef);

        decayCoef  = calcCoef (dSamples, targetRatioDR);
        decayBase  = (sustain - targetRatioDR) * (1.0f - decayCoef);

        releaseCoef = calcCoef (rSamples, targetRatioDR);
        releaseBase = -targetRatioDR * (1.0f - releaseCoef);
    }

    // targetRatioA alto = ataque casi lineal y rapido (bueno para transitorios).
    // targetRatioDR bajo = decay/release marcadamente exponencial (el "pluck").
    static constexpr float targetRatioA  = 0.3f;
    static constexpr float targetRatioDR = 0.0001f;

    double sampleRate = 44100.0;

    float attackTimeMs  = 0.5f;
    float decayTimeMs   = 220.0f;
    float sustain       = 0.0f;
    float releaseTimeMs = 120.0f;

    float attackCoef = 0.0f,  attackBase = 0.0f;
    float decayCoef  = 0.0f,  decayBase  = 0.0f;
    float releaseCoef = 0.0f, releaseBase = 0.0f;

    float output = 0.0f;
    Stage stage  = Stage::Idle;
};
}
