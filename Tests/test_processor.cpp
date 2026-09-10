#include <juce_core/juce_core.h>
#include "PluginProcessor.h"
#include "Params/ParameterIDs.h"
#include "Common/StateVersion.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace
{
using namespace stabr;

juce::MidiBuffer makeNoteOn (int note, float vel, int samplePos)
{
    juce::MidiBuffer mb;
    mb.addEvent (juce::MidiMessage::noteOn (1, note, vel), samplePos);
    return mb;
}

bool bufferIsFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const auto* d = b.getReadPointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (d[i]))
                return false;
    }

    return true;
}

class ProcessorTests : public juce::UnitTest
{
public:
    ProcessorTests() : juce::UnitTest ("StabrProcessor", "Plugin") {}

    void runTest() override
    {
        beginTest ("silencio: salida exactamente 0.0 en reposo");
        {
            // Requisito del brief. Si esto falla hay denormals o una cola que
            // no termina de decaer.
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 512);
            proc.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            bool clean = true;

            for (int block = 0; block < 200 && clean; ++block)
            {
                buffer.clear();
                proc.processBlock (buffer, midi);

                for (int ch = 0; ch < 2 && clean; ++ch)
                {
                    const auto* d = buffer.getReadPointer (ch);

                    for (int i = 0; i < 512; ++i)
                    {
                        if (d[i] != 0.0f)
                        {
                            expect (false, "muestra no nula (" + juce::String (d[i])
                                               + ") en bloque " + juce::String (block));
                            clean = false;
                            break;
                        }
                    }
                }
            }

            if (clean)
                expect (true);
        }

        beginTest ("silencio exacto despues de que se apaga una nota");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);

            juce::AudioBuffer<float> buffer (2, 256);

            auto midi = makeNoteOn (60, 1.0f, 0);
            buffer.clear();
            proc.processBlock (buffer, midi);

            // Con sustain 0 (el default) la voz se libera sola tras el decay.
            juce::MidiBuffer empty;
            for (int block = 0; block < 400; ++block)
            {
                buffer.clear();
                proc.processBlock (buffer, empty);
            }

            float peak = 0.0f;
            for (int block = 0; block < 50; ++block)
            {
                buffer.clear();
                proc.processBlock (buffer, empty);

                for (int ch = 0; ch < 2; ++ch)
                    peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, 256));
            }

            expect (peak == 0.0f, "cola residual de " + juce::String (peak));
        }

        beginTest ("suena algo cuando llega una nota");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 512);
            proc.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            auto midi = makeNoteOn (60, 1.0f, 0);

            buffer.clear();
            proc.processBlock (buffer, midi);

            const auto peak = buffer.getMagnitude (0, 0, 512);
            expect (peak > 0.01f, "la nota no produjo senal (peak " + juce::String (peak) + ")");
            expect (peak <= 1.0f, "clipping con una sola nota (peak " + juce::String (peak) + ")");
        }

        beginTest ("60 s con parametros y MIDI aleatorios: sin NaN/inf");
        {
            constexpr double sr = 48000.0;
            constexpr int maxBlock = 2048;

            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, sr, maxBlock);
            proc.prepareToPlay (sr, maxBlock);

            juce::AudioBuffer<float> buffer (2, maxBlock);
            juce::Random rng (0x57AB);

            auto& params = proc.getParameters();
            const int totalSamples = (int) (60.0 * sr);
            int rendered = 0;
            int notesOn = 0;
            bool ok = true;

            while (rendered < totalSamples && ok)
            {
                // Tamano de bloque variable, cubriendo los extremos del brief.
                const int n = rng.nextInt ({ 32, maxBlock + 1 });

                if (rng.nextFloat() < 0.25f)
                    for (auto* p : params)
                        if (rng.nextFloat() < 0.35f)
                            p->setValueNotifyingHost (rng.nextFloat());

                juce::MidiBuffer midi;

                if (rng.nextFloat() < 0.4f)
                {
                    const int pos = rng.nextInt (n);
                    const int note = rng.nextInt ({ 24, 96 });

                    if (rng.nextBool() || notesOn == 0)
                    {
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, rng.nextFloat()), pos);
                        ++notesOn;
                    }
                    else
                    {
                        midi.addEvent (juce::MidiMessage::noteOff (1, note), pos);
                        --notesOn;
                    }
                }

                if (rng.nextFloat() < 0.05f)
                    midi.addEvent (juce::MidiMessage::pitchWheel (1, rng.nextInt (16384)), 0);

                buffer.setSize (2, n, false, false, true);
                buffer.clear();
                proc.processBlock (buffer, midi);

                if (! bufferIsFinite (buffer))
                {
                    expect (false, "NaN/inf tras " + juce::String (rendered / sr, 2) + " s");
                    ok = false;
                    break;
                }

                rendered += n;
            }

            if (ok)
            {
                expect (true);
                logMessage ("  60 s procesados en bloques variables, todo finito");
            }
        }

        beginTest ("independencia del tamano de buffer (32 vs 512)");
        {
            const auto a = renderNote (48000.0, 32,  48000);
            const auto b = renderNote (48000.0, 512, 48000);

            expectEquals ((int) a.size(), (int) b.size());

            double maxDiff = 0.0;
            for (size_t i = 0; i < a.size() && i < b.size(); ++i)
                maxDiff = juce::jmax (maxDiff, (double) std::abs (a[i] - b[i]));

            logMessage ("  diferencia maxima: " + juce::String (maxDiff, 12));
            expect (maxDiff < 1.0e-6,
                    "el tamano de buffer cambia la salida (diff " + juce::String (maxDiff) + ")");
        }

        beginTest ("independencia del sample rate (duracion del stab en ms)");
        {
            // El decay tiene que durar lo mismo en segundos a cualquier sr.
            std::vector<double> durations;

            for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                const auto out = renderNote (sr, 256, (int) sr);

                float peak = 0.0f;
                for (auto s : out)
                    peak = juce::jmax (peak, std::abs (s));

                const float threshold = peak * 0.001f;   // -60 dB del pico
                size_t lastLoud = 0;

                for (size_t i = 0; i < out.size(); ++i)
                    if (std::abs (out[i]) > threshold)
                        lastLoud = i;

                const auto ms = 1000.0 * (double) lastLoud / sr;
                durations.push_back (ms);
                logMessage ("  " + juce::String (sr, 0) + " Hz -> " + juce::String (ms, 2) + " ms");
            }

            const auto minMs = *std::min_element (durations.begin(), durations.end());
            const auto maxMs = *std::max_element (durations.begin(), durations.end());

            expect (minMs > 1.0, "el stab no sono");
            expect (maxMs / minMs < 1.10,
                    "la duracion varia mas de 10% entre sample rates ("
                        + juce::String (minMs, 2) + " a " + juce::String (maxMs, 2) + " ms)");
        }

        beginTest ("round-trip de estado con stateVersion");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 512);
            proc.prepareToPlay (48000.0, 512);

            auto* decay = proc.getAPVTS().getParameter (pid::ampDecay);
            auto* gain  = proc.getAPVTS().getParameter (pid::masterGain);
            expect (decay != nullptr && gain != nullptr, "faltan parametros");

            if (decay == nullptr || gain == nullptr)
                return;

            decay->setValueNotifyingHost (0.73f);
            gain->setValueNotifyingHost (0.42f);

            const auto savedDecay = decay->getValue();
            const auto savedGain  = gain->getValue();

            juce::MemoryBlock blob;
            proc.getStateInformation (blob);
            expect (blob.getSize() > 0, "el estado guardado esta vacio");

            decay->setValueNotifyingHost (0.1f);
            gain->setValueNotifyingHost (0.9f);

            proc.setStateInformation (blob.getData(), (int) blob.getSize());

            expectWithinAbsoluteError (decay->getValue(), savedDecay, 1.0e-5f);
            expectWithinAbsoluteError (gain->getValue(),  savedGain,  1.0e-5f);

            // El campo de version tiene que estar para poder migrar en el futuro.
            auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), (int) blob.getSize());
            expect (xml != nullptr, "el estado no es XML valido");

            if (xml != nullptr)
            {
                const auto attr = state::versionProperty.toString();
                expect (xml->hasAttribute (attr), "falta stateVersion en el estado guardado");
                expectEquals (xml->getIntAttribute (attr), state::kCurrentVersion);
            }
        }

        beginTest ("un estado sin version (v0) se migra sin romper");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 512);
            proc.prepareToPlay (48000.0, 512);

            auto tree = proc.getAPVTS().copyState();
            tree.removeProperty (state::versionProperty, nullptr);

            auto xml = tree.createXml();
            expect (xml != nullptr);

            if (xml == nullptr)
                return;

            juce::MemoryBlock blob;
            juce::AudioProcessor::copyXmlToBinary (*xml, blob);
            proc.setStateInformation (blob.getData(), (int) blob.getSize());

            expectEquals ((int) proc.getAPVTS().state.getProperty (state::versionProperty),
                          state::kCurrentVersion);
        }

        beginTest ("el robo de voces no genera clicks");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 64);
            proc.prepareToPlay (48000.0, 64);

            // Sustain al maximo para que las voces no se liberen solas, y
            // poliofonia 1 para forzar un robo en cada nota nueva.
            proc.getAPVTS().getParameter (pid::ampSustain)->setValueNotifyingHost (1.0f);
            proc.getAPVTS().getParameter (pid::polyphony)->setValueNotifyingHost (0.0f);

            juce::AudioBuffer<float> buffer (2, 64);
            float maxJump = 0.0f;
            float previous = 0.0f;
            bool started = false;

            for (int block = 0; block < 400; ++block)
            {
                juce::MidiBuffer midi;

                if (block % 8 == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 48 + (block % 24), 0.9f), 0);

                buffer.clear();
                proc.processBlock (buffer, midi);

                const auto* d = buffer.getReadPointer (0);

                for (int i = 0; i < 64; ++i)
                {
                    if (started)
                        maxJump = juce::jmax (maxJump, std::abs (d[i] - previous));

                    previous = d[i];
                    started = true;
                }
            }

            logMessage ("  salto maximo entre muestras: " + juce::String (maxJump, 4));

            // La sierra ya salta ~2x su amplitud en cada flanco. Un robo sin
            // fade produce saltos claramente mayores.
            expect (maxJump < 1.2f,
                    "salto de " + juce::String (maxJump) + ": compatible con un click de robo");
        }
    }

private:
    /*  Renderiza una nota y devuelve el canal izquierdo concatenado. */
    static std::vector<float> renderNote (double sr, int blockSize, int totalSamples)
    {
        StabrProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, blockSize);
        proc.prepareToPlay (sr, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> out;
        out.reserve ((size_t) totalSamples + (size_t) blockSize);

        int rendered = 0;
        bool sentNote = false;

        while (rendered < totalSamples)
        {
            const int n = juce::jmin (blockSize, totalSamples - rendered);

            juce::MidiBuffer midi;

            if (! sentNote)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
                sentNote = true;
            }

            buffer.setSize (2, n, false, false, true);
            buffer.clear();
            proc.processBlock (buffer, midi);

            const auto* d = buffer.getReadPointer (0);
            out.insert (out.end(), d, d + n);
            rendered += n;
        }

        return out;
    }
};

static ProcessorTests processorTests;
}
