#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace stabr::gui::sp92
{
/*  Perilla del panel.

    Dos cosas que un Slider comun no hace:
      - GIRA SOLA hasta el valor nuevo cuando el sonido cambia desde afuera
        (Create STAB, un disco, Undo). El audio ya usa el valor nuevo: la
        animacion es solo visual.
      - Distingue el cambio del usuario del de la automatizacion, para mostrar
        el valor en el LCD solo cuando la esta tocando alguien.
*/
class HardwareKnob final : public juce::Slider
{
public:
    HardwareKnob (const juce::String& style, int ticks, const juce::String& title);

    std::function<void (HardwareKnob&)> onUserChange;

    void animateToCurrentValue (double now);
    void tick (double now);
    bool isAnimating (double now) const noexcept { return now - animStart < kAnimSeconds; }

    void paint (juce::Graphics&) override;
    void valueChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

    static constexpr double kAnimSeconds = 0.45;

private:
    double shownPos = -1.0;
    double animFrom = 0.0;
    double animStart = -10.0;
    double lastNow = 0.0;
    bool touching = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardwareKnob)
};
}
