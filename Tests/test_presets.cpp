#include <juce_core/juce_core.h>
#include "PluginProcessor.h"
#include "Params/ParameterIDs.h"
#include "Presets/Preset.h"
#include "Presets/FactoryPresets.h"
#include "Presets/SoundAnalysis.h"
#include "Presets/StabGenerator.h"
#include "Presets/PresetManager.h"

#include <set>
#include <vector>

namespace
{
using namespace stabr;
using namespace stabr::presets;

//==============================================================================
class FactoryPresetTests : public juce::UnitTest
{
public:
    FactoryPresetTests() : juce::UnitTest ("Factory presets", "Presets") {}

    void runTest() override
    {
        const auto& list = factoryPresets();

        beginTest ("20 presets, nombres unicos, IDs validos, valores en rango y sin parametros de sesion");
        {
            StabrProcessor proc;
            auto& apvts = proc.getAPVTS();

            expectEquals ((int) list.size(), 20);

            std::set<juce::String> names;

            for (const auto& preset : list)
            {
                expect (names.insert (preset.name).second, "nombre repetido: " + preset.name);
                expect (preset.description.isNotEmpty(), preset.name + " no tiene descripcion");

                for (const auto& v : preset.values)
                {
                    auto* param = apvts.getParameter (v.id);
                    expect (param != nullptr, preset.name + ": ID desconocido " + v.id);

                    if (param == nullptr)
                        continue;

                    expect (! isSessionParameter (v.id), preset.name + " toca la sesion: " + v.id);

                    const auto range = param->getNormalisableRange();
                    expect (v.value >= range.start - 1.0e-4f && v.value <= range.end + 1.0e-4f,
                            preset.name + ": " + v.id + " = " + juce::String (v.value) + " fuera de rango");
                }
            }
        }

        beginTest ("cargar un preset no toca la sesion y deja lo no mencionado en su default");
        {
            StabrProcessor proc;
            auto& apvts = proc.getAPVTS();
            auto& manager = proc.getPresetManager();

            setPlain (apvts, pid::scaleKey, 3.0f);
            setPlain (apvts, pid::masterGain, -12.0f);
            setPlain (apvts, pid::unisonDetune, 40.0f);   // Classic Organ Stab no lo menciona

            manager.loadFactoryPreset (0);

            expectWithinAbsoluteError (getPlain (apvts, pid::scaleKey), 3.0f, 1.0e-4f);
            expectWithinAbsoluteError (getPlain (apvts, pid::masterGain), -12.0f, 0.05f);
            expectWithinAbsoluteError (getPlain (apvts, pid::unisonDetune), 14.0f, 0.05f);
            expectWithinAbsoluteError (getPlain (apvts, pid::osc1Wave), 3.0f, 1.0e-4f);
            expectEquals (manager.getCurrentPresetName(), list[0].name);
        }

        beginTest ("ningun par de presets es casi igual");
        {
            StabrProcessor proc;
            float nearest = 1.0f;
            juce::String pair;

            for (size_t i = 0; i < list.size(); ++i)
                for (size_t j = i + 1; j < list.size(); ++j)
                {
                    const float d = distance (proc.getAPVTS(), list[i], list[j]);

                    if (d < nearest)
                    {
                        nearest = d;
                        pair = list[i].name + " / " + list[j].name;
                    }
                }

            logMessage ("par mas parecido: " + pair + " (distancia " + juce::String (nearest, 3) + ")");
            expect (nearest >= StabGenerator::kMinDistance, "demasiado parecidos: " + pair);
        }

        beginTest ("cada preset suena, no satura, deja los graves en mono y golpea al volumen objetivo");
        {
            StabrProcessor engine;

            logMessage ("  preset               | golpe dB | cola dB | pico  | lateral <100Hz | ganancia sugerida");
            logMessage ("  ---------------------+----------+---------+-------+----------------+------------------");

            for (const auto& preset : list)
            {
                const auto r = analyse (engine, preset);

                /*  Calibracion: la ganancia de salida que dejaria el preset en
                    el objetivo, con la misma iteracion de dos pasadas que usa
                    Create STAB (el limitador no es lineal). */
                auto calibrated = preset;
                float gain = 0.0f;

                if (const auto* current = preset.find (pid::outputGain))
                    gain = current->value;

                auto report = r;

                for (int pass = 0; pass < 3 && std::abs (report.hitDb - kTargetHitDb) > 0.2f; ++pass)
                {
                    gain = juce::jlimit (-24.0f, 12.0f, gain + (kTargetHitDb - report.hitDb));

                    bool replaced = false;
                    for (auto& pv : calibrated.values)
                        if (pv.id == pid::outputGain) { pv.value = gain; replaced = true; }

                    if (! replaced)
                        calibrated.values.push_back ({ pid::outputGain, gain });

                    report = analyse (engine, calibrated);
                }

                logMessage ("  " + preset.name.paddedRight (' ', 20)
                            + " | " + juce::String (r.hitDb, 1).paddedLeft (' ', 8)
                            + " | " + juce::String (r.tailDb, 1).paddedLeft (' ', 7)
                            + " | " + juce::String (r.peak, 3).paddedLeft (' ', 5)
                            + " | " + (juce::String (r.lowSideDb, 1) + " dB").paddedLeft (' ', 14)
                            + " | " + juce::String (gain, 1) + " dB -> " + juce::String (report.hitDb, 1));

                expect (r.finite, preset.name + " produce NaN/inf");
                expect (! r.isSilent(), preset.name + " casi no suena");
                expect (r.peak <= 1.0f, preset.name + " satura: pico " + juce::String (r.peak, 3));
                expect (r.hasMonoBass(), preset.name + " tiene graves en estereo: " + juce::String (r.lowSideDb, 1) + " dB");
                expect (std::abs (r.hitDb - kTargetHitDb) <= 2.0f,
                        preset.name + " golpea a " + juce::String (r.hitDb, 1) + " dB, objetivo " + juce::String (kTargetHitDb, 1));
            }
        }
    }
};

//==============================================================================
class CreateStabTests : public juce::UnitTest
{
public:
    CreateStabTests() : juce::UnitTest ("Create STAB", "Presets") {}

    void runTest() override
    {
        beginTest ("40 stabs con semillas fijas: verificados, al volumen objetivo y todos distintos");
        {
            StabrProcessor proc;
            auto& manager = proc.getPresetManager();

            std::vector<Preset> results;
            std::map<juce::String, int> archetypes;
            int accepted = 0, attempts = 0;
            int silent = 0, loudness = 0, peak = 0, tail = 0, bass = 0, similar = 0;
            float worstLoudness = 0.0f;

            const double start = juce::Time::getMillisecondCounterHiRes();

            for (int i = 0; i < 40; ++i)
            {
                const auto r = manager.createStabNow (1000 + i, {});

                accepted += r.accepted ? 1 : 0;
                attempts += r.attempts;
                ++archetypes[r.archetype];
                worstLoudness = juce::jmax (worstLoudness, std::abs (r.report.hitDb - kTargetHitDb));

                silent += r.rejectedSilent;   loudness += r.rejectedLoudness; peak += r.rejectedPeak;
                tail += r.rejectedTail;       bass += r.rejectedBass;         similar += r.rejectedSimilar;

                if (! r.accepted)
                    logMessage ("NO ACEPTADO: " + r.preset.name + " tras " + juce::String (r.attempts) + " intentos -"
                                + " silencio " + juce::String (r.rejectedSilent) + ", volumen " + juce::String (r.rejectedLoudness)
                                + ", pico " + juce::String (r.rejectedPeak) + ", cola " + juce::String (r.rejectedTail)
                                + ", graves " + juce::String (r.rejectedBass) + ", parecido " + juce::String (r.rejectedSimilar)
                                + " | golpe " + juce::String (r.report.hitDb, 1) + " dB, cola " + juce::String (r.report.tailDb, 1)
                                + " dB, pico " + juce::String (r.report.peak, 3) + ", graves " + juce::String (r.report.lowSideDb, 1) + " dB");

                expect (r.report.finite && ! r.report.isSilent(), r.preset.name + " no suena");
                expect (r.report.peak <= 1.0f, r.preset.name + " satura");
                results.push_back (r.preset);
            }

            const double msPerStab = (juce::Time::getMillisecondCounterHiRes() - start) / 40.0;

            juce::String mix;
            for (const auto& [archetypeName, count] : archetypes)
                mix << archetypeName << " " << count << "  ";

            logMessage ("aceptados " + juce::String (accepted) + "/40, intentos promedio "
                        + juce::String ((double) attempts / 40.0, 2) + ", peor desvio de volumen "
                        + juce::String (worstLoudness, 2) + " dB, " + juce::String (msPerStab, 0) + " ms por stab");
            logMessage ("arquetipos: " + mix);
            logMessage ("rechazos por motivo (sobre todos los intentos): silencio " + juce::String (silent)
                        + ", volumen " + juce::String (loudness) + ", pico " + juce::String (peak)
                        + ", cola " + juce::String (tail) + ", graves " + juce::String (bass)
                        + ", parecido " + juce::String (similar));

            expectEquals (accepted, 40);
            expect (worstLoudness <= StabGenerator::kLoudnessToleranceDb);

            float nearest = 1.0f;
            for (size_t i = 0; i < results.size(); ++i)
                for (size_t j = i + 1; j < results.size(); ++j)
                    nearest = juce::jmin (nearest, distance (proc.getAPVTS(), results[i], results[j]));

            logMessage ("distancia minima entre stabs generados: " + juce::String (nearest, 3));
            expect (nearest >= StabGenerator::kMinDistance);
        }

        beginTest ("la misma semilla da el mismo sonido");
        {
            StabrProcessor a, b;
            const auto ra = a.getPresetManager().createStabNow (42, {});
            const auto rb = b.getPresetManager().createStabNow (42, {});

            expectEquals (distance (a.getAPVTS(), ra.preset, rb.preset), 0.0f);
            expectEquals (ra.preset.name, rb.preset.name);
        }

        beginTest ("candados: el acorde y los efectos quedan, la tonalidad nunca cambia");
        {
            StabrProcessor proc;
            auto& apvts = proc.getAPVTS();

            setPlain (apvts, pid::scaleKey, 5.0f);
            setPlain (apvts, pid::scaleMode, 3.0f);
            setPlain (apvts, pid::chordType, 8.0f);
            setPlain (apvts, pid::chordSpread, 2.0f);
            setPlain (apvts, pid::delayMix, 0.77f);
            setPlain (apvts, pid::seqEnabled, 1.0f);

            GeneratorLocks locks;
            locks.chord = true;
            locks.fx = true;
            proc.getPresetManager().createStabNow (7, locks);

            expectWithinAbsoluteError (getPlain (apvts, pid::chordType), 8.0f, 1.0e-4f);
            expectWithinAbsoluteError (getPlain (apvts, pid::chordSpread), 2.0f, 1.0e-4f);
            expectWithinAbsoluteError (getPlain (apvts, pid::delayMix), 0.77f, 1.0e-3f);
            expectWithinAbsoluteError (getPlain (apvts, pid::scaleKey), 5.0f, 1.0e-4f);
            expectWithinAbsoluteError (getPlain (apvts, pid::scaleMode), 3.0f, 1.0e-4f);
            expectWithinAbsoluteError (getPlain (apvts, pid::seqEnabled), 1.0f, 1.0e-4f);

            proc.getPresetManager().createStabNow (8, {});
            expectWithinAbsoluteError (getPlain (apvts, pid::scaleKey), 5.0f, 1.0e-4f);
            expectWithinAbsoluteError (getPlain (apvts, pid::seqEnabled), 1.0f, 1.0e-4f, "generar un sonido apago el secuenciador");
        }

        beginTest ("deshacer y rehacer vuelven exactamente al sonido anterior");
        {
            StabrProcessor proc;
            auto& apvts = proc.getAPVTS();
            auto& manager = proc.getPresetManager();

            manager.loadFactoryPreset (7);
            const auto before = capturePreset (apvts, "before");

            manager.createStabNow (99, {});
            const auto after = capturePreset (apvts, "after");
            expect (distance (apvts, before, after) > 0.0f);

            // Tolerancia de 1e-6 y no 0: capturar y aplicar convierte entre
            // valor plano y normalizado, y esa ida y vuelta deja error de
            // redondeo del orden de 1e-10. Ninguna diferencia audible.
            expect (manager.canUndo());
            manager.undo();
            expectWithinAbsoluteError (distance (apvts, capturePreset (apvts, {}), before), 0.0f, 1.0e-6f);
            expectEquals (manager.getCurrentPresetName(), factoryPresets()[7].name);

            expect (manager.canRedo());
            manager.redo();
            expectWithinAbsoluteError (distance (apvts, capturePreset (apvts, {}), after), 0.0f, 1.0e-6f);
        }

        beginTest ("guardar y cargar un preset de usuario");
        {
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("stabr_test_presets");
            dir.deleteRecursively();

            StabrProcessor proc;
            auto& apvts = proc.getAPVTS();
            auto& manager = proc.getPresetManager();
            manager.setUserDirectory (dir);

            manager.loadFactoryPreset (3);
            setPlain (apvts, pid::filterCutoff, 1234.0f);
            const auto saved = capturePreset (apvts, {});

            const auto result = manager.saveUserPreset ("My: Stab?");
            expect (result.wasOk(), result.getErrorMessage());
            expectEquals (manager.getUserPresetFiles().size(), 1);

            manager.loadFactoryPreset (10);
            expect (distance (apvts, capturePreset (apvts, {}), saved) > 0.0f);

            expect (manager.loadUserPreset (manager.getUserPresetFiles()[0]));

            // No se exige 0 exacto: el valor pasa por texto decimal en el XML y
            // vuelve con error de redondeo del orden de 1e-9. En memoria (undo)
            // si es exacto, y ese test lo exige.
            expectWithinAbsoluteError (distance (apvts, capturePreset (apvts, {}), saved), 0.0f, 1.0e-6f);
            expectEquals (manager.getCurrentPresetName(), juce::String ("My: Stab?"));

            expect (! manager.saveUserPreset ("   ").wasOk(), "un nombre vacio no deberia guardarse");

            dir.deleteRecursively();
        }

        beginTest ("el nombre del sonido se guarda con el proyecto");
        {
            StabrProcessor proc;
            proc.getPresetManager().loadFactoryPreset (2);

            juce::MemoryBlock blob;
            proc.getStateInformation (blob);

            StabrProcessor other;
            other.setStateInformation (blob.getData(), (int) blob.getSize());
            expectEquals (other.getPresetManager().getCurrentPresetName(), factoryPresets()[2].name);
        }
    }
};

static FactoryPresetTests factoryPresetTests;
static CreateStabTests createStabTests;
}
