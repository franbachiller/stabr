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
    reset();
}

void VoiceManager::reset() noexcept
{
    for (auto& v : voices)
        v.reset();

    for (auto& s : slots)
        s = {};

    scratch.clear();
    stampCounter = 0;
    nextChordId  = 0;
}

void VoiceManager::setMaxPolyphony (int n) noexcept
{
    maxPolyphony = juce::jlimit (1, kMaxPolyphony, n);
}

void VoiceManager::setParams (const VoiceParams& p) noexcept
{
    params = p;

    for (auto& v : voices)
        if (v.isActive())
            v.setParams (p);
}

int VoiceManager::getOscillatorCount() const noexcept
{
    int n = 0;

    for (const auto& v : voices)
        n += v.getOscillatorCount();

    return n;
}

//==============================================================================
bool VoiceManager::hasVoicesFor (int chordId) const noexcept
{
    for (const auto& v : voices)
        if (v.isActive() && v.getChordId() == chordId)
            return true;

    return false;
}

void VoiceManager::reapFinishedSlots() noexcept
{
    // Un slot soltado cuyas voces ya terminaron vuelve al pool de slots.
    for (auto& s : slots)
        if (s.chordId >= 0 && ! s.sounding && ! hasVoicesFor (s.chordId))
            s = {};
}

Voice* VoiceManager::findFreeVoice() noexcept
{
    for (auto& v : voices)
        if (! v.isActive())
            return &v;

    return nullptr;
}

VoiceManager::ChordSlot* VoiceManager::findFreeSlot() noexcept
{
    for (auto& s : slots)
        if (s.chordId < 0)
            return &s;

    return nullptr;
}

VoiceManager::ChordSlot* VoiceManager::findOldestSoundingSlot() noexcept
{
    ChordSlot* best = nullptr;
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();

    for (auto& s : slots)
    {
        if (s.chordId >= 0 && s.sounding && s.stamp < oldest)
        {
            oldest = s.stamp;
            best   = &s;
        }
    }

    return best;
}

VoiceManager::ChordSlot* VoiceManager::findSlotForTrigger (int triggerNote) noexcept
{
    // El mas reciente primero: si la misma tecla se disparo dos veces, el
    // note-off tiene que apagar el acorde nuevo, no el que ya venia decayendo.
    ChordSlot* best = nullptr;
    std::uint64_t newest = 0;

    for (auto& s : slots)
    {
        if (s.chordId >= 0 && s.sounding && s.triggerNote == triggerNote && s.stamp >= newest)
        {
            newest = s.stamp;
            best   = &s;
        }
    }

    return best;
}

void VoiceManager::stealChord (ChordSlot& slot) noexcept
{
    // Fade corto en TODAS las voces del acorde: se va entero o no se va.
    for (auto& v : voices)
        if (v.isActive() && v.getChordId() == slot.chordId)
            v.startSteal();

    slot = {};
}

void VoiceManager::releaseChord (ChordSlot& slot) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && v.getChordId() == slot.chordId)
            v.noteOff();

    // Deja de contar para la polifonia, pero el slot sigue reservado hasta que
    // sus voces terminen de decaer (lo limpia reapFinishedSlots).
    slot.sounding = false;
}

//==============================================================================
void VoiceManager::noteOnChord (int triggerNote, const chord::ChordResult& notes, float velocity) noexcept
{
    if (notes.count <= 0)
        return;

    reapFinishedSlots();

    // Retrigger de la misma tecla: el acorde anterior se suelta y decae solo.
    // Superponer las colas es exactamente como suenan los stabs encadenados.
    if (auto* previous = findSlotForTrigger (triggerNote))
        releaseChord (*previous);

    // Limite de polifonia: se libera lugar robando el acorde sonando mas viejo.
    while (getSoundingChordCount() >= maxPolyphony)
    {
        auto* victim = findOldestSoundingSlot();

        if (victim == nullptr)
            break;

        stealChord (*victim);
    }

    auto* slot = findFreeSlot();

    if (slot == nullptr)
    {
        // Sin slots: se roba el mas viejo aunque este soltado.
        ChordSlot* oldest = nullptr;
        std::uint64_t oldestStamp = std::numeric_limits<std::uint64_t>::max();

        for (auto& s : slots)
        {
            if (s.chordId >= 0 && s.stamp < oldestStamp)
            {
                oldestStamp = s.stamp;
                oldest = &s;
            }
        }

        if (oldest == nullptr)
            return;

        stealChord (*oldest);
        slot = findFreeSlot();

        if (slot == nullptr)
            return;
    }

    const int chordId = nextChordId++;
    const auto stamp  = stampCounter++;

    slot->triggerNote = triggerNote;
    slot->chordId     = chordId;
    slot->stamp       = stamp;
    slot->sounding    = true;

    /*  PRESUPUESTO DE OSCILADORES (DECISIONS.md D-014).

        8 acordes x 7 notas x 7 unisonos x 2 osciladores son casi 800
        osciladores: el peor caso teorico no entra en ninguna CPU. Cuando el
        acorde nuevo no cabe se le BAJA EL UNISONO, no se le tiran notas.
        Perder densidad de detune es casi imperceptible; perder la novena del
        acorde cambia la armonia. */
    const int subPerVoice = params.subLevel > 0.0001f ? 1 : 0;
    const int inUse = getOscillatorCount();
    int unisonToUse = juce::jlimit (1, Voice::kMaxUnison, params.unisonCount);

    while (unisonToUse > 1
           && inUse + notes.count * (unisonToUse * 2 + subPerVoice) > kOscillatorBudget)
    {
        --unisonToUse;
    }

    lastUnisonUsed = unisonToUse;

    for (int i = 0; i < notes.count; ++i)
    {
        auto* v = findFreeVoice();

        if (v == nullptr)
        {
            // El pool se quedo sin voces. Antes que dejar el acorde a medias
            // (que suena peor que no tocarlo), se roba el acorde mas viejo que
            // no sea este y se reintenta una vez.
            ChordSlot* victim = nullptr;
            std::uint64_t oldestStamp = std::numeric_limits<std::uint64_t>::max();

            for (auto& s : slots)
            {
                if (s.chordId >= 0 && s.chordId != chordId && s.stamp < oldestStamp)
                {
                    oldestStamp = s.stamp;
                    victim = &s;
                }
            }

            if (victim != nullptr)
            {
                // startSteal deja las voces en fade, todavia activas: hay que
                // reciclarlas de una para poder usarlas ahora.
                const int victimId = victim->chordId;

                for (auto& vv : voices)
                    if (vv.isActive() && vv.getChordId() == victimId)
                        vv.reset();

                *victim = {};
                v = findFreeVoice();
            }

            if (v == nullptr)
                break;
        }

        v->noteOn (notes[i], velocity, chordId, stamp, params, unisonToUse);
    }
}

void VoiceManager::noteOnSingle (int midiNote, float velocity) noexcept
{
    chord::ChordResult single;
    single.notes[0] = midiNote;
    single.count = 1;

    noteOnChord (midiNote, single, velocity);
}

void VoiceManager::noteOff (int triggerNote) noexcept
{
    if (auto* slot = findSlotForTrigger (triggerNote))
        releaseChord (*slot);
}

void VoiceManager::allNotesOff (bool allowTailOff) noexcept
{
    for (auto& s : slots)
    {
        if (s.chordId < 0)
            continue;

        if (allowTailOff)
            releaseChord (s);
        else
            stealChord (s);
    }

    if (! allowTailOff)
        for (auto& v : voices)
            if (v.isActive())
                v.startSteal();
}

//==============================================================================
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
        // no sumar dos veces la senal en el unico canal real.
        jassert (numSamples <= scratch.getNumSamples());
        right = scratch.getWritePointer (0);
        juce::FloatVectorOperations::clear (right, juce::jmin (numSamples, scratch.getNumSamples()));
    }

    for (auto& v : voices)
        v.renderAdd (left, right, numSamples);
}

//==============================================================================
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

int VoiceManager::getSoundingChordCount() const noexcept
{
    int n = 0;

    for (const auto& s : slots)
        if (s.chordId >= 0 && s.sounding)
            ++n;

    return n;
}

chord::ChordResult VoiceManager::getSoundingNotes() const noexcept
{
    chord::ChordResult r;

    // Solo el acorde sonando mas reciente: mostrar todas las colas superpuestas
    // convertiria al visualizador en una sopa ilegible.
    const ChordSlot* newest = nullptr;
    std::uint64_t newestStamp = 0;

    for (const auto& s : slots)
        if (s.chordId >= 0 && s.sounding && s.stamp >= newestStamp)
        {
            newestStamp = s.stamp;
            newest = &s;
        }

    if (newest == nullptr)
        return r;

    for (const auto& v : voices)
    {
        if (! v.isActive() || v.getChordId() != newest->chordId)
            continue;

        if (r.count >= chord::kMaxChordNotes)
            break;

        r.notes[(size_t) r.count++] = v.getNote();
    }

    // Orden ascendente: el visualizador espera las voces de grave a agudo.
    for (int i = 1; i < r.count; ++i)
    {
        const int key = r.notes[(size_t) i];
        int j = i - 1;

        while (j >= 0 && r.notes[(size_t) j] > key)
        {
            r.notes[(size_t) (j + 1)] = r.notes[(size_t) j];
            --j;
        }

        r.notes[(size_t) (j + 1)] = key;
    }

    return r;
}
}
