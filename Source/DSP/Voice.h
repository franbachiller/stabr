#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>

#include "Oscillators/Oscillator.h"
#include "Filters/SvfTpt.h"
#include "Drive/Drive.h"
#include "Env/AdsrExp.h"
#include "VoiceParams.h"

namespace stabr
{
/*  Una voz = una nota del acorde.

    Cadena: N copias de unisono (osc1 + osc2) + sub -> drive -> filtro -> amp.

    El campo chordId existe desde la Fase 1: el robo de voces roba ACORDES
    ENTEROS, no notas sueltas. Ver DECISIONS.md D-001.
*/
class Voice
{
public:
    static constexpr int kMaxUnison = 7;

    /*  Cada cuantas muestras se recalculan los coeficientes del filtro.

        El cutoff se modula por la envolvente de filtro, pero recalcular tan()
        por muestra y por voz seria el mayor costo del sintetizador. A 48 kHz,
        16 muestras son 0.33 ms: mucho mas rapido que cualquier movimiento
        audible de envolvente. Entre puntos de control, g se INTERPOLA
        linealmente por muestra, asi que no hay escalones ni zipper noise en
        barridos rapidos. */
    static constexpr int kControlRateSamples = 16;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void noteOn (int midiNote, float velocity, int chordIdIn, std::uint64_t stamp,
                 const VoiceParams& params, int unisonOverride) noexcept;
    void noteOff() noexcept;

    /*  Robo: fade lineal corto a cero y despues Idle. Nunca se corta de golpe. */
    void startSteal() noexcept;

    void setParams (const VoiceParams& params) noexcept;

    /*  Suma en los buffers; nunca escribe encima. */
    void renderAdd (float* left, float* right, int numSamples) noexcept;

    bool isActive() const noexcept       { return active; }
    bool isReleasing() const noexcept    { return active && (stealing || ampEnv.isReleasing()); }
    bool isStealing() const noexcept     { return stealing; }
    int  getNote() const noexcept        { return note; }
    int  getChordId() const noexcept     { return chordId; }
    std::uint64_t getStamp() const noexcept { return startStamp; }

    /*  Osciladores que esta voz tiene corriendo, para el presupuesto global.
        Ver DECISIONS.md D-014. */
    int getOscillatorCount() const noexcept
    {
        return active ? (unisonCount * 2 + (subActive ? 1 : 0)) : 0;
    }

private:
    void updatePitch() noexcept;
    void updateUnisonLayout() noexcept;
    float computeCutoff (float filterEnvValue) const noexcept;

    struct UnisonCopy
    {
        Oscillator osc1, osc2;
        float detuneRatio = 1.0f;   // multiplicador de frecuencia por el detune
        float panL = 0.7071f, panR = 0.7071f;
    };

    std::array<UnisonCopy, (size_t) kMaxUnison> unison;
    Oscillator subOsc;

    AdsrExp ampEnv, filterEnv;
    StereoLadder filter;
    Drive driveL, driveR;

    juce::SmoothedValue<float> level;
    juce::Random random;

    VoiceParams params;

    double sampleRate = 44100.0;

    int   note        = -1;
    int   chordId     = -1;
    float velocityGain = 1.0f;
    float velocity01   = 1.0f;
    std::uint64_t startStamp = 0;

    int   unisonCount = 1;
    float unisonNormalise = 1.0f;
    bool  subActive = false;

    bool  active   = false;
    bool  stealing = false;
    float stealGain  = 1.0f;
    float stealDelta = 0.0f;

    // Coeficientes del filtro, interpolados entre puntos de control.
    SvfCoeffs coeffs1, coeffs2;
    SvfCoeffs step1, step2;
    int samplesUntilControlUpdate = 0;

    // 5 ms es suficiente para matar el click y lo bastante corto para que el
    // robo no se escuche como un fade.
    static constexpr float kStealFadeMs = 5.0f;
};
}
