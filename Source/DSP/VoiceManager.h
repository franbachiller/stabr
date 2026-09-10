#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>

#include "Voice.h"

namespace stabr
{
/*  Pool fijo de voces. Todo se reserva en prepare(); noteOn/noteOff/render son
    realtime-safe.

    Motor propio en vez de juce::Synthesiser: ver DECISIONS.md D-001.
*/
class VoiceManager
{
public:
    static constexpr int kMaxPolyphony = 16;
    // Headroom sobre la poliofonia maxima: las voces robadas siguen ocupando
    // lugar durante su fade de 5 ms. Sin headroom, un robo obliga a otro robo.
    static constexpr int kPoolSize = kMaxPolyphony * 2;

    void prepare (double sampleRate, int maximumBlockSize);
    void reset() noexcept;

    void setMaxPolyphony (int n) noexcept;
    void setEnvelopeParameters (float attackMs, float decayMs, float sustain, float releaseMs) noexcept;
    void setLevel (float linearGain) noexcept;
    void setPitchBendSemitones (float semis) noexcept;

    void noteOn (int midiNote, float velocity, int chordId = -1) noexcept;
    void noteOff (int midiNote) noexcept;
    void allNotesOff (bool allowTailOff) noexcept;

    /*  Suma la salida de todas las voces en el buffer, en el rango dado. */
    void renderAdd (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

    int getActiveVoiceCount() const noexcept;
    bool hasActiveVoices() const noexcept;

private:
    Voice* findFreeVoice() noexcept;
    Voice* findVoiceToSteal() noexcept;
    int countSoundingVoices() const noexcept;

    std::array<Voice, (size_t) kPoolSize> voices;
    juce::AudioBuffer<float> scratch;   // canal derecho cuando la salida es mono

    double sampleRate   = 44100.0;
    int maxPolyphony    = 8;
    std::uint64_t stampCounter = 0;
};
}
