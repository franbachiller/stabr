#include "Voice.h"
#include <cmath>

namespace stabr
{
void Voice::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    osc.setSampleRate (sampleRate);
    ampEnv.setSampleRate (sampleRate);
    level.reset (sampleRate, 0.005);
    reset();
}

void Voice::reset() noexcept
{
    ampEnv.reset();
    osc.reset (0.0f);
    level.setCurrentAndTargetValue (0.0f);
    active     = false;
    stealing   = false;
    stealGain  = 1.0f;
    stealDelta = 0.0f;
    note       = -1;
    chordId    = -1;
    bendSemitones = 0.0f;
}

void Voice::noteOn (int midiNote, float velocity, int chordIdIn, std::uint64_t stamp) noexcept
{
    note         = midiNote;
    chordId      = chordIdIn;
    startStamp   = stamp;
    velocityGain = juce::jlimit (0.0f, 1.0f, velocity);
    active       = true;
    stealing     = false;
    stealGain    = 1.0f;
    stealDelta   = 0.0f;

    // Fase fija: el transitorio de un stab tiene que ser identico en cada
    // disparo. La fase aleatoria se expone como opcion en Fase 3.
    osc.reset (0.0f);
    updateFrequency();
    ampEnv.noteOn();
}

void Voice::noteOff() noexcept
{
    ampEnv.noteOff();
}

void Voice::startSteal() noexcept
{
    if (! active || stealing)
        return;

    stealing   = true;
    stealGain  = 1.0f;
    const auto fadeSamples = juce::jmax (1.0f, (float) (kStealFadeMs * 0.001 * sampleRate));
    stealDelta = 1.0f / fadeSamples;
}

void Voice::setEnvelopeParameters (float attackMs, float decayMs, float sustain, float releaseMs) noexcept
{
    ampEnv.setParameters (attackMs, decayMs, sustain, releaseMs);
}

void Voice::setLevel (float linearGain) noexcept
{
    level.setTargetValue (linearGain * velocityGain);
}

void Voice::setPitchBendSemitones (float semis) noexcept
{
    bendSemitones = semis;
    if (active)
        updateFrequency();
}

void Voice::updateFrequency() noexcept
{
    const auto midi = (float) note + bendSemitones;
    osc.setFrequency (440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f));
}

void Voice::renderAdd (float* left, float* right, int numSamples) noexcept
{
    if (! active)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float env = ampEnv.getNextSample();

        if (stealing)
        {
            stealGain -= stealDelta;
            if (stealGain <= 0.0f)
            {
                stealGain = 0.0f;
                active    = false;
                stealing  = false;
                ampEnv.reset();
                osc.reset (0.0f);
                return;   // el resto del bloque ya no suma nada
            }
        }
        else if (! ampEnv.isActive())
        {
            // La envolvente termino en cero exacto: la voz se libera.
            active = false;
            osc.reset (0.0f);
            return;
        }

        const float sample = osc.processSaw() * env * level.getNextValue() * stealGain;

        left[i]  += sample;
        right[i] += sample;
    }
}
}
