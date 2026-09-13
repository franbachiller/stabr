#pragma once

#include <algorithm>
#include <array>
#include <initializer_list>

#include "ChordTypes.h"

namespace stabr::chord
{
/*  Nombre de un acorde a partir de las notas que SUENAN.

    POR QUE NO SE USA EL PARAMETRO: con scale lock en Diatonic la calidad la
    decide la escala, no el control. Con "Min7" elegido en La menor, la tecla
    Si da Bm7b5 y la tecla Sol da G7. Mostrar "Min7" seria mentirle al usuario
    justo en el componente que existe para explicarle que esta tocando.

    Puro, sin JUCE y sin asignar memoria.
*/
struct ChordName
{
    bool valid = false;
    int root = 0;              // clase de altura 0..11
    int bass = 0;              // clase de altura de la nota mas grave
    const char* quality = "";  // "m7", "maj9", "" (mayor)...
};

namespace naming_detail
{
    struct Quality
    {
        int mask;              // bit i = intervalo de i semitonos (modulo 12)
        const char* name;
        bool fifthOptional;    // se reconoce tambien sin la quinta (D-022 la saca primero)
    };

    constexpr int maskOf (std::initializer_list<int> intervals) noexcept
    {
        int m = 0;

        for (int i : intervals)
            m |= 1 << (i % 12);

        return m;
    }

    /*  De mas notas a menos: ante una igualdad exacta gana la primera, pero
        como se exige que coincidan TODAS las clases de altura, el orden solo
        importa entre calidades que comparten mascara (ninguna). */
    inline constexpr std::array<Quality, 19> qualities
    {{
        { maskOf ({ 0, 3, 7, 10, 2, 5 }), "m11",     true  },
        { maskOf ({ 0, 4, 7, 11, 2 }),    "maj9",    true  },
        { maskOf ({ 0, 3, 7, 10, 2 }),    "m9",      true  },
        { maskOf ({ 0, 4, 7, 10, 2 }),    "9",       true  },
        { maskOf ({ 0, 4, 7, 9, 2 }),     "6/9",     true  },
        { maskOf ({ 0, 3, 6, 10 }),       "m7b5",    false },
        { maskOf ({ 0, 3, 6, 9 }),        "dim7",    false },
        { maskOf ({ 0, 4, 7, 11 }),       "maj7",    true  },
        { maskOf ({ 0, 3, 7, 10 }),       "m7",      true  },
        { maskOf ({ 0, 4, 7, 10 }),       "7",       true  },
        { maskOf ({ 0, 4, 7, 2 }),        "add9",    true  },
        { maskOf ({ 0, 3, 7, 2 }),        "m(add9)", true  },
        { maskOf ({ 0, 4, 7 }),           "",        false },
        { maskOf ({ 0, 3, 7 }),           "m",       false },
        { maskOf ({ 0, 3, 6 }),           "dim",     false },
        { maskOf ({ 0, 4, 8 }),           "aug",     false },
        { maskOf ({ 0, 2, 7 }),           "sus2",    false },
        { maskOf ({ 0, 5, 7 }),           "sus4",    false },
        { maskOf ({ 0, 7 }),              "5",       false },
    }};

    constexpr int rotate (int mask, int root) noexcept
    {
        return ((mask >> root) | (mask << (12 - root))) & 0xFFF;
    }
}

/*  Dos pasadas: primero coincidencia exacta, despues sin la quinta. En cada
    pasada se prueba primero el BAJO como fundamental y despues el resto, asi
    que un acorde en estado fundamental nunca se nombra como inversion de
    otro, y una inversion real sale con barra: Am7/C. */
inline ChordName nameChord (const ChordResult& c) noexcept
{
    using namespace naming_detail;

    ChordName result;

    if (c.count <= 0)
        return result;

    int pcs = 0;
    int lowest = c.notes[0];

    for (int i = 0; i < c.count; ++i)
    {
        const int n = c.notes[(size_t) i];
        pcs |= 1 << (((n % 12) + 12) % 12);
        lowest = std::min (lowest, n);
    }

    const int bass = ((lowest % 12) + 12) % 12;
    result.bass = bass;

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int k = 0; k < 13; ++k)
        {
            const int root = k == 0 ? bass : k - 1;

            if ((k > 0 && root == bass) || (pcs & (1 << root)) == 0)
                continue;

            const int rel = rotate (pcs, root);

            for (const auto& q : qualities)
            {
                if (pass == 1 && ! q.fifthOptional)
                    continue;

                const int target = pass == 0 ? q.mask : (q.mask & ~(1 << 7));

                if (rel == target)
                {
                    result.valid = true;
                    result.root = root;
                    result.quality = q.name;
                    return result;
                }
            }
        }
    }

    return result;
}
}
