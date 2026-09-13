#pragma once

#include <juce_graphics/juce_graphics.h>
#include <cmath>

/*  Movimiento del SP-92.

    Todo depende del tiempo real transcurrido (dt), nunca de "un paso por
    cuadro": el aparato se mueve igual en un monitor de 60 Hz que en uno de
    144 Hz. Las constantes de tiempo salen del mockup aprobado (las transiciones
    CSS de 60 ms, 120 ms, 280 ms) y estan escritas como tau: una aproximacion
    exponencial llega al 95 % en 3 tau.
*/
namespace stabr::gui::sp92::anim
{
/*  Se acerca a target con constante de tiempo tau (segundos). */
inline float approach (float current, float target, float dt, float tau) noexcept
{
    if (tau <= 0.0f || dt >= tau * 8.0f)
        return target;

    const float next = current + (target - current) * (1.0f - std::exp (-dt / tau));
    return std::abs (next - target) < 0.002f ? target : next;
}

/*  Idem con tiempos distintos para subir y bajar (un LED prende rapido y se
    apaga con cola). */
inline float approach (float current, float target, float dt, float tauUp, float tauDown) noexcept
{
    return approach (current, target, dt, target > current ? tauUp : tauDown);
}

inline float easeOutCubic (float t) noexcept   { t = juce::jlimit (0.0f, 1.0f, t); return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }
inline float easeInOutCubic (float t) noexcept { t = juce::jlimit (0.0f, 1.0f, t); return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) * 0.5f; }

/*  cubic-bezier() de CSS: devuelve el avance para un tiempo t en 0..1. */
inline float cubicBezier (float x1, float y1, float x2, float y2, float t) noexcept
{
    t = juce::jlimit (0.0f, 1.0f, t);

    auto bez = [] (float a, float b, float s) { const float u = 1.0f - s; return 3.0f * u * u * s * a + 3.0f * u * s * s * b + s * s * s; };
    auto dBez = [] (float a, float b, float s) { const float u = 1.0f - s; return 3.0f * u * u * a + 6.0f * u * s * (b - a) + 3.0f * s * s * (1.0f - b); };

    float s = t;
    for (int i = 0; i < 6; ++i)
    {
        const float d = dBez (x1, x2, s);
        if (std::abs (d) < 1.0e-5f) break;
        s = juce::jlimit (0.0f, 1.0f, s - (bez (x1, x2, s) - t) / d);
    }

    return bez (y1, y2, s);
}

/*  Resorte amortiguado: para lo que es de goma o de metal y rebota apenas. */
struct Spring
{
    float pos = 0.0f, vel = 0.0f;

    void step (float target, float dt, float hz, float damping) noexcept
    {
        const float w = juce::MathConstants<float>::twoPi * hz;
        int n = juce::jlimit (1, 64, (int) std::ceil (dt / 0.002f));
        const float h = dt / (float) n;

        while (--n >= 0)
        {
            vel += (w * w * (target - pos) - 2.0f * damping * w * vel) * h;
            pos += vel * h;
        }
    }

    bool settled (float target) const noexcept { return std::abs (pos - target) < 0.001f && std::abs (vel) < 0.01f; }
    void snap (float target) noexcept          { pos = target; vel = 0.0f; }
};

//==============================================================================
/*  Sombra difusa al estilo box-shadow: capas concentricas cuya suma imita el
    desenfoque. Mucho mas barato que desenfocar una imagen en cada cuadro. */
inline void blurredRoundedRect (juce::Graphics& g, juce::Rectangle<float> r, float radius,
                                juce::Colour colour, float blur, float spread = 0.0f)
{
    if (colour.getAlpha() == 0)
        return;

    r = r.expanded (spread);

    if (blur <= 0.5f)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (r, radius);
        return;
    }

    constexpr int layers = 7;
    const float alpha = colour.getFloatAlpha();

    for (int i = 0; i < layers; ++i)
    {
        // De afuera hacia adentro: la capa i cubre desde +blur/2 hasta -blur/2.
        const float k = (float) i / (float) (layers - 1);
        const float grow = blur * (0.5f - k);
        g.setColour (colour.withAlpha (alpha / (float) layers * 1.15f));
        g.fillRoundedRectangle (r.expanded (grow), juce::jmax (0.0f, radius + grow));
    }
}

inline void blurredEllipse (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float blur)
{
    if (colour.getAlpha() == 0)
        return;

    constexpr int layers = 6;
    const float alpha = colour.getFloatAlpha();

    for (int i = 0; i < layers; ++i)
    {
        const float k = (float) i / (float) (layers - 1);
        g.setColour (colour.withAlpha (alpha / (float) layers * 1.15f));
        g.fillEllipse (r.expanded (blur * (0.5f - k)));
    }
}

/*  Resplandor de un LED: el punto mas un halo que se desvanece. */
inline void ledGlow (juce::Graphics& g, juce::Rectangle<float> led, juce::Colour colour, float amount, float blur)
{
    if (amount <= 0.001f)
        return;

    blurredEllipse (g, led.expanded (blur * 0.5f), colour.withMultipliedAlpha (0.55f * amount), blur);
}
}
