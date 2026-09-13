#pragma once

#include <array>
#include <algorithm>

namespace stabr::seq
{
inline constexpr int kNumSteps   = 16;
inline constexpr int kMaxRatchet = 3;

struct Step
{
    bool  on          = false;
    float velocity    = 0.8f;   // 0..1
    float probability = 1.0f;   // 0..1
    int   ratchet     = 1;      // 1..kMaxRatchet disparos dentro del paso
};

using Pattern = std::array<Step, (size_t) kNumSteps>;

//==============================================================================
/*  Resolucion del paso. Orden estrictamente creciente, igual que las divisiones
    de TempoSync.h (ver DECISIONS.md D-033). */
enum class StepRate
{
    n1_32 = 0,
    n1_16T,
    n1_16,
    n1_8T,
    n1_8,
    n1_4,
    numRates
};

inline constexpr std::array<double, (size_t) StepRate::numRates> rateInBeats
{
    0.125,               // 1/32
    0.25 * 2.0 / 3.0,    // 1/16T
    0.25,                // 1/16
    0.5  * 2.0 / 3.0,    // 1/8T
    0.5,                 // 1/8
    1.0                  // 1/4
};

inline constexpr std::array<const char*, (size_t) StepRate::numRates> rateNames
{
    "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4"
};

inline double stepLengthInBeats (StepRate r) noexcept
{
    const auto i = std::clamp ((int) r, 0, (int) StepRate::numRates - 1);
    return rateInBeats[(size_t) i];
}

//==============================================================================
/*  Patrones de fabrica. Ver DECISIONS.md D-039.

    'Custom' toca los 16 pasos editables (parametros seq_step_*). Los demas son
    tablas de solo lectura: elegir uno NO sobrescribe los pasos editables. Asi
    el patron se puede automatizar desde el host sin tocar parametros desde el
    hilo de audio, y cambiar de patron nunca destruye lo que el usuario edito.

    Todos estan escritos para 1/16 y 16 pasos = un compas de 4/4. */
enum class PatternId
{
    custom = 0,
    offbeat8,
    tresillo332,
    skippy16,
    sparseTechno,
    numPatterns
};

inline constexpr std::array<const char*, (size_t) PatternId::numPatterns> patternNames
{
    "Custom", "Off-beat 8ths", "3+3+2", "Skippy 16ths", "Sparse Techno"
};

namespace detail
{
    constexpr Step hit (float velocity, float probability = 1.0f, int ratchet = 1) noexcept
    {
        return { true, velocity, probability, ratchet };
    }
}

/*  El contratiempo de corcheas: el "and" de cada negra. EL stab de house. */
inline constexpr Pattern kOffbeat8 = []
{
    Pattern p {};
    p[2]  = detail::hit (0.90f);
    p[6]  = detail::hit (0.90f);
    p[10] = detail::hit (0.90f);
    p[14] = detail::hit (0.90f);
    return p;
}();

/*  3+3+2 en corcheas sobre el compas: el tresillo cubano. Acento en el uno,
    los otros dos empujan contra el cuatro por cuatro del bombo. */
inline constexpr Pattern kTresillo332 = []
{
    Pattern p {};
    p[0]  = detail::hit (1.00f);
    p[6]  = detail::hit (0.85f);
    p[12] = detail::hit (0.90f);
    return p;
}();

/*  Contratiempos con semicorcheas fantasma detras. Los fantasmas van con poca
    velocity y probabilidad parcial: la gracia del skippy es que no sea igual
    en cada vuelta. */
inline constexpr Pattern kSkippy16 = []
{
    Pattern p {};
    p[2]  = detail::hit (0.95f);
    p[3]  = detail::hit (0.50f, 0.80f);
    p[6]  = detail::hit (0.95f);
    p[7]  = detail::hit (0.45f, 0.50f);
    p[10] = detail::hit (0.95f);
    p[11] = detail::hit (0.50f, 0.80f);
    p[13] = detail::hit (0.40f, 0.35f);
    p[14] = detail::hit (0.95f);
    return p;
}();

/*  Techno con aire: pocos golpes, fuera de lugar, y uno que aparece a veces. */
inline constexpr Pattern kSparseTechno = []
{
    Pattern p {};
    p[3]  = detail::hit (1.00f);
    p[10] = detail::hit (0.80f, 0.70f);
    p[14] = detail::hit (0.60f, 0.35f);
    return p;
}();

/*  Patron de fabrica por id. 'custom' no tiene tabla: lo resuelve el llamador. */
inline const Pattern& factoryPattern (PatternId id) noexcept
{
    switch (id)
    {
        case PatternId::tresillo332:  return kTresillo332;
        case PatternId::skippy16:     return kSkippy16;
        case PatternId::sparseTechno: return kSparseTechno;
        case PatternId::offbeat8:
        case PatternId::custom:
        case PatternId::numPatterns:
        default:                      return kOffbeat8;
    }
}
}
