#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <vector>

#include "Finish.h"

namespace stabr::gui::sp92
{
/*  Glifo 5x7 de la pantalla: 7 filas, 5 bits por fila (bit 4 = columna
    izquierda). Nullptr si el caracter no existe. */
const std::uint8_t* dotGlyph (juce::juce_wchar c) noexcept;

//==============================================================================
/*  Pantalla de matriz de puntos retroiluminada.

    Se dibuja en un framebuffer de puntos. commit() lo compara con lo anterior
    y cada punto va hacia su estado con la inercia de un cristal liquido real:
    prende en ~35 ms y se apaga en ~120 ms. Una pantalla quieta no repinta.

    Render (D-051): los puntos son cuadrados y se rasterizan a mano sobre los
    pixeles fisicos, calculando la cobertura exacta de cada punto en cada pixel.
    Asi todos los puntos son identicos a cualquier escala: nada de redondeos
    del renderizador ni moire entre puntos prendidos y apagados. Encima va el
    brillo: un resplandor gaussiano en las pantallas emisivas (ambar, azul) y la
    sombra que cada punto proyecta sobre la retroiluminacion en el LCD verde.
    Solo se recalcula la zona que cambio.
*/
class DotMatrix final : public juce::Component
{
public:
    DotMatrix (int dotsWide, int dotsHigh, juce::BorderSize<int> glassInsets = { 8, 10, 8, 10 });

    void setFinish (const Finish&);

    void clear() noexcept;
    void set (int x, int y) noexcept;
    void unset (int x, int y) noexcept;
    void block (int x, int y, int size) noexcept;
    void hline (int x0, int x1, int y, bool dotted = false) noexcept;
    void vline (int x, int y0, int y1, bool dotted = false) noexcept;

    int  text (const juce::String&, int x, int y, int scale = 1) noexcept;
    int  textWidth (const juce::String&, int scale = 1) const noexcept;
    void textRight (const juce::String&, int xRight, int y, int scale = 1) noexcept;
    void textCenter (const juce::String&, int xCentre, int y, int scale = 1) noexcept;

    /*  Avanza la inercia dt segundos y repinta si algo se movio. dt grande
        (o el valor por defecto) salta directo al estado final. */
    void commit (float dt = 1.0f);

    bool isLit (int x, int y) const noexcept;
    float getLevel (int x, int y) const noexcept;
    int getDotsWide() const noexcept { return W; }
    int getDotsHigh() const noexcept { return H; }

    /*  Area de la grilla y de un punto encendido, en coordenadas locales. */
    juce::Rectangle<float> getGridArea() const;
    juce::Rectangle<float> getDotRect (int x, int y) const;

    /*  La capa de puntos del ultimo paint, pixel a pixel (para tests). */
    const juce::Image& getDotLayer() const noexcept { return layer; }

    void paint (juce::Graphics&) override;
    void resized() override { geomScale = 0.0f; }

    static constexpr float kOnTau = 0.012f, kOffTau = 0.040f;
    static constexpr float kLitSize = 0.88f, kUnlitSize = 0.82f;

private:
    struct Span
    {
        int first = 0;
        std::vector<float> weights;
    };

    void rebuildGeometry (float scale);
    void renderRegion (juce::Rectangle<int> dots);

    const int W, H;
    const juce::BorderSize<int> insets;
    std::vector<std::uint8_t> fb, last;
    std::vector<float> level;
    bool settling = true;
    const Finish* fin = nullptr;

    // Geometria a la escala fisica actual.
    float geomScale = 0.0f;
    int geomW = 0, geomH = 0, PW = 0, PH = 0;
    std::vector<Span> litCols, litRows, offCols, offRows;
    std::vector<float> lit, glow, tmp, unlit, kernel;
    int blurRadius = 1, shadowDx = 0, shadowDy = 0;
    juce::Image layer;
    juce::Rectangle<int> dirtyDots;
    bool fullDirty = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DotMatrix)
};
}
