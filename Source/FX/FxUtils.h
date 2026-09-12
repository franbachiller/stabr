#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

namespace stabr::fx
{
/*  Pasabajos/pasaaltos de un polo, forma TPT.

    Un polo alcanza para los filtros del lazo de realimentacion del delay y
    para el damping de la reverb: lo que hace falta es que las repeticiones se
    vayan oscureciendo, no una pendiente pronunciada. Y un polo es
    incondicionalmente estable dentro de un lazo, que es lo que importa aca.
*/
class OnePoleTpt
{
public:
    void setSampleRate (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        setCutoff (cutoff);
    }

    void reset() noexcept { state = 0.0f; }

    void setCutoff (float hz) noexcept
    {
        cutoff = std::clamp (hz, 5.0f, (float) (sampleRate * 0.49));

        const auto g = (float) std::tan (3.14159265358979323846
                                             * (double) cutoff / sampleRate);
        coeff = g / (1.0f + g);
    }

    inline float processLowpass (float x) noexcept
    {
        const float v = (x - state) * coeff;
        const float y = v + state;
        state = y + v;
        return y;
    }

    inline float processHighpass (float x) noexcept
    {
        return x - processLowpass (x);
    }

private:
    double sampleRate = 44100.0;
    float cutoff = 1000.0f;
    float coeff  = 0.1f;
    float state  = 0.0f;
};

//==============================================================================
/*  Seguidor de envolvente para el ducking.

    Ataque rapido y release lento: tiene que agarrar el transitorio del stab de
    inmediato (si no, el primer golpe pasa sin duckear) y soltar despacio (si
    no, el ducking bombea entre golpe y golpe).
*/
class EnvelopeFollower
{
public:
    void setSampleRate (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        setTimes (attackMs, releaseMs);
    }

    void reset() noexcept { envelope = 0.0f; }

    void setTimes (float attackMilliseconds, float releaseMilliseconds) noexcept
    {
        attackMs  = std::max (0.1f, attackMilliseconds);
        releaseMs = std::max (1.0f, releaseMilliseconds);

        attackCoeff  = coeffFor (attackMs);
        releaseCoeff = coeffFor (releaseMs);
    }

    inline float process (float x) noexcept
    {
        const float rectified = std::abs (x);
        const float coeff = rectified > envelope ? attackCoeff : releaseCoeff;

        envelope += (rectified - envelope) * coeff;
        return envelope;
    }

    float getValue() const noexcept { return envelope; }

private:
    float coeffFor (float ms) const noexcept
    {
        return 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * sampleRate));
    }

    double sampleRate = 44100.0;
    float attackMs = 1.0f, releaseMs = 120.0f;
    float attackCoeff = 0.1f, releaseCoeff = 0.001f;
    float envelope = 0.0f;
};

//==============================================================================
/*  LFO de wow & flutter.

    Dos componentes sumadas, porque una sola no suena a cinta:
      WOW     lento y periodico (~0.5 Hz), el bamboleo del motor
      FLUTTER rapido e irregular (~7 Hz con ruido), el roce de la cinta

    Una senoidal pura suena a chorus, no a maquina vieja.
*/
class WowFlutter
{
public:
    /*  La semilla es EXPLICITA y no puede faltar.

        juce::Random por defecto se siembra con el reloj, asi que cada instancia
        daria una secuencia distinta y el plugin sonaria diferente en cada
        carga. Peor: rompe cualquier test de reproducibilidad. Lo detecto el
        test de independencia del tamano de buffer, que empezo a fallar sin que
        nada del render hubiera cambiado.

        Cada canal recibe una semilla DISTINTA pero fija: si compartieran una,
        el bamboleo seria identico en los dos y sonaria mono. */
    void prepare (double sr, int seed) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        wowInc     = (float) (kWowHz / sampleRate);
        flutterInc = (float) (kFlutterHz / sampleRate);
        noiseCoeff = 1.0f - std::exp (-1.0f / (float) (0.02 * sampleRate));

        randomSeed = seed;
        reset();
    }

    void reset() noexcept
    {
        wowPhase = 0.0f;
        flutterPhase = 0.0f;
        smoothedNoise = 0.0f;
        random.setSeed (randomSeed);
    }

    /*  depth en 0..1; devuelve un desplazamiento en muestras. */
    inline float process (float depth, float maxSamples) noexcept
    {
        wowPhase += wowInc;
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;

        flutterPhase += flutterInc;
        if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;

        const float wow = std::sin (wowPhase * juce::MathConstants<float>::twoPi);
        const float flutter = std::sin (flutterPhase * juce::MathConstants<float>::twoPi);

        // Ruido suavizado: la irregularidad que distingue una cinta de un LFO.
        const float target = random.nextFloat() * 2.0f - 1.0f;
        smoothedNoise += (target - smoothedNoise) * noiseCoeff;

        const float combined = 0.6f * wow + 0.25f * flutter + 0.15f * smoothedNoise;

        return combined * depth * maxSamples;
    }

private:
    static constexpr double kWowHz = 0.47;
    static constexpr double kFlutterHz = 6.9;

    double sampleRate = 44100.0;
    float wowInc = 0.0f, flutterInc = 0.0f;
    float wowPhase = 0.0f, flutterPhase = 0.0f;
    float smoothedNoise = 0.0f, noiseCoeff = 0.01f;
    juce::Random random;
    int randomSeed = 1;
};

//==============================================================================
/*  Saturador suave acotado, para lazos de realimentacion.

    En el lazo del delay NO es opcional: sin el, un feedback cerca de 1 (o el
    modo freeze) crece sin limite. Con el, el lazo se autolimita igual que una
    cinta que se satura, que es justamente el sonido que se busca.
*/
inline float loopSaturate (float x) noexcept
{
    constexpr float limit = 1.4f;
    const float s = x / limit;
    return limit * s / std::sqrt (1.0f + s * s);
}
}
