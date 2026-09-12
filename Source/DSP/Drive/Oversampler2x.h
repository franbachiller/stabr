#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace stabr
{
/*  Sobremuestreo x2 con filtro halfband FIR.

    POR QUE PROPIO Y NO juce::dsp::Oversampling: hace falta UNA instancia por
    voz y por canal (el estado del filtro tiene que ser continuo por voz), o sea
    mas de cien instancias. juce::dsp::Oversampling reserva buffers de bloque
    por instancia; aca el estado es una linea de retardo de 12 floats.

    POR QUE FIR Y NO IIR POLIFASICO: las dos son opciones validas, pero los
    coeficientes de un IIR polifasico son numeros magicos de una tabla que hay
    que tener bien. Los de un FIR halfband se GENERAN por sinc enventanado, o
    sea que son verificables: hay un test que mide la atenuacion real en la
    banda de rechazo. DSP delicado se escribe y despues se mide, no se confia.

    ESTRUCTURA HALFBAND: un filtro halfband de largo 4k+3 tiene la mitad de los
    coeficientes en cero. Aprovechandolo, cada direccion se reduce a un FIR
    corto en una rama polifasica y un retardo puro con ganancia 1/2 en la otra.
    Sin eso, el costo seria el doble.
*/
class HalfbandKernel
{
public:
    static constexpr int kLength   = 23;                    // 4k+3, centro impar
    static constexpr int kCentre   = (kLength - 1) / 2;     // 11
    static constexpr int kFirTaps  = (kLength + 1) / 2;     // 12 coeficientes pares
    static constexpr int kDelayTap = kCentre / 2;           // 5 muestras de entrada

    /*  Coeficientes de los indices PARES del halfband, h[0], h[2], ... h[22].
        Los impares son cero salvo el centro, que vale 0.5 exacto. */
    static const std::array<float, (size_t) kFirTaps>& evenTaps()
    {
        static const std::array<float, (size_t) kFirTaps> taps = build();
        return taps;
    }

    /*  Rango de coeficientes que NO son cero.

        La ventana de Blackman vale exactamente 0 en los extremos, asi que los
        coeficientes de las puntas tambien. Multiplicarlos seria tirar el 17%
        del trabajo del FIR, que es el grueso del costo del sobremuestreo.

        Se DERIVAN escaneando la tabla, no se hardcodean: si alguna vez se
        cambia la ventana o el largo, esto se ajusta solo. */
    struct NonZeroRange { int first = 0; int last = kFirTaps - 1; };

    static const NonZeroRange& nonZeroRange()
    {
        static const NonZeroRange range = []
        {
            const auto& taps = evenTaps();
            NonZeroRange r;

            /*  Umbral, no comparacion exacta con cero: la ventana de Blackman
                deja los extremos en ~1e-18, no en 0.0f exacto. Comparar con
                == 0.0f (el primer intento) no descartaba nada y la
                optimizacion no hacia efecto.

                1e-6 respecto del coeficiente mayor son -120 dB: descartarlos
                no cambia la respuesta de forma medible. */
            constexpr float negligible = 1.0e-6f;

            while (r.first < kFirTaps && std::abs (taps[(size_t) r.first]) < negligible)
                ++r.first;

            while (r.last > r.first && std::abs (taps[(size_t) r.last]) < negligible)
                --r.last;

            return r;
        }();

        return range;
    }

private:
    static std::array<float, (size_t) kFirTaps> build()
    {
        constexpr double pi = 3.14159265358979323846;

        std::array<double, (size_t) kLength> h {};

        for (int n = 0; n < kLength; ++n)
        {
            const double m = (double) (n - kCentre);

            // sinc a fs/4: da exactamente cero en los indices pares desde el
            // centro, que es lo que hace halfband a un halfband.
            const double sinc = (m == 0.0) ? 1.0
                                           : std::sin (pi * 0.5 * m) / (pi * 0.5 * m);

            // Ventana de Blackman.
            const double t = 2.0 * pi * (double) n / (double) (kLength - 1);
            const double window = 0.42 - 0.5 * std::cos (t) + 0.08 * std::cos (2.0 * t);

            h[(size_t) n] = 0.5 * sinc * window;
        }

        // Normalizacion a ganancia unitaria en continua.
        double sum = 0.0;
        for (auto v : h)
            sum += v;

        if (sum != 0.0)
            for (auto& v : h)
                v /= sum;

        std::array<float, (size_t) kFirTaps> taps {};

        for (int j = 0; j < kFirTaps; ++j)
            taps[(size_t) j] = (float) h[(size_t) (j * 2)];

        return taps;
    }
};

//==============================================================================
/*  Sobremuestreador x2 de una sola muestra por vez.

    Estado: dos lineas de retardo cortas. Barato de copiar y de tener por voz.
*/
class Oversampler2x
{
public:
    void reset() noexcept
    {
        upHistory.fill (0.0f);
        downEven.fill (0.0f);
        downOdd.fill (0.0f);
        upIndex = 0;
        downIndex = 0;
    }

    /*  Una muestra de entrada -> dos de salida al doble de tasa. */
    inline void upsample (float x, float& out0, float& out1) noexcept
    {
        const auto& taps = HalfbandKernel::evenTaps();
        const auto& range = HalfbandKernel::nonZeroRange();

        upHistory[(size_t) upIndex] = x;

        // Rama FIR: las muestras de salida de indice par.
        float acc = 0.0f;

        for (int j = range.first; j <= range.last; ++j)
        {
            const int idx = (upIndex - j + HalfbandKernel::kFirTaps)
                              % HalfbandKernel::kFirTaps;
            acc += taps[(size_t) j] * upHistory[(size_t) idx];
        }

        // Rama de retardo puro: las de indice impar. El x2 compensa la ganancia
        // de 1/2 que introduce intercalar ceros.
        const int delayed = (upIndex - HalfbandKernel::kDelayTap
                               + HalfbandKernel::kFirTaps) % HalfbandKernel::kFirTaps;

        out0 = 2.0f * acc;
        out1 = upHistory[(size_t) delayed];

        upIndex = (upIndex + 1) % HalfbandKernel::kFirTaps;
    }

    /*  Dos muestras al doble de tasa -> una a tasa normal. */
    inline float downsample (float in0, float in1) noexcept
    {
        const auto& taps = HalfbandKernel::evenTaps();
        const auto& range = HalfbandKernel::nonZeroRange();

        downEven[(size_t) downIndex] = in0;
        downOdd[(size_t) downIndex]  = in1;

        float acc = 0.0f;

        for (int j = range.first; j <= range.last; ++j)
        {
            const int idx = (downIndex - j + HalfbandKernel::kFirTaps)
                              % HalfbandKernel::kFirTaps;
            acc += taps[(size_t) j] * downEven[(size_t) idx];
        }

        const int oddIdx = (downIndex - (HalfbandKernel::kDelayTap + 1)
                              + HalfbandKernel::kFirTaps) % HalfbandKernel::kFirTaps;

        acc += 0.5f * downOdd[(size_t) oddIdx];

        downIndex = (downIndex + 1) % HalfbandKernel::kFirTaps;
        return acc;
    }

private:
    std::array<float, (size_t) HalfbandKernel::kFirTaps> upHistory {};
    std::array<float, (size_t) HalfbandKernel::kFirTaps> downEven {};
    std::array<float, (size_t) HalfbandKernel::kFirTaps> downOdd {};
    int upIndex = 0;
    int downIndex = 0;
};
}
