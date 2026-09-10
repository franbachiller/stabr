#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Oscillators/PolyBlepOsc.h"
#include <vector>
#include <cmath>

namespace
{
class PolyBlepOscTests : public juce::UnitTest
{
public:
    PolyBlepOscTests() : juce::UnitTest ("PolyBlepOsc", "DSP") {}

    void runTest() override
    {
        beginTest ("salida acotada y finita en todo el rango MIDI y sample rates");
        {
            for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                stabr::PolyBlepOsc osc;
                osc.setSampleRate (sr);

                for (int note = 0; note <= 127; ++note)
                {
                    osc.setFrequency (440.0f * std::pow (2.0f, ((float) note - 69.0f) / 12.0f));
                    osc.reset (0.0f);

                    for (int i = 0; i < 2048; ++i)
                    {
                        const auto v = osc.processSaw();
                        expect (std::isfinite (v),
                                "NaN/inf en nota " + juce::String (note) + " @ " + juce::String (sr, 0));
                        expect (std::abs (v) <= 1.5f,
                                "salida fuera de rango (" + juce::String (v) + ") en nota "
                                    + juce::String (note));
                    }
                }
            }
        }

        beginTest ("frecuencia cero no rompe nada");
        {
            stabr::PolyBlepOsc osc;
            osc.setSampleRate (48000.0);
            osc.setFrequency (0.0f);
            osc.reset (0.0f);

            for (int i = 0; i < 1000; ++i)
                expect (std::isfinite (osc.processSaw()));
        }

        beginTest ("la frecuencia se clampea bajo Nyquist");
        {
            stabr::PolyBlepOsc osc;
            osc.setSampleRate (48000.0);
            osc.setFrequency (100000.0f);

            for (int i = 0; i < 1000; ++i)
                expect (std::isfinite (osc.processSaw()));
        }

        beginTest ("PolyBLEP reduce el aliasing frente a la sierra ingenua");
        {
            // A ~4.2 kHz con sr 48 kHz entran solo ~5 armonicos: todo lo demas
            // que aparezca en el espectro es aliasing.
            constexpr double sr = 48000.0;
            constexpr float freq = 4186.01f;    // C8
            constexpr int fftOrder = 13;        // 8192 puntos

            const auto aliasBlep  = measureAliasEnergy (sr, freq, fftOrder, true);
            const auto aliasNaive = measureAliasEnergy (sr, freq, fftOrder, false);

            logMessage ("  alias PolyBLEP: "
                        + juce::String (juce::Decibels::gainToDecibels (aliasBlep), 1) + " dB");
            logMessage ("  alias ingenua : "
                        + juce::String (juce::Decibels::gainToDecibels (aliasNaive), 1) + " dB");

            expect (aliasBlep < aliasNaive * 0.5,
                    "PolyBLEP no mejora sobre la sierra ingenua");
        }

        beginTest ("reset de fase es determinista (transitorio repetible)");
        {
            // Un stab tiene que sonar identico en cada disparo.
            stabr::PolyBlepOsc a, b;
            a.setSampleRate (48000.0);
            b.setSampleRate (48000.0);
            a.setFrequency (220.0f);
            b.setFrequency (220.0f);

            a.reset (0.0f);
            for (int i = 0; i < 5000; ++i)
                a.processSaw();

            a.reset (0.0f);
            b.reset (0.0f);

            for (int i = 0; i < 4096; ++i)
                expect (a.processSaw() == b.processSaw(), "el transitorio no es repetible");
        }
    }

private:
    /*  FFT sobre ventana Hann; devuelve la magnitud RMS de los bins que NO
        corresponden a armonicos de la fundamental. */
    static double measureAliasEnergy (double sr, float freq, int fftOrder, bool useBlep)
    {
        const int n = 1 << fftOrder;

        stabr::PolyBlepOsc osc;
        osc.setSampleRate (sr);
        osc.setFrequency (freq);
        osc.reset (0.0f);

        std::vector<float> data ((size_t) n * 2, 0.0f);
        const double phaseInc = freq / sr;
        double naivePhase = 0.0;

        for (int i = 0; i < n; ++i)
        {
            float v;

            if (useBlep)
            {
                v = osc.processSaw();
            }
            else
            {
                v = (float) (2.0 * naivePhase - 1.0);
                naivePhase += phaseInc;
                if (naivePhase >= 1.0)
                    naivePhase -= 1.0;
            }

            const auto w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1));
            data[(size_t) i] = v * (float) w;
        }

        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (data.data());

        const double binHz = sr / n;
        double aliasEnergy = 0.0;
        int aliasBins = 0;

        for (int k = 1; k < n / 2; ++k)
        {
            const double ratio = (k * binHz) / freq;

            if (std::abs (ratio - std::round (ratio)) < 0.05)
                continue;   // armonico legitimo

            const double mag = (double) data[(size_t) k] / (double) n;
            aliasEnergy += mag * mag;
            ++aliasBins;
        }

        return aliasBins > 0 ? std::sqrt (aliasEnergy / aliasBins) : 0.0;
    }
};

static PolyBlepOscTests polyBlepOscTests;
}
