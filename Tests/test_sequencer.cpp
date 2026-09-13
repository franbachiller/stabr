#include <juce_core/juce_core.h>
#include "Sequencer/StepSequencer.h"
#include "Sequencer/NoteLatch.h"
#include "PluginProcessor.h"
#include "Params/ParameterIDs.h"
#include "Params/ParamCache.h"
#include "Chord/ChordGenerator.h"
#include "Common/StateVersion.h"
#include <vector>
#include <functional>
#include <cstdint>

namespace
{
using namespace stabr;
using seq::StepSequencer;
using SeqType = StepSequencer::Event::Type;

struct AbsEvent
{
    std::int64_t sample = 0;
    bool on = false;
    float velocity = 0.0f;

    bool operator== (const AbsEvent& o) const noexcept
    {
        return sample == o.sample && on == o.on && velocity == o.velocity;
    }
};

seq::SeqParams customPattern()
{
    seq::SeqParams p;
    p.enabled = true;
    p.pattern = seq::PatternId::custom;

    for (auto& s : p.custom)
        s = {};

    return p;
}

/*  Corre el secuenciador con un transporte que avanza sin saltos y devuelve
    los eventos en muestras absolutas. El ppq de cada bloque se recalcula
    desde cero, como hace un host real, no se arrastra del bloque anterior. */
std::vector<AbsEvent> runSequencer (StepSequencer& s, double sr, double bpm, std::int64_t totalSamples,
                                    const std::function<int()>& nextBlock,
                                    double startPpq = 0.0, int latency = 0)
{
    std::vector<AbsEvent> result;
    StepSequencer::EventList out;

    seq::Transport t;
    t.bpm = bpm;
    t.isPlaying = true;
    t.hasPosition = true;

    std::int64_t pos = 0;

    while (pos < totalSamples)
    {
        const int n = (int) std::min<std::int64_t> (nextBlock(), totalSamples - pos);
        t.ppq = startPpq + (double) pos * bpm / 60.0 / sr;
        s.process (t, n, latency, out);

        for (int i = 0; i < out.count; ++i)
            result.push_back ({ pos + out[i].offset, out[i].type == SeqType::gateOn, out[i].velocity });

        pos += n;
    }

    return result;
}

std::vector<std::int64_t> onsetsOf (const std::vector<AbsEvent>& events)
{
    std::vector<std::int64_t> r;

    for (const auto& e : events)
        if (e.on)
            r.push_back (e.sample);

    return r;
}

std::function<int()> fixedBlock (int n)
{
    return [n] { return n; };
}

juce::String listToString (const std::vector<std::int64_t>& v, size_t maxItems = 8)
{
    juce::String s;

    for (size_t i = 0; i < std::min (v.size(), maxItems); ++i)
        s << juce::String (v[i]) << " ";

    return s;
}

//==============================================================================
class StepSequencerTests : public juce::UnitTest
{
public:
    StepSequencerTests() : juce::UnitTest ("StepSequencer", "Sequencer") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;
        constexpr double bpm = 120.0;
        constexpr std::int64_t sixteenth = 6000;   // 120 BPM a 48 kHz
        constexpr std::int64_t bar = 16 * sixteenth;

        beginTest ("off-beat de corcheas: cada disparo en la muestra EXACTA");
        {
            StepSequencer s;
            s.prepare (sr);

            seq::SeqParams p;
            p.enabled = true;
            p.pattern = seq::PatternId::offbeat8;
            s.setParams (p);

            const auto on = onsetsOf (runSequencer (s, sr, bpm, 2 * bar, fixedBlock (512)));

            std::vector<std::int64_t> expected;
            for (int i = 0; i < 8; ++i)
                expected.push_back ((2 + 4 * i) * sixteenth);

            expect (on == expected, "disparos: " + listToString (on));
        }

        beginTest ("el resultado NO depende del tamano de bloque");
        {
            /*  Patron con todo lo que puede romperse en un borde de bloque:
                swing, ratchets, probabilidad, gate largo y un tempo con
                decimales para que los pasos no caigan en muestras enteras. */
            auto p = customPattern();
            p.custom[0]  = { true, 1.0f, 1.0f, 3 };
            p.custom[3]  = { true, 0.7f, 0.5f, 1 };
            p.custom[5]  = { true, 0.6f, 1.0f, 2 };
            p.custom[10] = { true, 0.9f, 1.0f, 1 };
            p.custom[13] = { true, 0.8f, 0.35f, 3 };
            p.swing = 0.62f;
            p.gate  = 0.85f;
            p.length = 14;

            constexpr double sr2 = 44100.0, bpm2 = 128.37;
            const std::int64_t total = (std::int64_t) (8 * 4 * 60.0 / bpm2 * sr2);

            auto render = [&] (const std::function<int()>& blocks)
            {
                StepSequencer s;
                s.prepare (sr2);
                s.setParams (p);
                return runSequencer (s, sr2, bpm2, total, blocks);
            };

            const auto reference = render (fixedBlock (1));
            expect (reference.size() > 40, "el patron de prueba casi no disparo");

            for (int block : { 32, 128, 511, 2048 })
            {
                const auto other = render (fixedBlock (block));
                expect (other == reference,
                        "con bloque " + juce::String (block) + " da " + juce::String ((int) other.size())
                            + " eventos contra " + juce::String ((int) reference.size()));
            }

            juce::Random rng (1234);
            const auto irregular = render ([&rng] { return 1 + rng.nextInt (2048); });
            expect (irregular == reference, "con bloques irregulares el resultado cambia");
        }

        beginTest ("swing: 50 % es recto, 75 % atrasa los impares media semicorchea");
        {
            auto allOn = customPattern();
            for (auto& st : allOn.custom)
                st = { true, 0.8f, 1.0f, 1 };

            for (const auto& [swing, expectedOdd] : { std::pair { 0.5f,   6000 },
                                                      std::pair { 0.625f, 7500 },
                                                      std::pair { 0.75f,  9000 } })
            {
                StepSequencer s;
                s.prepare (sr);
                allOn.swing = swing;
                s.setParams (allOn);

                const auto on = onsetsOf (runSequencer (s, sr, bpm, bar, fixedBlock (256)));

                expect (on.size() >= 4);
                if (on.size() < 4)
                    continue;

                expectEquals ((int) on[0], 0, "el paso par se movio");
                expectEquals ((int) on[1], expectedOdd, "swing " + juce::String (swing));
                expectEquals ((int) on[2], 12000, "el paso par siguiente se movio");
                expectEquals ((int) on[3], 12000 + expectedOdd);
            }
        }

        beginTest ("ratchet x3: tres disparos repartidos en el paso, cada uno con su cierre");
        {
            auto p = customPattern();
            p.custom[0] = { true, 0.9f, 1.0f, 3 };
            p.gate = 0.5f;

            StepSequencer s;
            s.prepare (sr);
            s.setParams (p);

            const auto events = runSequencer (s, sr, bpm, sixteenth, fixedBlock (128));

            const std::vector<std::pair<std::int64_t, bool>> expected
            {
                { 0, true }, { 1000, false }, { 2000, true }, { 3000, false }, { 4000, true }, { 5000, false }
            };

            expectEquals ((int) events.size(), (int) expected.size());

            for (size_t i = 0; i < std::min (events.size(), expected.size()); ++i)
            {
                expectEquals ((int) events[i].sample, (int) expected[i].first, "evento " + juce::String ((int) i));
                expect (events[i].on == expected[i].second, "tipo del evento " + juce::String ((int) i));
            }
        }

        beginTest ("probabilidad: 0 nunca, 1 siempre, 0.5 cerca de la mitad y reproducible");
        {
            auto p = customPattern();
            const std::int64_t bars = 100;

            auto countWith = [&] (float prob)
            {
                for (auto& st : p.custom)
                    st = { true, 0.8f, prob, 1 };

                StepSequencer s;
                s.prepare (sr);
                s.setParams (p);
                return runSequencer (s, sr, bpm, bars * bar, fixedBlock (1024));
            };

            expectEquals ((int) onsetsOf (countWith (0.0f)).size(), 0);
            expectEquals ((int) onsetsOf (countWith (1.0f)).size(), (int) (bars * 16));

            const auto half = onsetsOf (countWith (0.5f));
            const int total = (int) (bars * 16);
            logMessage ("probabilidad 0.5: " + juce::String ((int) half.size()) + " de " + juce::String (total));

            expect ((int) half.size() > total * 45 / 100 && (int) half.size() < total * 55 / 100,
                    "0.5 dio " + juce::String ((int) half.size()) + " de " + juce::String (total));

            expect (onsetsOf (countWith (0.5f)) == half, "la probabilidad no es reproducible");

            // Tiene que variar entre vueltas del patron, no repetir el mismo compas.
            std::vector<int> perBar ((size_t) bars, 0);
            for (auto t : half)
                ++perBar[(size_t) (t / bar)];

            bool allEqual = true;
            for (auto c : perBar)
                allEqual = allEqual && c == perBar[0];

            expect (! allEqual, "todos los compases dieron lo mismo: la tirada no cambia por vuelta");
        }

        beginTest ("largo del patron: 3 pasos se repiten cada 3 semicorcheas (polimetria)");
        {
            auto p = customPattern();
            p.custom[0] = { true, 0.8f, 1.0f, 1 };
            p.custom[3] = { true, 0.8f, 1.0f, 1 };   // queda fuera del largo: no debe sonar nunca
            p.length = 3;

            StepSequencer s;
            s.prepare (sr);
            s.setParams (p);

            const auto on = onsetsOf (runSequencer (s, sr, bpm, bar, fixedBlock (300)));
            const std::vector<std::int64_t> expected { 0, 18000, 36000, 54000, 72000, 90000 };

            expect (on == expected, "disparos: " + listToString (on));
        }

        beginTest ("rate 1/8: el mismo patron a la mitad de velocidad");
        {
            seq::SeqParams p;
            p.enabled = true;
            p.pattern = seq::PatternId::offbeat8;
            p.rate = seq::StepRate::n1_8;

            StepSequencer s;
            s.prepare (sr);
            s.setParams (p);

            const auto on = onsetsOf (runSequencer (s, sr, bpm, 2 * bar, fixedBlock (512)));
            expect (on.size() >= 2);

            if (on.size() >= 2)
            {
                expectEquals ((int) on[0], 24000);
                expectEquals ((int) on[1], 72000);
            }
        }

        beginTest ("arrancar a mitad de compas: el primer disparo cae en el grid, no en el arranque");
        {
            seq::SeqParams p;
            p.enabled = true;
            p.pattern = seq::PatternId::offbeat8;

            StepSequencer s;
            s.prepare (sr);
            s.setParams (p);

            // Compas 10, beat 2.3: el proximo contratiempo es 37.5, a 0.2 negras.
            const auto on = onsetsOf (runSequencer (s, sr, bpm, bar, fixedBlock (512), 37.3));
            expect (! on.empty());

            if (! on.empty())
                expectEquals ((int) on[0], 4800, "el primer disparo no quedo alineado");
        }

        beginTest ("parar cierra la nota abierta y no emite nada mientras esta parado");
        {
            auto p = customPattern();
            for (auto& st : p.custom)
                st = { true, 0.8f, 1.0f, 1 };
            p.gate = 1.0f;   // la nota queda abierta siempre hasta el paso siguiente

            StepSequencer s;
            s.prepare (sr);
            s.setParams (p);

            runSequencer (s, sr, bpm, bar / 2 + 777, fixedBlock (256));
            expect (s.isGateOpen(), "con gate 100% deberia haber una nota abierta");

            seq::Transport stopped;
            stopped.bpm = bpm;
            stopped.ppq = 8.1;
            stopped.isPlaying = false;
            stopped.hasPosition = true;

            StepSequencer::EventList out;
            s.process (stopped, 256, 0, out);

            expectEquals (out.count, 1, "al parar tiene que salir exactamente un cierre");
            if (out.count == 1)
            {
                expect (out[0].type == SeqType::gateOff);
                expectEquals (out[0].offset, 0);
            }

            expect (! s.isGateOpen());
            expectEquals (s.getCurrentStep(), -1);

            for (int i = 0; i < 50; ++i)
            {
                s.process (stopped, 256, 0, out);
                expectEquals (out.count, 0, "emitio eventos con el transporte parado");
            }
        }

        beginTest ("un salto del transporte (loop) cierra la nota y dispara desde la posicion nueva");
        {
            auto p = customPattern();
            for (auto& st : p.custom)
                st = { true, 0.8f, 1.0f, 1 };
            p.gate = 1.0f;

            StepSequencer s;
            s.prepare (sr);
            s.setParams (p);

            // Loop de un compas que vuelve al principio.
            runSequencer (s, sr, bpm, bar - 256, fixedBlock (256));

            seq::Transport jump;
            jump.bpm = bpm;
            jump.ppq = 0.0;
            jump.isPlaying = true;
            jump.hasPosition = true;

            StepSequencer::EventList out;
            s.process (jump, 256, 0, out);

            expect (out.count >= 2, "el salto no genero cierre + disparo");
            if (out.count >= 2)
            {
                expect (out[0].type == SeqType::gateOff && out[0].offset == 0, "no cerro la nota al saltar");
                expect (out[1].type == SeqType::gateOn && out[1].offset == 0, "no disparo el paso 1 en la muestra 0");
            }
        }

        beginTest ("con latencia declarada, los disparos se atrasan exactamente esa cantidad");
        {
            seq::SeqParams p;
            p.enabled = true;
            p.pattern = seq::PatternId::offbeat8;

            StepSequencer s;
            s.prepare (sr);
            s.setParams (p);

            const auto on = onsetsOf (runSequencer (s, sr, bpm, bar, fixedBlock (512), 0.0, 960));
            expect (! on.empty());

            if (! on.empty())
                expectEquals ((int) on[0], 12000 + 960);
        }

        beginTest ("sin posicion del host (Standalone) corre libre desde el paso 1");
        {
            seq::SeqParams p;
            p.enabled = true;
            p.pattern = seq::PatternId::offbeat8;

            StepSequencer s;
            s.prepare (sr);
            s.setParams (p);

            seq::Transport none;   // hasPosition = false, isPlaying = false
            none.bpm = bpm;

            auto firstOnset = [&]
            {
                StepSequencer::EventList out;

                for (std::int64_t pos = 0; pos < bar; pos += 512)
                {
                    s.process (none, 512, 0, out);

                    for (int i = 0; i < out.count; ++i)
                        if (out[i].type == SeqType::gateOn)
                            return pos + out[i].offset;
                }

                return (std::int64_t) -1;
            };

            expectEquals ((int) firstOnset(), 12000, "sin host no arranco o no quedo alineado");

            // Apagar y volver a prender reinicia el reloj libre.
            p.enabled = false;
            s.setParams (p);
            StepSequencer::EventList out;
            s.process (none, 512, 0, out);

            p.enabled = true;
            s.setParams (p);
            expectEquals ((int) firstOnset(), 12000, "al reencender no volvio al paso 1");
        }

        beginTest ("fuzz: cada disparo tiene su cierre y los eventos salen ordenados");
        {
            /*  Saltos, stops, cambios de patron, de rate, de largo y de tempo en
                el medio, con bloques de 1 a 2048. Lo unico que se exige es lo
                que nunca puede fallar: nada de notas colgadas ni eventos fuera
                del bloque. */
            juce::Random rng (777);

            StepSequencer s;
            s.prepare (sr);

            auto p = customPattern();
            StepSequencer::EventList out;
            seq::Transport t;
            t.hasPosition = true;
            t.isPlaying = true;
            t.bpm = 124.0;

            bool open = false;
            int violations = 0;
            int ons = 0;

            for (int block = 0; block < 60000; ++block)
            {
                if (rng.nextInt (200) == 0)
                {
                    p.pattern = (seq::PatternId) rng.nextInt ((int) seq::PatternId::numPatterns);
                    p.rate    = (seq::StepRate) rng.nextInt ((int) seq::StepRate::numRates);
                    p.length  = 1 + rng.nextInt (16);
                    p.swing   = 0.5f + 0.25f * rng.nextFloat();
                    p.gate    = 0.05f + 0.95f * rng.nextFloat();
                    p.enabled = rng.nextInt (10) != 0;

                    for (auto& st : p.custom)
                        st = { rng.nextBool(), rng.nextFloat(), rng.nextFloat(), 1 + rng.nextInt (3) };

                    s.setParams (p);
                }

                if (rng.nextInt (300) == 0) t.isPlaying = ! t.isPlaying;
                if (rng.nextInt (400) == 0) t.ppq = rng.nextDouble() * 64.0 - 1.0;   // incluye pre-roll negativo
                if (rng.nextInt (500) == 0) t.bpm = 20.0 + rng.nextDouble() * 300.0;

                const int n = 1 + rng.nextInt (2048);
                s.process (t, n, 0, out);

                int lastOffset = 0;

                for (int i = 0; i < out.count; ++i)
                {
                    const auto& e = out[i];

                    if (e.offset < 0 || e.offset >= n || e.offset < lastOffset)
                        ++violations;

                    lastOffset = e.offset;

                    if (e.type == SeqType::gateOn)
                    {
                        if (open) ++violations;   // disparo sin cerrar el anterior
                        open = true;
                        ++ons;
                    }
                    else
                    {
                        if (! open) ++violations; // cierre de algo que no estaba abierto
                        open = false;
                    }
                }

                expect (open == s.isGateOpen(), "el estado interno no coincide con los eventos");

                if (t.isPlaying)
                    t.ppq += n * t.bpm / 60.0 / sr;
            }

            logMessage ("fuzz: " + juce::String (ons) + " disparos");
            expect (ons > 1000, "el fuzz casi no disparo, no prueba nada");
            expectEquals (violations, 0);

            t.isPlaying = false;
            s.process (t, 64, 0, out);
            expect (! s.isGateOpen(), "quedo una nota abierta despues de parar");
        }
    }
};

//==============================================================================
class NoteLatchTests : public juce::UnitTest
{
public:
    NoteLatchTests() : juce::UnitTest ("NoteLatch", "Sequencer") {}

    void runTest() override
    {
        beginTest ("soltar la tecla no borra el latch");
        {
            seq::NoteLatch l;
            l.noteOn (60, 0.9f);
            l.noteOff (60);

            expectEquals (l.size(), 1);
            expectEquals (l.getLastNote(), 60);
        }

        beginTest ("la primera tecla despues de soltar todo empieza un acorde nuevo");
        {
            seq::NoteLatch l;
            l.noteOn (60, 0.9f);
            l.noteOff (60);
            l.noteOn (62, 0.9f);

            expectEquals (l.size(), 1);
            expectEquals (l.getLatchedNotes()[0], 62);
        }

        beginTest ("teclas apretadas juntas se suman: acorde armado a mano");
        {
            seq::NoteLatch l;
            l.noteOn (67, 0.9f);
            l.noteOn (64, 0.9f);
            l.noteOn (71, 0.9f);
            l.noteOff (64);
            l.noteOff (67);
            l.noteOff (71);

            const auto notes = l.getLatchedNotes();
            expectEquals (notes.count, 3);
            expectEquals (notes[0], 64);   // de grave a agudo
            expectEquals (notes[1], 67);
            expectEquals (notes[2], 71);
        }

        beginTest ("all-notes-off olvida las teclas apretadas pero no el acorde");
        {
            seq::NoteLatch l;
            l.noteOn (60, 0.9f);
            l.noteOn (64, 0.9f);   // note-off perdidos: el host mando all-notes-off
            l.releaseAllHeld();

            expectEquals (l.size(), 2, "el all-notes-off borro el latch");

            l.noteOn (50, 0.9f);
            expectEquals (l.size(), 1, "una tecla fantasma hizo que la nueva se sumara");
        }
    }
};

//==============================================================================
struct FakePlayHead final : public juce::AudioPlayHead
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = true;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setIsPlaying (playing);
        return info;
    }
};

void setPlain (StabrProcessor& proc, const char* id, float plainValue)
{
    auto* param = proc.getAPVTS().getParameter (id);
    jassert (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
}

class SequencerIntegrationTests : public juce::UnitTest
{
public:
    SequencerIntegrationTests() : juce::UnitTest ("Sequencer en el plugin", "Sequencer") {}

    static constexpr double sr = 48000.0;
    static constexpr int blockSize = 256;

    void prepare (StabrProcessor& proc, FakePlayHead& ph)
    {
        proc.setPlayConfigDetails (0, 2, sr, blockSize);
        proc.prepareToPlay (sr, blockSize);
        proc.setPlayHead (&ph);

        setPlain (proc, pid::seqEnabled, 1.0f);

        // Sin efectos con cola: estos tests miden el TIMING de las voces.
        setPlain (proc, pid::delayMix, 0.0f);
        setPlain (proc, pid::reverbMix, 0.0f);
        setPlain (proc, pid::chorusMix, 0.0f);
    }

    /*  Procesa un bloque y avanza el transporte si esta andando. */
    static void processBlock (StabrProcessor& proc, FakePlayHead& ph,
                              juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        buffer.clear();
        proc.processBlock (buffer, midi);

        if (ph.playing)
            ph.ppq += blockSize * ph.bpm / 60.0 / sr;
    }

    static chord::ChordParams currentChordParams (StabrProcessor& proc)
    {
        ParamCache cache;
        cache.resolve (proc.getAPVTS());

        chord::ChordParams cp;
        cache.fillChordParams (cp);
        return cp;
    }

    /*  Toca hasta el bloque que contiene 'sample' y devuelve lo que suena ahi. */
    static chord::ChordResult soundingAt (StabrProcessor& proc, FakePlayHead& ph, std::int64_t sample)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer empty;

        for (std::int64_t pos = 0; pos <= sample; pos += blockSize)
            processBlock (proc, ph, buffer, empty);

        return proc.getSoundingNotes();
    }

    void expectSameChord (const chord::ChordResult& got, const chord::ChordResult& expected, const juce::String& what)
    {
        bool same = got.count == expected.count;

        for (int i = 0; same && i < got.count; ++i)
            same = got[i] == expected[i];

        juce::String g, e;
        for (int i = 0; i < got.count; ++i)      g << got[i] << " ";
        for (int i = 0; i < expected.count; ++i) e << expected[i] << " ";

        expect (same, what + ": suena [" + g + "] y deberia [" + e + "]");
    }

    void runTest() override
    {
        beginTest ("CRITERIO DE LA FASE: arranca el transporte y el stab toca solo, alineado");
        {
            StabrProcessor proc;
            FakePlayHead ph;
            prepare (proc, ph);

            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer empty;

            // Dos compases sin una sola nota MIDI.
            std::vector<float> out;
            chord::ChordResult firstChord;

            for (std::int64_t pos = 0; pos < 192000; pos += blockSize)
            {
                processBlock (proc, ph, buffer, empty);

                if (pos <= 12000 && 12000 < pos + blockSize)
                    firstChord = proc.getSoundingNotes();

                const auto* l = buffer.getReadPointer (0);
                out.insert (out.end(), l, l + blockSize);
            }

            // Arranques de cada stab: primera muestra audible despues de un silencio.
            std::vector<std::int64_t> onsets;
            int quiet = 100000;

            for (size_t i = 0; i < out.size(); ++i)
            {
                const float a = std::abs (out[i]);

                if (a > 1.0e-3f && quiet >= 2000)
                    onsets.push_back ((std::int64_t) i);

                quiet = a < 1.0e-4f ? quiet + 1 : 0;
            }

            logMessage ("arranques: " + listToString (onsets));
            expectEquals ((int) onsets.size(), 8, "en dos compases de contratiempos tienen que sonar 8 stabs");

            if (! onsets.empty())
            {
                // El retardo fijo del camino de audio (sobremuestreo del drive y
                // la saturacion) es aceptable; que VARIE entre stabs, no.
                const auto latency = onsets[0] - 12000;
                expect (latency >= 0 && latency < 64,
                        "el primer stab no cayo en su tiempo: muestra " + juce::String (onsets[0]));

                for (size_t i = 1; i < onsets.size(); ++i)
                {
                    const auto grid = (std::int64_t) (12000 + 24000 * (std::int64_t) i);
                    expect (std::abs ((onsets[i] - grid) - latency) <= 1,
                            "el stab " + juce::String ((int) i) + " se corrio: muestra " + juce::String (onsets[i]));
                }
            }

            // Sin nada latcheado suena el acorde de la tonica (La por defecto).
            const auto cp = currentChordParams (proc);
            expectSameChord (firstChord,
                             chord::generateChord (StabrProcessor::kFallbackRootBase + cp.keyRoot, cp),
                             "acorde de la tonica sin MIDI");
        }

        beginTest ("latch: tecla con el transporte parado, se suelta, y el patron dispara ESE acorde");
        {
            StabrProcessor proc;
            FakePlayHead ph;
            ph.playing = false;
            prepare (proc, ph);

            juce::AudioBuffer<float> buffer (2, blockSize);

            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 62, 0.9f), 10);
            processBlock (proc, ph, buffer, on);

            expect (proc.getSoundingNotes().count > 0,
                    "con el transporte parado la tecla tiene que sonar en vivo");

            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::noteOff (1, 62), 0);
            processBlock (proc, ph, buffer, off);

            juce::MidiBuffer empty;
            for (int i = 0; i < 100; ++i)
                processBlock (proc, ph, buffer, empty);

            ph.playing = true;
            ph.ppq = 0.0;

            const auto cp = currentChordParams (proc);
            expectSameChord (soundingAt (proc, ph, 12000), chord::generateChord (62, cp), "acorde latcheado");
        }

        beginTest ("passthrough: el acorde tocado a mano se secuencia tal cual");
        {
            StabrProcessor proc;
            FakePlayHead ph;
            ph.playing = false;
            prepare (proc, ph);
            setPlain (proc, pid::noteMode, (float) chord::NoteMode::passthrough);

            juce::AudioBuffer<float> buffer (2, blockSize);

            juce::MidiBuffer on;
            for (int n : { 64, 60, 67 })
                on.addEvent (juce::MidiMessage::noteOn (1, n, 0.9f), 0);
            processBlock (proc, ph, buffer, on);

            juce::MidiBuffer off;
            for (int n : { 60, 64, 67 })
                off.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            processBlock (proc, ph, buffer, off);

            ph.playing = true;
            ph.ppq = 0.0;

            chord::ChordResult expected;
            expected.notes[0] = 60;
            expected.notes[1] = 64;
            expected.notes[2] = 67;
            expected.count = 3;

            expectSameChord (soundingAt (proc, ph, 12000), expected, "acorde a mano");
        }

        beginTest ("con el patron andando, una tecla cambia el acorde pero no dispara fuera de tiempo");
        {
            StabrProcessor proc;
            FakePlayHead ph;
            prepare (proc, ph);

            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer empty;

            // Hasta despues del primer stab y de que se apague.
            for (std::int64_t pos = 0; pos < 30000; pos += blockSize)
                processBlock (proc, ph, buffer, empty);

            const int activeBefore = proc.getActiveVoiceCount();

            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 65, 0.9f), 0);
            processBlock (proc, ph, buffer, on);

            expectEquals (proc.getSoundingNotes().count, 0,
                          "la tecla disparo un acorde fuera del patron");
            expect (proc.getActiveVoiceCount() <= activeBefore, "la tecla agrego voces");

            // En el proximo contratiempo (36000) suena el acorde de la tecla.
            const auto cp = currentChordParams (proc);
            chord::ChordResult got;

            for (std::int64_t pos = 30000 + blockSize; pos <= 36000; pos += blockSize)
            {
                processBlock (proc, ph, buffer, empty);
                got = proc.getSoundingNotes();
            }

            expectSameChord (got, chord::generateChord (65, cp), "acorde despues de cambiar la tecla");
        }

        beginTest ("al arrancar se suelta lo que se tocaba en vivo, y al parar no queda nada colgado");
        {
            StabrProcessor proc;
            FakePlayHead ph;
            ph.playing = false;
            prepare (proc, ph);

            setPlain (proc, pid::ampSustain, 1.0f);
            setPlain (proc, pid::seqGate, 1.0f);

            juce::AudioBuffer<float> buffer (2, blockSize);

            // Tecla mantenida (sin note-off) con el transporte parado.
            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
            processBlock (proc, ph, buffer, on);
            expect (proc.getSoundingNotes().count > 0);

            ph.playing = true;
            ph.ppq = 0.0;

            // Con el patron de contratiempos el primer golpe es en la muestra
            // 12000: en el primer bloque no tiene que sonar NADA. Si suena algo,
            // es el acorde en vivo que no se solto.
            juce::MidiBuffer empty;
            processBlock (proc, ph, buffer, empty);
            expectEquals (proc.getSoundingNotes().count, 0,
                          "el acorde en vivo siguio sonando encima del patron");

            // Recien ahora los 16 pasos encendidos. El gate es POR PASO: con el
            // patron de contratiempos la nota solo queda abierta durante la
            // semicorchea de cada golpe. Con todos prendidos y gate 100% siempre
            // hay una nota abierta, que es lo que tiene que cerrar el stop.
            setPlain (proc, pid::seqPattern, (float) seq::PatternId::custom);
            for (auto* id : pid::seqStepOn)
                setPlain (proc, id, 1.0f);

            for (int i = 0; i < 400; ++i)
                processBlock (proc, ph, buffer, empty);

            expect (proc.getSoundingNotes().count > 0, "con sustain y gate 100% deberia estar sonando");

            ph.playing = false;
            processBlock (proc, ph, buffer, empty);

            expectEquals (proc.getSoundingNotes().count, 0, "al parar el transporte quedo un acorde colgado");
            expectEquals (proc.getSequencerStep(), -1);
        }

        beginTest ("un proyecto viejo sin parametros del secuenciador carga con sus defaults");
        {
            /*  D-040. Prende el secuenciador y carga un estado de la v0.4, que
                no tiene ningun seq_*. Tienen que volver a su default, no quedar
                como estaban: si no, un proyecto viejo empezaria a tocar solo.
                Hoy JUCE lo resuelve (valueTreeChildAdded); este test es la
                guardia por si una version futura deja de hacerlo. */
            StabrProcessor proc;
            FakePlayHead ph;
            prepare (proc, ph);

            auto oldState = proc.getAPVTS().copyState();

            for (int i = oldState.getNumChildren(); --i >= 0;)
                if (oldState.getChild (i).getProperty ("id").toString().startsWith ("seq_"))
                    oldState.removeChild (i, nullptr);

            oldState.setProperty (state::versionProperty, 1, nullptr);

            setPlain (proc, pid::seqEnabled, 1.0f);
            setPlain (proc, pid::seqSwing, 0.7f);
            setPlain (proc, pid::seqStepOn[0], 1.0f);

            auto xml = oldState.createXml();
            expect (xml != nullptr);

            if (xml == nullptr)
                return;

            juce::MemoryBlock blob;
            juce::AudioProcessor::copyXmlToBinary (*xml, blob);
            proc.setStateInformation (blob.getData(), (int) blob.getSize());

            auto plain = [&] (const char* id)
            {
                auto* param = proc.getAPVTS().getParameter (id);
                return param->convertFrom0to1 (param->getValue());
            };

            expectEquals (plain (pid::seqEnabled), 0.0f, "el secuenciador quedo prendido al cargar un proyecto viejo");
            expectWithinAbsoluteError (plain (pid::seqSwing), 0.5f, 1.0e-4f);
            expectEquals (plain (pid::seqStepOn[0]), 0.0f);

            // Y un parametro que SI estaba en el estado viejo se sigue cargando.
            expectWithinAbsoluteError (plain (pid::delayMix), 0.0f, 1.0e-4f);
        }
    }
};

static StepSequencerTests stepSequencerTests;
static NoteLatchTests noteLatchTests;
static SequencerIntegrationTests sequencerIntegrationTests;
}
