#pragma once

#include <cmath>
#include <algorithm>
#include <array>

namespace stabr
{
enum class FilterType
{
    lowpass = 0,
    bandpass,
    highpass,
    numTypes
};

inline constexpr std::array<const char*, (size_t) FilterType::numTypes> filterTypeNames
{
    "Low Pass", "Band Pass", "High Pass"
};

//==============================================================================
/*  Coeficientes del SVF, SEPARADOS del estado.

    POR QUE ESTAN SEPARADOS: los dos canales de una voz tienen exactamente el
    mismo cutoff y la misma resonancia, y solo se diferencian en el estado.
    Si cada filtro calculara sus propios coeficientes, la voz haria cuatro
    divisiones por muestra (dos canales x dos etapas) para obtener cuatro veces
    el mismo resultado. Calculandolos una vez y compartiendolos, son dos.
*/
struct SvfCoeffs
{
    float k  = 2.0f;      // 1/Q
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    static SvfCoeffs make (float g, float k) noexcept
    {
        SvfCoeffs c;
        c.k  = k;
        c.a1 = 1.0f / (1.0f + g * (g + k));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
        return c;
    }

    /*  Interpolacion lineal de los coeficientes ya calculados.

        a1, a2 y a3 son funciones suaves de g, asi que interpolarlos
        directamente entre dos puntos de control es indistinguible de
        recalcularlos, y evita la division por muestra. Esa division era el
        costo dominante del filtro. */
    void addStep (const SvfCoeffs& step) noexcept
    {
        a1 += step.a1;
        a2 += step.a2;
        a3 += step.a3;
    }

    static SvfCoeffs stepBetween (const SvfCoeffs& from, const SvfCoeffs& to, float inverseSteps) noexcept
    {
        SvfCoeffs s;
        s.k  = to.k;
        s.a1 = (to.a1 - from.a1) * inverseSteps;
        s.a2 = (to.a2 - from.a2) * inverseSteps;
        s.a3 = (to.a3 - from.a3) * inverseSteps;
        return s;
    }
};

//==============================================================================
/*  Prewarp del cutoff. tan() es caro: se llama a tasa de control, no por
    muestra. Ver Voice::kControlRateSamples. */
inline float gForCutoff (float cutoffHz, double sampleRate) noexcept
{
    const auto maxCutoff = (float) (sampleRate * 0.49);
    const auto hz = std::clamp (cutoffHz, 10.0f, maxCutoff);

    return (float) std::tan (3.14159265358979323846 * (double) hz / sampleRate);
}

/*  resonance 0..1 -> k = 1/Q.

    kMinDamping = 0 es auto-oscilacion EXACTA: el filtro queda marginalmente
    estable y sostiene el tono indefinidamente. Es lo que pide el brief, y es
    el sonido acid/rave clasico. Con 0.02 (el primer intento) daba Q=50, que
    suena a resonancia fuerte pero se apaga en ~8 ms: el test que verifica que
    la oscilacion SE SOSTIENE lo detecto.

    Lo que la acota no es el amortiguamiento sino el saturador del estado del
    pasabanda, igual que en un filtro analogico de verdad. */
inline constexpr float kMinDamping = 0.0f;

inline float kForResonance (float resonance01) noexcept
{
    const auto r = std::clamp (resonance01, 0.0f, 1.0f);
    return 2.0f - (2.0f - kMinDamping) * r;
}

//==============================================================================
/*  State Variable Filter topology-preserving transform (Zavalishin / Simper).

    POR QUE ESTE Y NO UN BIQUAD: el cutoff se modula continuamente desde la
    envolvente de filtro. Un biquad con coeficientes recalculados al vuelo es
    caro e inestable al barrer rapido; la forma TPT esta pensada exactamente
    para eso y se mantiene estable barriendo a velocidad de audio. Ademas da
    LP, BP y HP del mismo estado, gratis.

    Esta clase es SOLO ESTADO: dos variables. Los coeficientes llegan de afuera.
*/
class SvfTpt
{
public:
    void reset() noexcept
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    inline float process (float x, const SvfCoeffs& c, FilterType type) noexcept
    {
        const float v3 = x - ic2eq;
        const float v1 = c.a1 * ic1eq + c.a2 * v3;
        const float v2 = ic2eq + c.a2 * ic1eq + c.a3 * v3;

        // El estado del pasabanda es el lazo de resonancia. Saturarlo acota la
        // auto-oscilacion sin ensuciar el filtro en resonancias bajas, que es
        // lo que hace un filtro analogico de verdad. Sin esto, con k -> 0 la
        // oscilacion crece sin limite.
        ic1eq = softClip (2.0f * v1 - ic1eq);
        ic2eq = 2.0f * v2 - ic2eq;

        switch (type)
        {
            case FilterType::lowpass:  return v2;
            case FilterType::bandpass: return v1;
            case FilterType::highpass: return x - c.k * v1 - v2;

            case FilterType::numTypes:
            default:                   return v2;
        }
    }

private:
    /*  Saturador algebraico: acotado a +-limit por construccion, sin ramas ni
        exponenciales. Transparente muy por debajo del limite. */
    static inline float softClip (float x) noexcept
    {
        constexpr float limit = 2.0f;
        const float s = x / limit;
        return limit * s / std::sqrt (1.0f + s * s);
    }

    float ic1eq = 0.0f, ic2eq = 0.0f;
};

//==============================================================================
/*  Filtro estereo de la voz: 12 o 24 dB/oct, dos canales.

    24 dB = dos SVF en cascada. El primero lleva la resonancia del usuario y el
    segundo queda en Butterworth: asi la pendiente es limpia y la resonancia no
    se eleva al cuadrado, que es lo que pasa al cascadear dos etapas identicas
    y hace que el filtro se vuelva incontrolable al subir Q.

    Los cuatro SVF (dos etapas x dos canales) comparten los mismos dos juegos de
    coeficientes.
*/
class StereoLadder
{
public:
    enum class Slope { db12 = 0, db24, numSlopes };

    // k = 0.707 -> Q = 1.414, Butterworth.
    static constexpr float kButterworthK = 0.7071f;

    void reset() noexcept
    {
        for (auto& s : stages)
            s.reset();
    }

    void setType (FilterType t) noexcept  { type = t; }
    void setSlope (Slope s) noexcept      { slope = s; }
    void setResonance (float r01) noexcept { resonance = std::clamp (r01, 0.0f, 1.0f); }
    void setPeakCompensation (bool c) noexcept { compensate = c; }

    float getResonanceK() const noexcept { return kForResonance (resonance); }

    /*  Coeficientes para un g dado. El llamador los calcula a tasa de control y
        los interpola por muestra. */
    SvfCoeffs coeffsStage1 (float g) const noexcept
    {
        return SvfCoeffs::make (g, kForResonance (resonance));
    }

    static SvfCoeffs coeffsStage2 (float g) noexcept
    {
        return SvfCoeffs::make (g, kButterworthK);
    }

    inline void process (float& l, float& r,
                         const SvfCoeffs& c1, const SvfCoeffs& c2) noexcept
    {
        l = stages[0].process (l, c1, type);
        r = stages[1].process (r, c1, type);

        if (slope == Slope::db24)
        {
            l = stages[2].process (l, c2, type);
            r = stages[3].process (r, c2, type);
        }

        if (compensate)
        {
            /*  Doma el pico de resonancia para que crankearla no obligue a
                bajar el fader.

                NOTA HONESTA: en un SVF la banda pasante NO pierde ganancia (a
                diferencia de un ladder con realimentacion), asi que esto no
                "recupera" nada: acota el pico. Util igual, pero no es lo mismo
                que la compensacion de ganancia de un ladder. */
            const float g = 1.0f / (1.0f + 0.8f * resonance);
            l *= g;
            r *= g;
        }
    }

private:
    std::array<SvfTpt, 4> stages;   // [0]=L1 [1]=R1 [2]=L2 [3]=R2
    FilterType type = FilterType::lowpass;
    Slope slope = Slope::db24;
    float resonance = 0.0f;
    bool compensate = false;
};
}
