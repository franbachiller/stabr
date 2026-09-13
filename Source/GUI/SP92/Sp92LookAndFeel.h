#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Finish.h"

namespace stabr::gui::sp92
{
/*  Perillas y teclas de goma del SP-92.

    Propiedades que leen los dibujos (en getProperties() del componente):
      Slider:     "sp92Knob" = "big" | "small" | "switch" | "encoder", "ticks" = int
      TextButton: "sp92Dark" = bool (tecla negra), "sp92Led" = bool,
                  "sp92Lit" = bool (LED encendido sin toggle), "sp92Arrow" = "left" | "right",
                  "sp92Play" = bool (triangulo antes del texto)
*/
class Sp92LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    Sp92LookAndFeel();

    void setFinish (const Finish& f) noexcept { fin = &f; }
    const Finish& getFinish() const noexcept { return *fin; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getPopupMenuFont() override;

    /*  Dibujo de una perilla, compartido con componentes que no son Slider. */
    static void paintKnob (juce::Graphics&, juce::Rectangle<float> area, float angle,
                           const juce::String& style, int ticks, const Finish&, bool enabled, bool focused);

private:
    const Finish* fin = &finish (0);
};

/*  El acabado activo, leido del LookAndFeel del componente. */
inline const Finish& currentFinish (juce::Component& c)
{
    if (auto* lf = dynamic_cast<Sp92LookAndFeel*> (&c.getLookAndFeel()))
        return lf->getFinish();

    return finish (0);
}
}
