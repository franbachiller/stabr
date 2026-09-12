#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace stabr
{
/*  Oscilador de tabla band-limited para la forma tipo organo / M1.

    POR QUE NO POLYBLEP (ver DECISIONS.md D-004): esta forma es una suma fija de
    armonicos, no una discontinuidad. PolyBLEP corrige saltos; aca no hay
    saltos que corregir. Hay que sintetizarla aditivamente.

    El clasico stab de house de piano-organo vive en estos armonicos: la
    fundamental con la octava y la doceava fuertes, que es lo que le da el
    color hueco de organo de barras.

    MIPMAP: una tabla por cantidad de armonicos. En notas agudas los armonicos
    altos pasarian Nyquist y aliasarian, asi que se elige la tabla que tenga
    solo los que entran. Con 8 armonicos alcanzan 8 tablas.

    Las tablas se generan UNA sola vez al arrancar (static local, inicializacion
    thread-safe garantizada por el estandar desde C++11), son const y se
    comparten entre todas las voces. Nunca en prepareToPlay ni en el hilo de
    audio. Costo: 8 x 2048 x 4 bytes = 64 KB.
*/
class OrganTable
{
public:
    static constexpr int kTableSize     = 2048;
    static constexpr int kMaxHarmonics  = 8;

    /*  Amplitudes de los armonicos 1..8. Perfil de organo de barras: la
        fundamental manda, la octava (2) y la doceava (3) le dan el cuerpo
        hueco, y los pares altos el brillo del ataque. */
    static constexpr std::array<float, kMaxHarmonics> kHarmonicAmplitudes
    {
        1.00f,   // 1  fundamental
        0.55f,   // 2  octava
        0.35f,   // 3  doceava
        0.28f,   // 4  doble octava
        0.10f,   // 5
        0.18f,   // 6
        0.06f,   // 7
        0.12f    // 8
    };

    using Table = std::array<float, (size_t) kTableSize>;
    using TableSet = std::array<Table, (size_t) kMaxHarmonics>;

    /*  tables[k] contiene los armonicos 1..(kMaxHarmonics - k).
        O sea: tables[0] es la forma completa, tables[7] una senoidal pura. */
    static const TableSet& getTables()
    {
        static const TableSet tables = buildTables();
        return tables;
    }

    /*  Indice de la tabla mas rica cuyos armonicos entran todos bajo Nyquist. */
    static int chooseTableIndex (float frequency, double sampleRate) noexcept
    {
        if (frequency <= 0.0f)
            return 0;

        const auto nyquist = (float) (sampleRate * 0.5);
        const int fitting = (int) std::floor (nyquist / frequency);
        const int allowed = std::clamp (fitting, 1, kMaxHarmonics);

        return kMaxHarmonics - allowed;
    }

private:
    static TableSet buildTables()
    {
        TableSet set {};

        for (int k = 0; k < kMaxHarmonics; ++k)
        {
            const int numHarmonics = kMaxHarmonics - k;
            auto& table = set[(size_t) k];

            float peak = 0.0f;

            for (int i = 0; i < kTableSize; ++i)
            {
                const double t = (double) i / (double) kTableSize;
                double sum = 0.0;

                for (int h = 1; h <= numHarmonics; ++h)
                    sum += (double) kHarmonicAmplitudes[(size_t) (h - 1)]
                             * std::sin (2.0 * 3.14159265358979323846 * h * t);

                table[(size_t) i] = (float) sum;
                peak = std::max (peak, std::abs (table[(size_t) i]));
            }

            // Normalizacion por tabla: sin esto, el volumen saltaria al cruzar
            // de una tabla a otra subiendo por el teclado.
            if (peak > 0.0f)
                for (auto& s : table)
                    s /= peak;
        }

        return set;
    }
};

//==============================================================================
/*  Lector de la tabla. El estado es solo la fase: las tablas son compartidas. */
class OrganOsc
{
public:
    void setSampleRate (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        setFrequency (frequency);
    }

    void setFrequency (float hz) noexcept
    {
        frequency  = std::clamp (hz, 0.0f, (float) (sampleRate * 0.5) - 1.0f);
        phaseInc   = (float) (frequency / sampleRate);
        tableIndex = pinnedIndex >= 0 ? pinnedIndex
                                      : OrganTable::chooseTableIndex (frequency, sampleRate);
    }

    /*  Fija una tabla concreta en vez de elegirla por frecuencia.

        Se usa para la senoidal del sub: la ultima tabla del mipmap tiene un
        solo armonico, o sea que YA es una senoidal pura y band-limited por
        construccion. Reusarla evita tener que llamar a std::sin por muestra en
        cada voz, que con 70 voces no es gratis.
    */
    void pinTableIndex (int index) noexcept
    {
        pinnedIndex = std::clamp (index, -1, OrganTable::kMaxHarmonics - 1);
        setFrequency (frequency);
    }

    void unpinTable() noexcept
    {
        pinnedIndex = -1;
        setFrequency (frequency);
    }

    void reset (float phase01 = 0.0f) noexcept
    {
        phase = phase01 - std::floor (phase01);
    }

    inline float process() noexcept
    {
        const auto& table = OrganTable::getTables()[(size_t) tableIndex];

        const float pos   = phase * (float) OrganTable::kTableSize;
        const int   index = (int) pos;
        const float frac  = pos - (float) index;

        const int i0 = index & (OrganTable::kTableSize - 1);
        const int i1 = (index + 1) & (OrganTable::kTableSize - 1);

        const float value = table[(size_t) i0]
                          + frac * (table[(size_t) i1] - table[(size_t) i0]);

        phase += phaseInc;
        if (phase >= 1.0f)
            phase -= 1.0f;

        return value;
    }

private:
    double sampleRate = 44100.0;
    float frequency   = 440.0f;
    float phaseInc    = 0.0f;
    float phase       = 0.0f;
    int   tableIndex  = 0;
    int   pinnedIndex = -1;   // -1 = elegir por frecuencia
};
}
