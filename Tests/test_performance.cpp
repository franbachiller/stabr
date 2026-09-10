#include <juce_core/juce_core.h>
#include "PluginProcessor.h"
#include "Params/ParameterIDs.h"
#include <vector>

namespace
{
using namespace stabr;

/*  Medicion de CPU.

    El brief pide reportar el costo con 8 voces x 7 unisonos, buffer 128 y
    48 kHz. El unisono llega en Fase 3, asi que esto establece la LINEA DE BASE
    con 8 voces y un oscilador por voz. El numero a vigilar es el "realtime
    factor": cuantos segundos de audio se generan por segundo de CPU.

    Deliberadamente NO falla salvo que el costo sea absurdo. Es un instrumento
    de medicion para comparar entre fases, no un test de correctitud: el
    hardware varia y un umbral estricto daria falsos negativos en otra maquina.
*/
class PerformanceTests : public juce::UnitTest
{
public:
    PerformanceTests() : juce::UnitTest ("Performance", "Plugin") {}

    void runTest() override
    {
        beginTest ("costo de CPU por cantidad de voces (48 kHz, buffer 128)");
        {
            logMessage ("  voces | rt factor | CPU de un core | us por bloque");
            logMessage ("  ------+-----------+----------------+--------------");

            double baselineCpu = 0.0;

            for (int voices : { 1, 4, 8, 16 })
            {
                const auto r = measure (48000.0, 128, voices, 5.0);

                logMessage ("  " + juce::String (voices).paddedLeft (' ', 5)
                            + " | " + juce::String (r.realtimeFactor, 1).paddedLeft (' ', 9)
                            + " | " + (juce::String (r.cpuPercent, 2) + " %").paddedLeft (' ', 14)
                            + " | " + juce::String (r.microsPerBlock, 1).paddedLeft (' ', 12));

                if (voices == 8)
                    baselineCpu = r.cpuPercent;
            }

            logMessage ("");
            logMessage ("  Referencia Fase 1: 8 voces = " + juce::String (baselineCpu, 2)
                        + " % de un core. Fase 3 suma osc2 + sub + unisono +");
            logMessage ("  filtro SVF + drive, asi que este numero va a subir mucho.");

            // Umbral flojo a proposito: solo atrapa una regresion catastrofica.
            expect (baselineCpu < 25.0,
                    "8 voces cuestan " + juce::String (baselineCpu, 2)
                        + " % de un core en Fase 1, que es demasiado para lo poco que hace");
        }

        beginTest ("costo con sample rates altos (8 voces, buffer 128)");
        {
            for (double sr : { 48000.0, 96000.0, 192000.0 })
            {
                const auto r = measure (sr, 128, 8, 3.0);
                logMessage ("  " + juce::String (sr, 0) + " Hz -> "
                            + juce::String (r.cpuPercent, 2) + " % de un core, rt factor "
                            + juce::String (r.realtimeFactor, 1));
            }

            expect (true);
        }

        beginTest ("el reposo no cuesta casi nada (silence gate)");
        {
            // Sin voces activas el SilenceGate corta el procesamiento. Si esto
            // no fuera cierto, un proyecto con 20 instancias de STABR en
            // silencio comeria CPU sin motivo.
            const auto idle   = measure (48000.0, 128, 0, 3.0);
            const auto active = measure (48000.0, 128, 8, 3.0);

            logMessage ("  reposo: " + juce::String (idle.cpuPercent, 3) + " %");
            logMessage ("  8 voces: " + juce::String (active.cpuPercent, 3) + " %");

            expect (idle.cpuPercent < active.cpuPercent,
                    "el reposo no cuesta menos que 8 voces sonando");
        }
    }

private:
    struct Result
    {
        double realtimeFactor = 0.0;   // segundos de audio por segundo de CPU
        double cpuPercent     = 0.0;   // % de un core para correr en tiempo real
        double microsPerBlock = 0.0;
    };

    static Result measure (double sr, int blockSize, int numVoices, double seconds)
    {
        StabrProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, blockSize);
        proc.prepareToPlay (sr, blockSize);

        // Sustain al maximo: las voces tienen que seguir sonando durante toda
        // la medicion, si no se apagan y medimos silencio.
        proc.getAPVTS().getParameter (pid::ampSustain)->setValueNotifyingHost (1.0f);

        // Y la poliofonia tiene que permitir las voces que vamos a medir. Sin
        // esto, pedir 16 voces con el default de 8 hace que el robo apague la
        // mitad y la medicion miente: da lo mismo que 8.
        if (numVoices > 0)
        {
            const auto norm = (float) (numVoices - 1) / (float) (VoiceManager::kMaxPolyphony - 1);
            proc.getAPVTS().getParameter (pid::polyphony)
                ->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        }

        juce::AudioBuffer<float> buffer (2, blockSize);

        // Todas las notas al principio, en un acorde ancho.
        juce::MidiBuffer firstMidi;
        for (int i = 0; i < numVoices; ++i)
            firstMidi.addEvent (juce::MidiMessage::noteOn (1, 36 + i * 4, 0.9f), 0);

        const int totalBlocks = (int) (seconds * sr / blockSize);

        // Calentamiento: se descarta para no medir el primer toque de cache.
        {
            auto midi = firstMidi;
            buffer.clear();
            proc.processBlock (buffer, midi);

            juce::MidiBuffer empty;
            for (int i = 0; i < 50; ++i)
            {
                buffer.clear();
                proc.processBlock (buffer, empty);
            }
        }

        juce::MidiBuffer empty;
        const auto start = juce::Time::getHighResolutionTicks();

        for (int i = 0; i < totalBlocks; ++i)
        {
            buffer.clear();
            proc.processBlock (buffer, empty);
        }

        const auto elapsed = juce::Time::highResolutionTicksToSeconds (
            juce::Time::getHighResolutionTicks() - start);

        const double audioSeconds = (double) (totalBlocks * blockSize) / sr;

        Result r;
        r.realtimeFactor = elapsed > 0.0 ? audioSeconds / elapsed : 0.0;
        r.cpuPercent     = elapsed > 0.0 ? 100.0 * elapsed / audioSeconds : 0.0;
        r.microsPerBlock = totalBlocks > 0 ? 1.0e6 * elapsed / totalBlocks : 0.0;

        return r;
    }
};

static PerformanceTests performanceTests;
}
