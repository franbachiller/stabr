#include <juce_core/juce_core.h>
#include "PluginProcessor.h"
#include "Params/ParameterIDs.h"
#include "Chord/ChordTypes.h"
#include <vector>

namespace
{
using namespace stabr;

/*  Medicion de CPU.

    El brief pide reportar el costo con 8 voces x 7 unisonos, buffer 128 y
    48 kHz. El unisono llega en Fase 3. Desde la Fase 2 la unidad de disparo es
    el ACORDE (cada tecla genera "density" voces), asi que se mide por acordes
    y se reporta cuantas voces sonaron de verdad. El numero a vigilar es el
    "realtime factor": cuantos segundos de audio se generan por segundo de CPU.

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
        beginTest ("costo de CPU por cantidad de acordes (48 kHz, buffer 128)");
        {
            // Desde la Fase 2 la unidad de disparo es el ACORDE, no la nota:
            // cada tecla genera 'density' voces. Medir "voces" ya no
            // corresponde con lo que el usuario configura.
            logMessage ("  acordes | voces | rt factor | CPU de un core | us por bloque");
            logMessage ("  --------+-------+-----------+----------------+--------------");

            double baselineCpu = 0.0;
            constexpr int density = 4;   // el default del plugin

            for (int chords = 1; chords <= VoiceManager::kMaxPolyphony; chords += (chords < 4 ? 1 : 4))
            {
                const auto r = measure (48000.0, 128, chords, density, 4.0);

                logMessage ("  " + juce::String (chords).paddedLeft (' ', 7)
                            + " | " + juce::String (r.voiceCount).paddedLeft (' ', 5)
                            + " | " + juce::String (r.realtimeFactor, 1).paddedLeft (' ', 9)
                            + " | " + (juce::String (r.cpuPercent, 2) + " %").paddedLeft (' ', 14)
                            + " | " + juce::String (r.microsPerBlock, 1).paddedLeft (' ', 12));

                if (chords == VoiceManager::kMaxPolyphony)
                    baselineCpu = r.cpuPercent;
            }

            logMessage ("");
            logMessage ("  Peor caso de Fase 2: " + juce::String (VoiceManager::kMaxPolyphony)
                        + " acordes x " + juce::String (density) + " voces = "
                        + juce::String (baselineCpu, 2) + " % de un core.");
            logMessage ("  La Fase 3 suma osc2 + sub + unisono + filtro SVF + drive: va a subir mucho.");

            // Umbral flojo a proposito: solo atrapa una regresion catastrofica.
            expect (baselineCpu < 25.0,
                    "el peor caso cuesta " + juce::String (baselineCpu, 2)
                        + " % de un core, demasiado para lo poco que hace todavia");
        }

        beginTest ("costo del unisono y del drive (peor caso real)");
        {
            /*  Aca es donde se ve si el presupuesto de osciladores (D-014) esta
                bien puesto: es la configuracion que un usuario pondria para un
                supersaw gordo, no un caso de laboratorio. */
            logMessage ("  config                          | osc | voces | CPU de un core");
            logMessage ("  --------------------------------+-----+-------+---------------");

            struct Case { const char* name; int unison; float drive; bool hq; };

            const Case cases[]
            {
                { "default (uni 1, drive off)     ", 1, 0.0f,  false },
                { "uni 3, drive off               ", 3, 0.0f,  false },
                { "uni 7, drive off               ", 7, 0.0f,  false },
                { "uni 7, drive a tope (x2)       ", 7, 1.0f,  false },
                { "uni 7, drive a tope (x4 HQ)    ", 7, 1.0f,  true  },
            };

            double worstRealtime = 0.0;
            double hqCost = 0.0;

            for (const auto& c : cases)
            {
                const auto r = measure (48000.0, 128, VoiceManager::kMaxPolyphony,
                                        stabr::chord::kMaxDensity, 3.0,
                                        c.unison, c.drive, c.hq);

                logMessage ("  " + juce::String (c.name)
                            + " | " + juce::String (r.oscCount).paddedLeft (' ', 3)
                            + " | " + juce::String (r.voiceCount).paddedLeft (' ', 5)
                            + " | " + (juce::String (r.cpuPercent, 2) + " %").paddedLeft (' ', 14));

                if (c.hq)
                    hqCost = r.cpuPercent;
                else
                    worstRealtime = juce::jmax (worstRealtime, r.cpuPercent);
            }

            logMessage ("");
            logMessage ("  Presupuesto de osciladores: " + juce::String (VoiceManager::kOscillatorBudget)
                        + " (es un techo BLANDO: nunca se tiran notas, asi que el minimo"
                        + " de 1 unisono por voz puede pasarlo)");

            /*  Solo se exige el peor caso de TIEMPO REAL. HQ Mode es x4 sobre
                cada voz y esta pensado para render offline, donde la CPU no
                importa: exigirle presupuesto de tiempo real seria pedirle algo
                que no promete. Se reporta igual para que el numero este a la
                vista. */
            expect (worstRealtime < 60.0,
                    "el peor caso de tiempo real cuesta " + juce::String (worstRealtime, 2)
                        + " % de un core");

            logMessage ("  HQ Mode (x4): " + juce::String (hqCost, 1)
                        + " % en el peor caso. Es para render offline, no para tocar en vivo.");
        }

        beginTest ("costo con sample rates altos (8 acordes x 4 voces, buffer 128)");
        {
            for (double sr : { 48000.0, 96000.0, 192000.0 })
            {
                const auto r = measure (sr, 128, VoiceManager::kMaxPolyphony, 4, 3.0);
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
            const auto idle   = measure (48000.0, 128, 0, 4, 3.0);
            const auto active = measure (48000.0, 128, VoiceManager::kMaxPolyphony, 4, 3.0);

            logMessage ("  reposo: " + juce::String (idle.cpuPercent, 3) + " %");
            logMessage ("  8 acordes: " + juce::String (active.cpuPercent, 3) + " % (" + juce::String (active.voiceCount) + " voces)");

            expect (idle.cpuPercent < active.cpuPercent,
                    "el reposo no cuesta menos que 8 acordes sonando");
        }
    }

private:
    struct Result
    {
        double realtimeFactor = 0.0;   // segundos de audio por segundo de CPU
        double cpuPercent     = 0.0;   // % de un core para correr en tiempo real
        double microsPerBlock = 0.0;
        int    voiceCount     = 0;     // voces realmente sonando, medidas
        int    oscCount       = 0;     // osciladores corriendo, medidos
    };

    static void setChoiceOrInt (juce::RangedAudioParameter* p, int value, int minV, int maxV)
    {
        if (p == nullptr || maxV <= minV)
            return;

        const auto norm = (float) (value - minV) / (float) (maxV - minV);
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    }

    static Result measure (double sr, int blockSize, int numChords, int density, double seconds,
                           int unison = 1, float drive = 0.0f, bool hq = false)
    {
        StabrProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, blockSize);
        proc.prepareToPlay (sr, blockSize);

        auto& apvts = proc.getAPVTS();

        // Sustain al maximo: las voces tienen que seguir sonando durante toda
        // la medicion, si no se apagan y medimos silencio.
        apvts.getParameter (pid::ampSustain)->setValueNotifyingHost (1.0f);

        // La poliofonia tiene que permitir los acordes que vamos a medir. Sin
        // esto, pedir mas acordes que el default hace que el robo apague la
        // mitad y la medicion miente.
        setChoiceOrInt (apvts.getParameter (pid::polyphony), numChords,
                        1, VoiceManager::kMaxPolyphony);
        setChoiceOrInt (apvts.getParameter (pid::chordDensity), density,
                        stabr::chord::kMinDensity, stabr::chord::kMaxDensity);
        setChoiceOrInt (apvts.getParameter (pid::unisonCount), unison, 1, Voice::kMaxUnison);
        apvts.getParameter (pid::driveAmount)->setValueNotifyingHost (drive);
        apvts.getParameter (pid::hqMode)->setValueNotifyingHost (hq ? 1.0f : 0.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);

        // Un acorde por tecla, repartidas a lo largo del teclado.
        juce::MidiBuffer firstMidi;
        for (int i = 0; i < numChords; ++i)
            firstMidi.addEvent (juce::MidiMessage::noteOn (1, 36 + i * 3, 0.9f), 0);

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
        r.voiceCount     = proc.getActiveVoiceCount();
        r.oscCount       = proc.getOscillatorCount();

        return r;
    }
};

static PerformanceTests performanceTests;
}
