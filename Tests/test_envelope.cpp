#include <juce_core/juce_core.h>
#include "DSP/Env/AdsrExp.h"

namespace
{
class AdsrExpTests : public juce::UnitTest
{
public:
    AdsrExpTests() : juce::UnitTest ("AdsrExp", "DSP") {}

    void runTest() override
    {
        beginTest ("arranca en Idle y devuelve cero exacto");
        {
            stabr::AdsrExp env;
            env.setSampleRate (48000.0);
            expect (! env.isActive());

            for (int i = 0; i < 1000; ++i)
                expect (env.getNextSample() == 0.0f, "silencio no exacto en Idle");
        }

        beginTest ("el ataque llega a 1.0 y no se pasa");
        {
            stabr::AdsrExp env;
            env.setSampleRate (48000.0);
            env.setParameters (10.0f, 500.0f, 1.0f, 100.0f);
            env.noteOn();

            float peak = 0.0f;
            for (int i = 0; i < 48000; ++i)
                peak = juce::jmax (peak, env.getNextSample());

            expectWithinAbsoluteError (peak, 1.0f, 1.0e-4f);
            expect (peak <= 1.0f + 1.0e-6f, "la envolvente se pasa de 1.0");
        }

        beginTest ("el ataque respeta su duracion (+-15%)");
        {
            for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                stabr::AdsrExp env;
                env.setSampleRate (sr);
                env.setParameters (50.0f, 1000.0f, 1.0f, 100.0f);
                env.noteOn();

                int samplesToPeak = 0;
                for (int i = 0; i < (int) sr; ++i)
                {
                    if (env.getNextSample() >= 0.999f)
                    {
                        samplesToPeak = i;
                        break;
                    }
                }

                const auto ms = 1000.0 * samplesToPeak / sr;
                expect (ms > 42.0 && ms < 58.0,
                        "ataque de 50 ms medido en " + juce::String (ms, 2)
                            + " ms a " + juce::String (sr, 0) + " Hz");
            }
        }

        beginTest ("el decay converge al sustain");
        {
            stabr::AdsrExp env;
            env.setSampleRate (48000.0);
            env.setParameters (1.0f, 100.0f, 0.5f, 100.0f);
            env.noteOn();

            for (int i = 0; i < 48000; ++i)
                env.getNextSample();

            expectWithinAbsoluteError (env.getLevel(), 0.5f, 1.0e-3f);
            expect (env.getStage() == stabr::AdsrExp::Stage::Sustain);
        }

        beginTest ("sustain 0 libera la voz sin llegar al release");
        {
            // Este es el caso por defecto del plugin: un stab con sustain 0 no
            // debe quedar ocupando una voz del pool despues del decay.
            stabr::AdsrExp env;
            env.setSampleRate (48000.0);
            env.setParameters (0.5f, 80.0f, 0.0f, 120.0f);
            env.noteOn();

            int i = 0;
            for (; i < 48000 && env.isActive(); ++i)
                env.getNextSample();

            expect (! env.isActive(), "la voz no se libero con sustain 0");
            expect (env.getLevel() == 0.0f, "no termino en cero exacto");
            expect (i < 48000, "tardo mas de un segundo en liberarse");
        }

        beginTest ("el release termina en cero EXACTO y en tiempo finito");
        {
            // Una exponencial que apunta a cero nunca llega. Si este test
            // falla, el plugin deja denormals colgando para siempre.
            stabr::AdsrExp env;
            env.setSampleRate (48000.0);
            env.setParameters (1.0f, 200.0f, 0.8f, 150.0f);
            env.noteOn();

            for (int i = 0; i < 24000; ++i)
                env.getNextSample();

            env.noteOff();

            int samples = 0;
            while (env.isActive() && samples < 48000 * 10)
            {
                env.getNextSample();
                ++samples;
            }

            expect (! env.isActive(), "el release nunca termino");
            expect (env.getLevel() == 0.0f, "release no termino en 0.0 exacto");

            const auto ms = 1000.0 * samples / 48000.0;
            expect (ms > 100.0 && ms < 400.0,
                    "release de 150 ms termino en " + juce::String (ms, 1) + " ms");

            for (int i = 0; i < 100; ++i)
                expect (env.getNextSample() == 0.0f);
        }

        beginTest ("tiempo cero no produce NaN");
        {
            stabr::AdsrExp env;
            env.setSampleRate (48000.0);
            env.setParameters (0.0f, 0.0f, 0.0f, 0.0f);
            env.noteOn();

            for (int i = 0; i < 1000; ++i)
            {
                const auto v = env.getNextSample();
                expect (std::isfinite (v), "NaN/inf con tiempos en cero");
            }
        }

        beginTest ("retrigger no produce salto");
        {
            stabr::AdsrExp env;
            env.setSampleRate (48000.0);
            env.setParameters (20.0f, 300.0f, 0.6f, 200.0f);
            env.noteOn();

            for (int i = 0; i < 2000; ++i)
                env.getNextSample();

            const auto before = env.getLevel();
            env.noteOn();
            const auto after = env.getNextSample();

            expect (std::abs (after - before) < 0.05f,
                    "el retrigger salta de " + juce::String (before) + " a " + juce::String (after));
        }
    }
};

static AdsrExpTests adsrExpTests;
}
