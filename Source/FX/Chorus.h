#pragma once

#include "FxUtils.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <cmath>

namespace stabr::fx
{
struct ChorusParams
{
    bool enabled  = true;
    bool ensemble = false;    // mas lineas, mas denso
    float rate    = 0.6f;     // Hz
    float depth   = 0.5f;     // 0..1
    float width   = 0.8f;     // 0..1
    float mix     = 0.0f;     // 0..1
};

//==============================================================================
/*  Chorus tipo BBD, con modo ensemble.

    CARACTER JUNO: dos lineas de retardo cortas moduladas por LFOs EN
    CONTRAFASE, con el pasabajos que tenian los chips BBD reales. Lo que le da
    el sonido no es la modulacion sino esa perdida de agudos: un chorus digital
    limpio suena a efecto, el BBD suena a instrumento.

    Las lineas son cortas (5 a 25 ms) y NO hay realimentacion. Con
    realimentacion seria un flanger, que es otro efecto.

    ENSEMBLE: cuatro lineas con LFOs repartidos en fase en vez de dos en
    contrafase. Mas espeso y menos identificable como "chorus".
*/
class Chorus
{
public:
    static constexpr int kMaxLines = 4;

    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;

        const int maxSamples = (int) std::ceil (kMaxDelayMs * 0.001 * sampleRate) + 8;

        for (auto& line : lines)
            line.assign ((size_t) maxSamples, 0.0f);

        bufferSize = maxSamples;

        for (auto& f : bbdFilter)
        {
            f.setSampleRate (sampleRate);
            f.setCutoff (kBbdCutoffHz);
        }

        reset();
    }

    void reset() noexcept
    {
        for (auto& line : lines)
            std::fill (line.begin(), line.end(), 0.0f);

        for (auto& f : bbdFilter)
            f.reset();

        writeIndex = 0;
        lfoPhase = 0.0f;
    }

    void setParams (const ChorusParams& p) noexcept
    {
        params = p;
        lfoInc = (float) (std::clamp (p.rate, 0.02f, 10.0f) / sampleRate);
        numLines = p.ensemble ? 4 : 2;

        baseSamples  = (float) (kBaseDelayMs * 0.001 * sampleRate);
        depthSamples = (float) (kDepthMs * 0.001 * sampleRate)
                           * std::clamp (p.depth, 0.0f, 1.0f);
    }

    inline void process (float& l, float& r) noexcept
    {
        if (! params.enabled || params.mix <= 0.0001f)
            return;

        const float dryL = l, dryR = r;
        const float mono = (dryL + dryR) * 0.5f;

        // Una sola escritura compartida: las lineas se diferencian por donde
        // leen, no por lo que guardan.
        for (int i = 0; i < numLines; ++i)
            lines[(size_t) i][(size_t) writeIndex] = mono;

        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        float wetL = 0.0f, wetR = 0.0f;

        for (int i = 0; i < numLines; ++i)
        {
            /*  Contrafase con dos lineas, repartidas con cuatro. En contrafase
                una linea sube mientras la otra baja, y el movimiento opuesto
                es lo que se percibe como ancho. */
            const float phase = lfoPhase + (float) i / (float) numLines;
            const float wrapped = phase - std::floor (phase);
            const float lfo = std::sin (wrapped * juce::MathConstants<float>::twoPi);

            const float delay = baseSamples + depthSamples * (1.0f + lfo) * 0.5f;
            float sample = readLine (i, delay);

            // El pasabajos del BBD: la perdida de agudos que le da el caracter.
            sample = bbdFilter[(size_t) i].processLowpass (sample);

            // Paneo: las lineas se reparten a lo ancho segun el control.
            const float pan = (numLines == 1) ? 0.0f
                                              : (2.0f * (float) i / (float) (numLines - 1) - 1.0f);
            const float panned = pan * std::clamp (params.width, 0.0f, 1.0f);

            wetL += sample * (0.5f - 0.5f * panned);
            wetR += sample * (0.5f + 0.5f * panned);
        }

        const float norm = 2.0f / (float) numLines;
        wetL *= norm;
        wetR *= norm;

        writeIndex = (writeIndex + 1) % bufferSize;

        l = dryL + (wetL - dryL) * params.mix;
        r = dryR + (wetR - dryR) * params.mix;
    }

private:
    inline float readLine (int index, float delaySamples) const noexcept
    {
        const auto& line = lines[(size_t) index];

        float pos = (float) writeIndex - std::clamp (delaySamples, 1.0f,
                                                     (float) (bufferSize - 2));

        while (pos < 0.0f)
            pos += (float) bufferSize;

        const int i0 = (int) pos % bufferSize;
        const int i1 = (i0 + 1) % bufferSize;
        const float frac = pos - std::floor (pos);

        return line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
    }

    static constexpr double kMaxDelayMs   = 40.0;
    static constexpr double kBaseDelayMs  = 6.0;
    static constexpr double kDepthMs      = 14.0;
    static constexpr float  kBbdCutoffHz  = 7200.0f;

    std::array<std::vector<float>, kMaxLines> lines;
    std::array<OnePoleTpt, kMaxLines> bbdFilter;

    ChorusParams params;
    double sampleRate = 44100.0;
    int bufferSize = 0;
    int writeIndex = 0;
    int numLines = 2;

    float lfoPhase = 0.0f, lfoInc = 0.0f;
    float baseSamples = 100.0f, depthSamples = 50.0f;
};
}
