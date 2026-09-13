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

    Los puntos apagados van a una imagen cacheada; los encendidos, agrupados en
    seis niveles de brillo. En los acabados con brillo se pinta antes un halo.
*/
class DotMatrix final : public juce::Component
{
public:
    DotMatrix (int dotsWide, int dotsHigh);

    void setFinish (const Finish&);

    void clear() noexcept;
    void set (int x, int y) noexcept;
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

    void paint (juce::Graphics&) override;
    void resized() override { unlitCache = {}; }

    static constexpr float kOnTau = 0.012f, kOffTau = 0.040f;

private:
    const int W, H;
    std::vector<std::uint8_t> fb, last;
    std::vector<float> level;
    bool settling = true;
    const Finish* fin = nullptr;

    juce::Image unlitCache;
    juce::Colour unlitColour;
    float unlitScale = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DotMatrix)
};
}
