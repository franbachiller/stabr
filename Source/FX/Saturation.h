#pragma once

#include "../DSP/Drive/Oversampler2x.h"
#include "FxUtils.h"
#include <array>
#include <cmath>

namespace stabr::fx
{
enum class SaturationType
{
    tape = 0,
    transistor,
    diode,
    bitcrush,
    numTypes
};

inline constexpr std::array<const char*, (size_t) SaturationType::numTypes> saturationTypeNames
{
    "Tape", "Transistor", "Diode", "Bitcrush"
};

struct SaturationParams
{
    bool enabled = true;
    SaturationType type = SaturationType::tape;
    float drive = 0.0f;        // 0..1
    float bits  = 12.0f;       // 2..16, solo bitcrush
    float rateReduction = 1.0f;// 1..50, divisor del sample rate, solo bitcrush
    float mix = 1.0f;
};

//==============================================================================
/*  Saturacion global, despues de la suma de voces.

    Aca si se paga x4 sin culpa: son dos canales, no cincuenta y seis voces. El
    costo que en el drive por voz era prohibitivo (D-013), aca es irrelevante.

    BITCRUSH NO SE SOBREMUESTREA, y es a proposito: su aliasing ES el efecto.
    Sobremuestrearlo lo volveria una distorsion suave y elegante, que es
    exactamente lo contrario de lo que alguien busca cuando pone "bitcrush".
*/
class Saturation
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset() noexcept
    {
        for (auto& o : stage1)
            o.reset();

        for (auto& o : stage2)
            o.reset();

        holdCounter = 0;
        heldL = 0.0f;
        heldR = 0.0f;
    }

    void setParams (const SaturationParams& p) noexcept
    {
        params = p;

        const float a = std::clamp (p.drive, 0.0f, 1.0f);
        gain = 1.0f + 23.0f * a * a;

        // Compensacion: solo atenua, nunca amplifica. Misma leccion que D-028.
        const float peak = std::abs (shape (gain, p.type));
        outputGain = peak > 1.0f ? 1.0f / peak : 1.0f;

        bypassed = a <= 0.0001f && p.type != SaturationType::bitcrush;
    }

    inline void process (float& l, float& r) noexcept
    {
        if (! params.enabled || bypassed)
            return;

        const float dryL = l, dryR = r;

        if (params.type == SaturationType::bitcrush)
        {
            l = crush (l, heldL);
            r = crush (r, heldR);

            if (++holdCounter >= (int) std::max (1.0f, params.rateReduction))
                holdCounter = 0;
        }
        else
        {
            l = oversampled (l, 0);
            r = oversampled (r, 1);
        }

        l = dryL + (l - dryL) * params.mix;
        r = dryR + (r - dryR) * params.mix;
    }

private:
    inline float oversampled (float x, int channel) noexcept
    {
        float a = 0.0f, b = 0.0f;
        stage1[(size_t) channel].upsample (x, a, b);

        a = processAtFour (a, channel);
        b = processAtFour (b, channel);

        return stage1[(size_t) channel].downsample (a, b);
    }

    inline float processAtFour (float x, int channel) noexcept
    {
        float a = 0.0f, b = 0.0f;
        stage2[(size_t) channel].upsample (x, a, b);

        a = shape (a * gain, params.type) * outputGain;
        b = shape (b * gain, params.type) * outputGain;

        return stage2[(size_t) channel].downsample (a, b);
    }

    /*  Las cuatro curvas. Todas ACOTADAS por construccion: la leccion de D-028
        fue que una curva que vuelve a crecer fuera de su rango amplifica los
        picos en vez de recortarlos. */
    static inline float shape (float x, SaturationType type) noexcept
    {
        switch (type)
        {
            case SaturationType::tape:
                // Simetrica y suave: comprime sin agregar aspereza.
                return std::tanh (std::clamp (x, -6.0f, 6.0f));

            case SaturationType::transistor:
            {
                // Rodilla mas dura y un poco asimetrica: mas agresiva.
                const float c = std::clamp (x, -4.0f, 4.0f);
                const float biased = c + 0.15f;
                return std::tanh (biased * 1.3f) - std::tanh (0.15f * 1.3f);
            }

            case SaturationType::diode:
            {
                /*  Muy asimetrica: el semiciclo positivo se recorta mucho antes
                    que el negativo, como un diodo real. Es la que mas armonicos
                    pares genera. */
                const float c = std::clamp (x, -5.0f, 5.0f);
                return c >= 0.0f ? std::tanh (c * 2.2f)
                                 : std::tanh (c * 0.7f) * 0.85f;
            }

            case SaturationType::bitcrush:
            case SaturationType::numTypes:
            default:
                return std::clamp (x, -1.0f, 1.0f);
        }
    }

    inline float crush (float x, float& held) noexcept
    {
        // Reduccion de sample rate: se retiene la muestra N ciclos.
        if (holdCounter == 0)
        {
            const float levels = std::pow (2.0f, std::clamp (params.bits, 2.0f, 16.0f));
            const float driven = std::clamp (x * gain, -1.0f, 1.0f);
            held = std::round (driven * levels) / levels;
        }

        return held * outputGain;
    }

    SaturationParams params;
    std::array<Oversampler2x, 2> stage1, stage2;

    double sampleRate = 44100.0;
    float gain = 1.0f;
    float outputGain = 1.0f;
    bool bypassed = true;

    int holdCounter = 0;
    float heldL = 0.0f, heldR = 0.0f;
};
}
