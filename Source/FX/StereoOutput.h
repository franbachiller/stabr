#pragma once

#include "FxUtils.h"
#include <atomic>
#include <cmath>
#include <algorithm>

namespace stabr::fx
{
struct StereoOutputParams
{
    float width      = 1.0f;      // 0 = mono, 1 = original, 2 = ensanchado
    float monoBelowHz = 120.0f;   // por debajo de esto, todo al centro
    bool  limiterOn  = true;
    float outputGainDb = 0.0f;
};

//==============================================================================
/*  Etapa de salida: ancho M/S, graves en mono y limitador.

    MONO POR DEBAJO DE UNA FRECUENCIA — esto no es un extra, es requisito de
    club. Los graves en estereo se cancelan parcialmente al sumarse a mono, y
    un sistema grande reproduce los graves por subs que son mono. Un stab con
    graves fuera de fase suena enorme en auriculares y desaparece en la pista.

    COMO SE HACE, Y COMO NO: la primera version partia la senal en dos bandas
    con un pasabajos, sumaba la grave a mono y recombinaba. Esta MAL. Un filtro
    de un polo desfasa, asi que "lo que queda" al restar (x - LP(x)) NO es el
    complemento limpio: el test midio que a 50 Hz, con el corte en 120 Hz,
    quedaba el 38% de la senal fuera de la banda monoficada.

    La forma correcta es no tocar la senal principal y filtrar SOLO EL LATERAL:

        mid  = (L + R) / 2        <- pasa entero, sin filtrar
        side = (L - R) / 2        <- se le sacan los graves

    El mid no atraviesa ningun filtro, asi que no hay problema de fase en el
    contenido mono. Lo unico que se filtra es la diferencia entre canales, y
    quitarle graves a eso es exactamente lo que se busca. Es como lo hacen las
    herramientas de mastering, y por este motivo.
*/
class StereoOutput
{
public:
    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;

        for (auto* f : { &sideHp1, &sideHp2 })
            f->setSampleRate (sampleRate);

        // Release del limitador: 60 ms. Mas rapido bombea, mas lento come
        // transitorios del stab siguiente.
        limiterRelease = 1.0f - std::exp (-1.0f / (float) (0.060 * sampleRate));

        reset();
    }

    void reset() noexcept
    {
        sideHp1.reset();
        sideHp2.reset();
        limiterGain = 1.0f;
        peakL.store (0.0f);
        peakR.store (0.0f);
        gainReduction.store (0.0f);
    }

    void setParams (const StereoOutputParams& p) noexcept
    {
        params = p;
        sideHp1.setCutoff (p.monoBelowHz);
        sideHp2.setCutoff (p.monoBelowHz);
        outputGain = std::pow (10.0f, p.outputGainDb / 20.0f);
    }

    inline void process (float& l, float& r) noexcept
    {
        //----------------------------------------------------------------
        // M/S. El mid pasa entero; al side se le sacan los graves.
        //----------------------------------------------------------------
        const float mid  = (l + r) * 0.5f;
        float side = (l - r) * 0.5f;

        // Pasaaltos de 12 dB/oct sobre el lateral: dos polos en cascada. Con un
        // solo polo, a media octava por debajo del corte todavia pasa mucho.
        side = sideHp1.processHighpass (side);
        side = sideHp2.processHighpass (side);

        side *= std::clamp (params.width, 0.0f, 2.0f);

        l = mid + side;
        r = mid - side;

        l *= outputGain;
        r *= outputGain;

        //----------------------------------------------------------------
        // Limitador suave.
        //----------------------------------------------------------------
        if (params.limiterOn)
        {
            const float peak = std::max (std::abs (l), std::abs (r));

            /*  Ataque instantaneo, release suave. Un limitador de salida tiene
                que agarrar el pico en la muestra en que ocurre: si tarda,
                deja pasar justamente lo que tiene que frenar. */
            const float wanted = peak > kCeiling ? kCeiling / peak : 1.0f;

            if (wanted < limiterGain)
                limiterGain = wanted;                       // ataque inmediato
            else
                limiterGain += (wanted - limiterGain) * limiterRelease;

            l *= limiterGain;
            r *= limiterGain;

            gainReduction.store (1.0f - limiterGain);
        }

        //----------------------------------------------------------------
        // Medidores, para la GUI de la Fase 6.
        //----------------------------------------------------------------
        peakL.store (std::max (peakL.load(), std::abs (l)));
        peakR.store (std::max (peakR.load(), std::abs (r)));
    }

    /*  Lee y resetea los picos. La GUI los consume; el hilo de audio solo
        escribe. Son atomicos, asi que no hace falta lock. */
    void readAndClearMeters (float& outL, float& outR, float& outReduction) noexcept
    {
        outL = peakL.exchange (0.0f);
        outR = peakR.exchange (0.0f);
        outReduction = gainReduction.load();
    }

private:
    static constexpr float kCeiling = 0.99f;

    StereoOutputParams params;
    OnePoleTpt sideHp1, sideHp2;

    double sampleRate = 44100.0;
    float outputGain = 1.0f;
    float limiterGain = 1.0f;
    float limiterRelease = 0.001f;

    std::atomic<float> peakL { 0.0f }, peakR { 0.0f }, gainReduction { 0.0f };
};
}
