#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>

#include "Oscillators/PolyBlepOsc.h"
#include "Env/AdsrExp.h"

namespace stabr
{
/*  Una voz = una nota.

    El campo chordId ya existe en Fase 1 aunque todavia no haya generador de
    acordes: en Fase 2 el robo de voces tiene que robar ACORDES ENTEROS. Robarle
    la septima a un min7 para meter la fundamental del acorde siguiente mutila
    la armonia y suena a bug. Ver DECISIONS.md D-001.
*/
class Voice
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void noteOn (int midiNote, float velocity, int chordIdIn, std::uint64_t stamp) noexcept;
    void noteOff() noexcept;

    /*  Robo: fade lineal corto a cero y despues Idle. Nunca se corta de golpe. */
    void startSteal() noexcept;

    void setEnvelopeParameters (float attackMs, float decayMs, float sustain, float releaseMs) noexcept;
    void setLevel (float linearGain) noexcept;
    void setPitchBendSemitones (float semis) noexcept;

    /*  Suma en los buffers; nunca escribe encima. */
    void renderAdd (float* left, float* right, int numSamples) noexcept;

    bool isActive() const noexcept       { return active; }
    bool isReleasing() const noexcept    { return active && (stealing || ampEnv.isReleasing()); }
    bool isStealing() const noexcept     { return stealing; }
    int  getNote() const noexcept        { return note; }
    int  getChordId() const noexcept     { return chordId; }
    std::uint64_t getStamp() const noexcept { return startStamp; }

private:
    void updateFrequency() noexcept;

    PolyBlepOsc osc;
    AdsrExp ampEnv;
    juce::SmoothedValue<float> level;

    double sampleRate = 44100.0;

    int  note      = -1;
    int  chordId   = -1;
    float velocityGain = 1.0f;
    float bendSemitones = 0.0f;
    std::uint64_t startStamp = 0;

    bool active   = false;
    bool stealing = false;
    float stealGain  = 1.0f;
    float stealDelta = 0.0f;

    // 5 ms es suficiente para matar el click y lo bastante corto para que el
    // robo no se escuche como un fade.
    static constexpr float kStealFadeMs = 5.0f;
};
}
