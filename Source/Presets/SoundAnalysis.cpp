#include "SoundAnalysis.h"
#include "../PluginProcessor.h"
#include "../Params/ParameterIDs.h"

#include <array>
#include <cmath>
#include <vector>

namespace stabr::presets
{
SoundReport analyse (StabrProcessor& engine, const Preset& preset, double sr)
{
    constexpr int blockSize = 512;
    constexpr double totalSeconds = 2.5;
    constexpr double noteSeconds = 0.25;
    constexpr int rootNote = 57;

    engine.setPlayConfigDetails (0, 2, sr, blockSize);
    engine.prepareToPlay (sr, blockSize);
    engine.reset();

    auto& apvts = engine.getAPVTS();
    applyPreset (apvts, preset);
    setPlain (apvts, pid::seqEnabled, 0.0f);

    const int total = (int) (totalSeconds * sr);
    const int noteOff = (int) (noteSeconds * sr);

    std::vector<float> left ((size_t) total, 0.0f), right ((size_t) total, 0.0f);
    juce::AudioBuffer<float> buffer (2, blockSize);

    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, total - pos);
        buffer.setSize (2, n, false, false, true);
        buffer.clear();

        juce::MidiBuffer midi;

        if (pos == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, rootNote, 0.9f), 0);

        if (noteOff >= pos && noteOff < pos + n)
            midi.addEvent (juce::MidiMessage::noteOff (1, rootNote), noteOff - pos);

        engine.processBlock (buffer, midi);

        std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + n, left.begin() + pos);
        std::copy (buffer.getReadPointer (1), buffer.getReadPointer (1) + n, right.begin() + pos);
    }

    SoundReport report;

    auto rmsDb = [&] (int from, int to)
    {
        double sum = 0.0;

        for (int i = from; i < to; ++i)
        {
            const double mid = 0.5 * ((double) left[(size_t) i] + (double) right[(size_t) i]);
            sum += mid * mid;
        }

        const double mean = sum / (double) juce::jmax (1, to - from);
        return (float) (10.0 * std::log10 (juce::jmax (mean, 1.0e-12)));
    };

    for (int i = 0; i < total; ++i)
    {
        const float l = left[(size_t) i], r = right[(size_t) i];

        if (! std::isfinite (l) || ! std::isfinite (r))
            report.finite = false;
        else
            report.peak = juce::jmax (report.peak, std::abs (l), std::abs (r));
    }

    /*  GOLPE: el RMS mas alto en ventanas de 100 ms, no el RMS de una ventana
        fija. La primera version usaba 300 ms y castigaba a los sonidos cortos:
        un blip de 70 ms daba -30 dB con el mismo pico que un acorde largo, y
        "calibrarlo" lo habria mandado contra el limitador. */
    const int window = (int) (0.1 * sr);
    const int hop = window / 4;
    float hit = -120.0f;

    for (int from = 0; from + window <= (int) (0.5 * sr); from += hop)
        hit = juce::jmax (hit, rmsDb (from, from + window));

    report.hitDb  = hit;
    report.tailDb = rmsDb ((int) (2.0 * sr), total);

    /*  GRAVES EN ESTEREO: el lateral por un pasabajos de 4 polos en 100 Hz.
        La primera version usaba UN polo en 150 Hz y media un cociente contra
        el central: 6 dB/oct dejan pasar el estereo del unisono, el chorus y la
        reverb de varios cientos de Hz, y el cociente de dos numeros diminutos
        salia cualquier cosa (1.07 en un preset con los graves en mono). Es el
        mismo error de D-035, esta vez del lado de la medicion. */
    const double a = std::exp (-2.0 * juce::MathConstants<double>::pi * 100.0 / sr);
    std::array<double, 4> lp {};
    double sumSide = 0.0;
    const int lowWindow = (int) (0.5 * sr);

    for (int i = 0; i < lowWindow; ++i)
    {
        double x = 0.5 * ((double) left[(size_t) i] - (double) right[(size_t) i]);

        for (auto& stage : lp)
        {
            stage = x + a * (stage - x);
            x = stage;
        }

        sumSide += x * x;
    }

    const double sideDb = 10.0 * std::log10 (juce::jmax (sumSide / lowWindow, 1.0e-12));
    report.lowSideDb = (float) sideDb - report.hitDb;
    return report;
}
}
