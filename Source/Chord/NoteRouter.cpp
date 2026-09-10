#include "NoteRouter.h"

namespace stabr
{
void NoteRouter::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    setAutoWindowMs (autoWindowMs);
    reset();
}

void NoteRouter::reset() noexcept
{
    pendingCount = 0;
    pendingCountdown = 0;
}

void NoteRouter::setNoteMode (chord::NoteMode m) noexcept
{
    if (m == mode)
        return;

    mode = m;
    // Cambiar de modo con notas en la ventana dejaria fantasmas colgados.
    pendingCount = 0;
    pendingCountdown = 0;
}

void NoteRouter::setAutoWindowMs (float ms) noexcept
{
    autoWindowMs = juce::jlimit (0.0f, 100.0f, ms);
    autoWindowSamples = (int) (autoWindowMs * 0.001 * sampleRate);
}

int NoteRouter::getLatencySamples() const noexcept
{
    return mode == chord::NoteMode::autoDetect ? autoWindowSamples : 0;
}

bool NoteRouter::isPending (int note) const noexcept
{
    for (int i = 0; i < pendingCount; ++i)
        if (pending[(size_t) i].note == note)
            return true;

    return false;
}

//==============================================================================
void NoteRouter::firePending (int offset, EventList& out) noexcept
{
    if (pendingCount <= 0)
        return;

    /*  ACA ESTA LA DECISION DEL MODO AUTO:
        una sola tecla en la ventana -> el generador arma el acorde.
        dos o mas -> el usuario esta tocando su propio acorde, se respeta. */
    const auto type = (pendingCount == 1) ? Event::Type::chordOn
                                          : Event::Type::noteOn;

    for (int i = 0; i < pendingCount; ++i)
    {
        Event e;
        e.offset   = offset;
        e.type     = type;
        e.note     = pending[(size_t) i].note;
        e.velocity = pending[(size_t) i].velocity;
        out.add (e);
    }

    pendingCount = 0;
    pendingCountdown = 0;
}

void NoteRouter::flushPendingImmediately (int offset, EventList& out) noexcept
{
    firePending (offset, out);
}

//==============================================================================
void NoteRouter::process (const juce::MidiBuffer& midi, int numSamples, EventList& out) noexcept
{
    out.clear();

    const bool autoMode = (mode == chord::NoteMode::autoDetect);

    for (const auto meta : midi)
    {
        const int offset = juce::jlimit (0, numSamples, meta.samplePosition);

        // Si el grupo pendiente vence ANTES de este evento, dispara primero:
        // los dos flujos (MIDI y vencimiento) se mezclan en orden temporal.
        if (autoMode && pendingCount > 0 && pendingCountdown <= offset)
            firePending (juce::jmax (0, pendingCountdown), out);

        const auto m = meta.getMessage();

        if (m.isNoteOn())
        {
            if (autoMode)
            {
                if (pendingCount == 0)
                    pendingCountdown = offset + autoWindowSamples;

                if (pendingCount < kMaxPendingNotes)
                {
                    pending[(size_t) pendingCount].note     = m.getNoteNumber();
                    pending[(size_t) pendingCount].velocity = m.getFloatVelocity();
                    ++pendingCount;
                }
                else
                {
                    // Ventana desbordada: ya hay muchas notas, o sea que es un
                    // acorde tocado a mano. Dispara ya en modo passthrough.
                    firePending (offset, out);

                    Event e;
                    e.offset   = offset;
                    e.type     = Event::Type::noteOn;
                    e.note     = m.getNoteNumber();
                    e.velocity = m.getFloatVelocity();
                    out.add (e);
                }
            }
            else
            {
                Event e;
                e.offset   = offset;
                e.type     = (mode == chord::NoteMode::chord) ? Event::Type::chordOn
                                                              : Event::Type::noteOn;
                e.note     = m.getNoteNumber();
                e.velocity = m.getFloatVelocity();
                out.add (e);
            }
        }
        else if (m.isNoteOff())
        {
            // Una nota que se suelta antes de que venza su ventana igual tiene
            // que sonar: se dispara el grupo y despues se la apaga.
            if (autoMode && isPending (m.getNoteNumber()))
                flushPendingImmediately (offset, out);

            Event e;
            e.offset = offset;
            e.type   = Event::Type::noteOff;
            e.note   = m.getNoteNumber();
            out.add (e);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            pendingCount = 0;
            pendingCountdown = 0;

            Event e;
            e.offset = offset;
            e.type   = m.isAllSoundOff() ? Event::Type::allSoundOff
                                         : Event::Type::allNotesOff;
            out.add (e);
        }
        else if (m.isPitchWheel())
        {
            Event e;
            e.offset     = offset;
            e.type       = Event::Type::pitchBend;
            e.pitchWheel = m.getPitchWheelValue();
            out.add (e);
        }
    }

    // Vencimiento dentro de este bloque pero despues del ultimo evento MIDI.
    if (autoMode && pendingCount > 0)
    {
        if (pendingCountdown <= numSamples)
            firePending (juce::jlimit (0, numSamples, pendingCountdown), out);
        else
            pendingCountdown -= numSamples;   // sigue esperando en el proximo bloque
    }
}
}
