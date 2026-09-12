#pragma once

#include "Oversampler2x.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace stabr
{
namespace drive_detail
{
    /*  Aproximacion de Pade a tanh, con la entrada acotada.

        El clamp a +-3 NO es opcional: fuera de ese rango la forma racional
        deja de saturar y vuelve a crecer, o sea que un pico fuerte saldria
        AMPLIFICADO en vez de recortado. Justo en +-3 la expresion da +-1
        exacto, asi que el empalme es continuo.
    */
    inline float shape (float x) noexcept
    {
        const float c = std::clamp (x, -3.0f, 3.0f);
        const float c2 = c * c;
        return c * (27.0f + c2) / (27.0f + 9.0f * c2);
    }

    /*  La curva completa con su offset y su compensacion. Es funcion libre para
        que el test de aliasing pueda medir EXACTAMENTE la misma no linealidad
        con y sin sobremuestreo. Si el test la reimplementara, estaria midiendo
        otra cosa. */
    inline float shapeWithBias (float x, float gain, float bias, float outputGain) noexcept
    {
        return (shape (x * gain + bias) - shape (bias)) * outputGain;
    }

    /*  Offset maximo antes de la curva.

        VALOR ELEGIDO CON DATOS, no a ojo. Con 0.6 (el primer intento) la
        asimetria era brutal: con la entrada a +0.5 la salida daba +1.0 y con
        -0.5 daba -3.37. Eso no es "agregar armonicos pares", es romper la
        forma de onda y meterle un offset enorme a la etapa siguiente. Lo
        encontro el test que verifica que la salida quede acotada.

        0.25 da asimetria audible (el lado positivo llega a ~0.6 cuando el
        negativo llega a 1.0) sin que la salida se salga de rango. */
    inline constexpr float kMaxBias = 0.25f;

    /*  Mapeo de amount 0..1 a los parametros de la curva. Compartido con el
        test por el mismo motivo. */
    struct Settings
    {
        float gain = 1.0f;
        float bias = 0.0f;
        float outputGain = 1.0f;
    };

    inline Settings settingsFor (float amount01) noexcept
    {
        const auto a = std::clamp (amount01, 0.0f, 1.0f);

        Settings s;
        // Hasta x20 de ganancia de entrada: con la curva acotada a +-1, eso da
        // desde transparente hasta saturacion dura.
        s.gain = 1.0f + 19.0f * a * a;
        s.bias = kMaxBias * a;

        /*  Normalizacion por el extremo MAS GRANDE de los dos, no solo por el
            positivo. Con una curva asimetrica los dos extremos son distintos;
            normalizar por uno solo deja al otro fuera de rango. */
        const float centre   = shape (s.bias);
        const float positive = std::abs (shape (s.gain + s.bias) - centre);
        const float negative = std::abs (shape (-s.gain + s.bias) - centre);
        const float extreme  = std::max (positive, negative);

        /*  SOLO se atenua, nunca se amplifica.

            La version anterior normalizaba siempre para que la entrada 1 diera
            salida 1. Pero la curva ya comprime por su cuenta: con el drive en
            CERO eso metia +2.2 dB de ganancia y una compresion leve. O sea que
            el drive apagado no era transparente, y mover el control apenas un
            poco daba un salto de nivel.

            Compensando solo cuando el extremo pasa la unidad, drive = 0 es
            exactamente la identidad y la compensacion aparece recien cuando
            hace falta. Lo encontro el test de salida acotada. */
        s.outputGain = extreme > 1.0f ? 1.0f / extreme : 1.0f;

        return s;
    }
}

/*  Drive por voz, antes del filtro.

    ASIMETRICO a proposito: un saturador simetrico solo genera armonicos
    impares y suena a distorsion "limpia". Sumando un offset antes de la curva
    y restandolo despues aparecen los pares, que es lo que le da cuerpo y
    caracter de valvula al stab cuando se lo empuja.

    OVERSAMPLING (DECISIONS.md D-013): x2 por defecto, x4 con HQ Mode. El brief
    pedia x4 siempre, pero x4 por voz con hasta 56 voces es CPU que no rinde:
    la etapa de up/down cuesta mas que la no linealidad. x4 queda para render
    offline, donde la CPU no importa.

    BYPASS CON RETARDO COMPENSADO: con el drive en cero no tiene sentido pagar
    el sobremuestreo, pero apagarlo a secas adelantaria la voz 11 muestras y
    eso SI hace un salto. La solucion es pasar la senal por una linea de
    retardo puro de exactamente esas 11 muestras (medidas, no estimadas: ver el
    test de round trip del sobremuestreador). Asi los dos caminos tienen el
    mismo retardo y, como con amount=0 la curva es la identidad, cruzar de uno
    al otro es inaudible: los dos difieren en -96 dB.

    Sin esto, el drive apagado costaba mas que el filtro y los osciladores
    juntos, para no hacer absolutamente nada.
*/
class Drive
{
public:
    /*  Retardo de grupo del sobremuestreador ida y vuelta, en muestras a tasa
        base. Medido con un round trip, no estimado. */
    static constexpr int kGroupDelaySamples = 11;

    void reset() noexcept
    {
        stage1.reset();
        stage2.reset();
        bypassDelay.fill (0.0f);
        bypassIndex = 0;
    }

    /*  amount en 0..1. */
    void setAmount (float amount01) noexcept
    {
        settings = drive_detail::settingsFor (amount01);
        bypassed = amount01 <= 0.0001f;
    }

    bool isBypassed() const noexcept { return bypassed; }

    void setHighQuality (bool shouldUseFourTimes) noexcept
    {
        highQuality = shouldUseFourTimes;
    }

    inline float process (float x) noexcept
    {
        if (bypassed)
        {
            // Solo retardo: mismo group delay que el camino con sobremuestreo,
            // para que encender y apagar el drive no salte.
            const float out = bypassDelay[(size_t) bypassIndex];
            bypassDelay[(size_t) bypassIndex] = x;
            bypassIndex = (bypassIndex + 1) % kGroupDelaySamples;
            return out;
        }

        float a = 0.0f, b = 0.0f;
        stage1.upsample (x, a, b);

        if (highQuality)
        {
            a = processAtFourTimes (a);
            b = processAtFourTimes (b);
        }
        else
        {
            a = shapeWithBias (a);
            b = shapeWithBias (b);
        }

        return stage1.downsample (a, b);
    }

private:
    /*  Segunda etapa de x2 sobre la senal que ya viene al doble de tasa.
        Una sola instancia usada secuencialmente: las dos muestras son el mismo
        flujo temporal, no dos canales. */
    inline float processAtFourTimes (float x) noexcept
    {
        float a = 0.0f, b = 0.0f;
        stage2.upsample (x, a, b);

        a = shapeWithBias (a);
        b = shapeWithBias (b);

        return stage2.downsample (a, b);
    }

    inline float shapeWithBias (float x) noexcept
    {
        return drive_detail::shapeWithBias (x, settings.gain, settings.bias,
                                            settings.outputGain);
    }

    Oversampler2x stage1, stage2;
    drive_detail::Settings settings;
    std::array<float, (size_t) kGroupDelaySamples> bypassDelay {};
    int bypassIndex = 0;
    bool bypassed = true;
    bool highQuality = false;
};
}
