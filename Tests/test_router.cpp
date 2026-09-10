#include <juce_core/juce_core.h>
#include "Chord/NoteRouter.h"
#include "PluginProcessor.h"
#include "Params/ParameterIDs.h"
#include <vector>

namespace
{
using namespace stabr;
using Type = NoteRouter::Event::Type;

juce::MidiBuffer buildMidi (std::initializer_list<std::pair<int, int>> noteAndOffset)
{
    juce::MidiBuffer mb;

    for (const auto& [note, offset] : noteAndOffset)
        mb.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), offset);

    return mb;
}

int countOfType (const NoteRouter::EventList& list, Type t)
{
    int n = 0;

    for (int i = 0; i < list.count; ++i)
        if (list[i].type == t)
            ++n;

    return n;
}

class NoteRouterTests : public juce::UnitTest
{
public:
    NoteRouterTests() : juce::UnitTest ("NoteRouter", "Chord") {}

    void runTest() override
    {
        beginTest ("modo Chord: cada nota genera un acorde, sin latencia");
        {
            NoteRouter r;
            r.prepare (48000.0);
            r.setNoteMode (chord::NoteMode::chord);

            expectEquals (r.getLatencySamples(), 0, "el modo Chord no debe tener latencia");

            NoteRouter::EventList out;
            auto midi = buildMidi ({ { 60, 0 }, { 64, 10 }, { 67, 20 } });
            r.process (midi, 512, out);

            expectEquals (out.count, 3);
            expectEquals (countOfType (out, Type::chordOn), 3);

            // El timing exacto se conserva.
            expectEquals (out[0].offset, 0);
            expectEquals (out[1].offset, 10);
            expectEquals (out[2].offset, 20);
        }

        beginTest ("modo Passthrough: las notas pasan tal cual, sin latencia");
        {
            NoteRouter r;
            r.prepare (48000.0);
            r.setNoteMode (chord::NoteMode::passthrough);

            expectEquals (r.getLatencySamples(), 0);

            NoteRouter::EventList out;
            auto midi = buildMidi ({ { 60, 0 }, { 64, 0 }, { 67, 0 } });
            r.process (midi, 512, out);

            expectEquals (out.count, 3);
            expectEquals (countOfType (out, Type::noteOn), 3);
            expectEquals (countOfType (out, Type::chordOn), 0);
        }

        beginTest ("modo Auto: una sola tecla en la ventana genera acorde");
        {
            NoteRouter r;
            r.prepare (48000.0);
            r.setNoteMode (chord::NoteMode::autoDetect);
            r.setAutoWindowMs (20.0f);

            const int window = r.getLatencySamples();
            expect (window > 0, "el modo Auto debe declarar latencia");
            expectWithinAbsoluteError (window, 960, 2);   // 20 ms a 48 kHz

            NoteRouter::EventList out;
            auto midi = buildMidi ({ { 60, 0 } });

            // Bloque corto: la ventana todavia no vencio, no debe sonar nada.
            r.process (midi, 128, out);
            expectEquals (out.count, 0, "disparo antes de que venciera la ventana");

            // Bloques vacios hasta pasar la ventana.
            juce::MidiBuffer empty;
            int fired = 0;

            for (int block = 0; block < 20 && fired == 0; ++block)
            {
                r.process (empty, 128, out);
                fired = out.count;
            }

            expectEquals (fired, 1, "no disparo al vencer la ventana");
            expect (out[0].type == Type::chordOn, "una sola tecla deberia generar acorde");
            expectEquals (out[0].note, 60);
        }

        beginTest ("modo Auto: dos teclas en la ventana pasan tal cual");
        {
            NoteRouter r;
            r.prepare (48000.0);
            r.setNoteMode (chord::NoteMode::autoDetect);
            r.setAutoWindowMs (20.0f);

            NoteRouter::EventList out;

            // Tres notas dentro de la misma ventana: es un acorde tocado a mano.
            auto midi = buildMidi ({ { 60, 0 }, { 64, 100 }, { 67, 200 } });
            r.process (midi, 512, out);
            expectEquals (out.count, 0, "no deberia disparar todavia");

            juce::MidiBuffer empty;
            int total = 0;

            for (int block = 0; block < 20 && total == 0; ++block)
            {
                r.process (empty, 128, out);
                total = out.count;
            }

            expectEquals (total, 3, "no disparo las tres notas juntas");
            expectEquals (countOfType (out, Type::noteOn), 3,
                          "con varias teclas deberia pasar tal cual, no generar acorde");
            expectEquals (countOfType (out, Type::chordOn), 0);
        }

        beginTest ("modo Auto: soltar la tecla antes de la ventana igual la hace sonar");
        {
            // Un staccato mas corto que la ventana no puede desaparecer.
            NoteRouter r;
            r.prepare (48000.0);
            r.setNoteMode (chord::NoteMode::autoDetect);
            r.setAutoWindowMs (20.0f);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 100);

            NoteRouter::EventList out;
            r.process (midi, 512, out);

            expect (out.count >= 2, "la nota corta se perdio: " + juce::String (out.count) + " eventos");
            expect (out[0].type == Type::chordOn, "deberia haber sonado como acorde");
            expect (out[out.count - 1].type == Type::noteOff, "falta el note off");
            expect (out[0].offset <= out[out.count - 1].offset, "el note off llego antes que la nota");
        }

        beginTest ("modo Auto: la ventana puede cruzar el limite de bloque");
        {
            // Con buffer chico la ventana abarca varios bloques. El disparo
            // tiene que caer en la muestra correcta, no al principio del bloque.
            NoteRouter r;
            r.prepare (48000.0);
            r.setNoteMode (chord::NoteMode::autoDetect);
            r.setAutoWindowMs (20.0f);   // 960 muestras

            NoteRouter::EventList out;
            auto midi = buildMidi ({ { 60, 0 } });

            r.process (midi, 256, out);          // 0..255
            expectEquals (out.count, 0);

            juce::MidiBuffer empty;
            r.process (empty, 256, out);         // 256..511
            expectEquals (out.count, 0);

            r.process (empty, 256, out);         // 512..767
            expectEquals (out.count, 0);

            r.process (empty, 256, out);         // 768..1023: vence en 960
            expectEquals (out.count, 1, "no disparo en el bloque correcto");
            expectEquals (out[0].offset, 960 - 768, "la muestra del disparo no es la correcta");
        }

        beginTest ("cambiar de modo no deja notas fantasma en la ventana");
        {
            NoteRouter r;
            r.prepare (48000.0);
            r.setNoteMode (chord::NoteMode::autoDetect);

            NoteRouter::EventList out;
            auto midi = buildMidi ({ { 60, 0 } });
            r.process (midi, 128, out);
            expectEquals (out.count, 0);

            r.setNoteMode (chord::NoteMode::chord);

            juce::MidiBuffer empty;

            for (int i = 0; i < 30; ++i)
            {
                r.process (empty, 128, out);
                expectEquals (out.count, 0, "quedo una nota fantasma de la ventana anterior");
            }
        }

        beginTest ("pitch bend y all-notes-off se conservan");
        {
            NoteRouter r;
            r.prepare (48000.0);
            r.setNoteMode (chord::NoteMode::chord);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::pitchWheel (1, 4096), 5);
            midi.addEvent (juce::MidiMessage::allNotesOff (1), 50);

            NoteRouter::EventList out;
            r.process (midi, 512, out);

            expectEquals (countOfType (out, Type::pitchBend), 1);
            expectEquals (countOfType (out, Type::allNotesOff), 1);
        }
    }
};

//==============================================================================
class ChordVoiceStealingTests : public juce::UnitTest
{
public:
    ChordVoiceStealingTests() : juce::UnitTest ("Chord voice stealing", "Chord") {}

    void runTest() override
    {
        beginTest ("una tecla dispara varias voces");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);

            // Sustain arriba para que las voces no se apaguen solas.
            proc.getAPVTS().getParameter (pid::ampSustain)->setValueNotifyingHost (1.0f);

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);

            buffer.clear();
            proc.processBlock (buffer, midi);

            const auto sounding = proc.getSoundingNotes();
            expect (sounding.count >= 3,
                    "una tecla sola deberia generar un acorde, sonaron "
                        + juce::String (sounding.count) + " notas");
        }

        beginTest ("el robo se lleva ACORDES ENTEROS, nunca notas sueltas");
        {
            /*  Este es D-001. Si el robo dejara acordes mutilados, un min7 se
                convertiria en triada al robarle la septima, y eso no suena a
                "se acabo la polifonia": suena a bug. */
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);

            auto& apvts = proc.getAPVTS();
            apvts.getParameter (pid::ampSustain)->setValueNotifyingHost (1.0f);
            apvts.getParameter (pid::polyphony)->setValueNotifyingHost (0.0f);   // 1 acorde
            apvts.getParameter (pid::chordDensity)->setValueNotifyingHost (1.0f); // 7 voces

            juce::AudioBuffer<float> buffer (2, 256);

            // Muchos acordes encadenados con polifonia 1: cada uno roba al anterior.
            for (int i = 0; i < 12; ++i)
            {
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, 48 + i * 2, 0.9f), 0);

                buffer.clear();
                proc.processBlock (buffer, midi);

                const auto sounding = proc.getSoundingNotes();

                // El acorde que suena siempre esta completo: nunca queda a medias.
                expect (sounding.count >= 5,
                        "acorde " + juce::String (i) + " quedo mutilado: "
                            + juce::String (sounding.count) + " voces");
            }
        }

        beginTest ("passthrough: las notas suenan tal cual");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);

            auto& apvts = proc.getAPVTS();
            apvts.getParameter (pid::ampSustain)->setValueNotifyingHost (1.0f);

            auto* noteModeParam = dynamic_cast<juce::AudioParameterChoice*> (
                apvts.getParameter (pid::noteMode));
            expect (noteModeParam != nullptr);

            if (noteModeParam == nullptr)
                return;

            *noteModeParam = (int) chord::NoteMode::passthrough;

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);

            buffer.clear();
            proc.processBlock (buffer, midi);

            const auto sounding = proc.getSoundingNotes();
            expectEquals (sounding.count, 1, "en passthrough una tecla debe dar una sola nota");
            expectEquals (sounding[0], 60);
        }

        beginTest ("soltar la tecla apaga el acorde entero");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);

            auto& apvts = proc.getAPVTS();
            apvts.getParameter (pid::ampSustain)->setValueNotifyingHost (1.0f);
            apvts.getParameter (pid::ampRelease)->setValueNotifyingHost (0.0f);   // release corto

            juce::AudioBuffer<float> buffer (2, 256);

            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
            buffer.clear();
            proc.processBlock (buffer, on);

            expect (proc.getSoundingNotes().count > 1);

            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            buffer.clear();
            proc.processBlock (buffer, off);

            expectEquals (proc.getSoundingNotes().count, 0,
                          "el acorde sigue contando como sonando despues del note off");

            juce::MidiBuffer empty;
            float peak = 0.0f;

            for (int i = 0; i < 400; ++i)
            {
                buffer.clear();
                proc.processBlock (buffer, empty);
            }

            for (int i = 0; i < 20; ++i)
            {
                buffer.clear();
                proc.processBlock (buffer, empty);
                peak = juce::jmax (peak, buffer.getMagnitude (0, 0, 256));
            }

            expectEquals (peak, 0.0f, "quedo audio despues de soltar la tecla");
        }
    }
};

static NoteRouterTests noteRouterTests;
static ChordVoiceStealingTests chordVoiceStealingTests;
}
