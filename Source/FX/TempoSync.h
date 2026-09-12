#pragma once

#include <array>
#include <algorithm>

namespace stabr::fx
{
/*  Divisiones musicales para todo lo que se sincroniza al host.

    EL ORDEN ES ESTRICTAMENTE CRECIENTE EN DURACION, y hay un test que lo
    verifica. Es lo que hace que girar el control se sienta bien: si el orden
    saltara, subir el knob a veces acortaria el delay.

    Y no es obvio: los tresillos y los punteados se INTERCALAN de forma que no
    coincide con agruparlos por division. La primera version de esta tabla los
    agrupaba (1/16T, 1/16, 1/16., despues 1/8T...) y quedaba desordenada, porque
    1/8T (0.333 negras) es MAS CORTO que 1/16. (0.375 negras). Lo encontro el
    test, no la lectura.

    En negras:
      1/64  0.0625 | 1/32T 0.083 | 1/32  0.125 | 1/16T 0.167 | 1/16 0.25
      1/8T  0.333  | 1/16. 0.375 | 1/8   0.5   | 1/4T  0.667 | 1/8. 0.75
      1/4   1.0    | 1/2T  1.333 | 1/4.  1.5   | 1/2   2.0   | 1/2. 3.0
      1 bar 4.0    | 2 bars 8.0  | 3 bars 12.0 | 4 bars 16.0

    1/8. es 3/16: EL tiempo de delay del dub techno.
*/
enum class Division
{
    n1_64 = 0,
    n1_32T,
    n1_32,
    n1_16T,
    n1_16,
    n1_8T,
    n1_16D,
    n1_8,
    n1_4T,
    n1_8D,      // = 3/16
    n1_4,
    n1_2T,
    n1_4D,
    n1_2,
    n1_2D,
    bar1, bar2, bar3, bar4,
    numDivisions
};

/*  Multiplicador respecto de una NEGRA. Una negra = 1 pulso = 60/bpm segundos.
    Punteado = x1.5, tresillo = x2/3. */
inline constexpr std::array<float, (size_t) Division::numDivisions> divisionInQuarters
{
    0.0625f,                 // 1/64
    0.125f * 2.0f / 3.0f,    // 1/32T
    0.125f,                  // 1/32
    0.25f  * 2.0f / 3.0f,    // 1/16T
    0.25f,                   // 1/16
    0.5f   * 2.0f / 3.0f,    // 1/8T
    0.25f  * 1.5f,           // 1/16.
    0.5f,                    // 1/8
    1.0f   * 2.0f / 3.0f,    // 1/4T
    0.5f   * 1.5f,           // 1/8.  = 3/16
    1.0f,                    // 1/4
    2.0f   * 2.0f / 3.0f,    // 1/2T
    1.0f   * 1.5f,           // 1/4.
    2.0f,                    // 1/2
    2.0f   * 1.5f,           // 1/2.
    4.0f, 8.0f, 12.0f, 16.0f // 1, 2, 3 y 4 compases
};

inline constexpr std::array<const char*, (size_t) Division::numDivisions> divisionNames
{
    "1/64", "1/32T", "1/32", "1/16T", "1/16",
    "1/8T", "1/16.", "1/8", "1/4T", "1/8.",
    "1/4",  "1/2T",  "1/4.", "1/2",  "1/2.",
    "1 bar", "2 bars", "3 bars", "4 bars"
};

//==============================================================================
/*  Duracion de una division en segundos.

    El BPM se clampea a un rango sensato: un host puede reportar 0 mientras no
    hay transporte, y dividir por eso da infinito. */
inline float divisionToSeconds (Division d, double bpm) noexcept
{
    const auto safeBpm = std::clamp (bpm, 20.0, 999.0);
    const auto secondsPerQuarter = 60.0 / safeBpm;
    const auto index = std::clamp ((int) d, 0, (int) Division::numDivisions - 1);

    return (float) (secondsPerQuarter * (double) divisionInQuarters[(size_t) index]);
}

/*  Cuantos pulsos (negras) dura una division. Se usa para alinear el pump con
    la posicion del transporte, que el host reporta en negras. */
inline double divisionInBeats (Division d) noexcept
{
    const auto index = std::clamp ((int) d, 0, (int) Division::numDivisions - 1);
    return (double) divisionInQuarters[(size_t) index];
}
}
