#include "VoiceManager.h"
#include <limits>

namespace stabr
{
void VoiceManager::prepare (double sr, int maximumBlockSize)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;

    for (auto& v : voices)
        v.prepare (sampleRate);

    // Reservado aca, jamas en processBlock.
    scratch.setSize (1, juce::jmax (1, maximumBlockSize), false, true, true);
    stampCounter = 0;
}

void VoiceManager::reset() noexcept
{
    for (auto& v : voices)
        v.reset();

    scratch.clear();
    stampCounter = 0;
}

void VoiceManager::setMaxPolyphony (int n) noexcept
{
    maxPolyphony = juce::jlimit (1, kMaxPolyphony, n);
}

void VoiceManager::setEnvelopeParameters (float a, float d, float s, float r) noexcept
{
    for (auto& v : voices)
        v.setEnvelopeParameters (a, d, s, r);
}

void VoiceManager::setLevel (float linearGain) noexcept
{
    for (auto& v : voices)
        v.setLevel (linearGain);
}

void VoiceManager::setPitchBendSemitones (float semis) noexcept
{
    for (auto& v : voices)
        v.setPitchBendSemitones (semis);
}

int VoiceManager::countSoundingVoices() const noexcept
{
    int n = 0;
    for (const auto& v : voices)
        if (v.isActive() && ! v.isStealing())
            ++n;

    return n;
}

Voice* VoiceManager::findFreeVoice() noexcept
{
    for (auto& v : voices)
        if (! v.isActive())
            return &v;

    return nullptr;
}

Voice* VoiceManager::findVoiceToSteal() noexcept
{
    // Prioridad 1: la voz en release mas vieja. Ya la estan soltando, robarla
    // es lo menos audible.
    Voice* best = nullptr;
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();

    for (auto& v : voices)
    {
        if (v.isActive() && ! v.isStealing() && v.isReleasing() && v.getStamp() < oldest)
        {
            oldest = v.getStamp();
            best   = &v;
        }
    }

    if (best != nullptr)
        return best;

    // Prioridad 2: la voz sonando mas vieja.
    oldest = std::numeric_limits<std::uint64_t>::max();
    for (auto& v : voices)
    {
        if (v.isActive() && ! v.isStealing() && v.getStamp() < oldest)
        {
            oldest = v.getStamp();
            best   = &v;
        }
    }

    return best;
}

void VoiceManager::noteOn (int midiNote, float velocity, int chordId) noexcept
{
    if (countSoundingVoices() >= maxPolyphony)
        if (auto* victim = findVoiceToSteal())
            victim->startSteal();

    auto* v = findFreeVoice();

    if (v == nullptr)
    {
        // El pool completo esta en fade. Caso patologico: se roba la mas vieja
        // igual, aceptando el click, antes que perder la nota.
        v = findVoiceToSteal();
        if (v == nullptr)
            return;

        v->reset();
    }

    v->noteOn (midiNote, velocity, chordId, stampCounter++);
}

void VoiceManager::noteOff (int midiNote) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && ! v.isStealing() && v.getNote() == midiNote)
            v.noteOff();
}

void VoiceManager::allNotesOff (bool allowTailOff) noexcept
{
    for (auto& v : voices)
    {
        if (! v.isActive())
            continue;

        if (allowTailOff)
            v.noteOff();
        else
            v.startSteal();
    }
}

void VoiceManager::renderAdd (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const int numChannels = buffer.getNumChannels();
    if (numChannels < 1)
        return;

    float* left  = buffer.getWritePointer (0, startSample);
    float* right = nullptr;

    if (numChannels >= 2)
    {
        right = buffer.getWritePointer (1, startSample);
    }
    else
    {
        // Salida mono: el canal derecho va a un scratch que se descarta, para
        // no sumar dos veces la señal en el unico canal real.
        jassert (numSamples <= scratch.getNumSamples());
        right = scratch.getWritePointer (0);
        juce::FloatVectorOperations::clear (right, juce::jmin (numSamples, scratch.getNumSamples()));
    }

    for (auto& v : voices)
        v.renderAdd (left, right, numSamples);
}

int VoiceManager::getActiveVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& v : voices)
        if (v.isActive())
            ++n;

    return n;
}

bool VoiceManager::hasActiveVoices() const noexcept
{
    for (const auto& v : voices)
        if (v.isActive())
            return true;

    return false;
}
}
