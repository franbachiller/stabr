#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace stabr::gui::sp92
{
/*  Perilla del panel.

    Lo que un Slider comun no hace:
      - GIRA SOLA hasta el valor nuevo cuando el sonido cambia desde afuera
        (Create STAB, un disco, Undo). El audio ya usa el valor nuevo: la
        animacion es solo visual.
      - Arrastrar es directo; los selectores caen en cada posicion con un
        movimiento corto, y la rueda o el teclado giran suave en vez de saltar.
      - Shift mientras se arrastra: ajuste fino (700 px el recorrido, como la
        maqueta).
      - Distingue el cambio del usuario del de la automatizacion, para mostrar
        el valor en el LCD solo cuando la esta tocando alguien.
*/
class HardwareKnob final : public juce::Slider
{
public:
    HardwareKnob (const juce::String& style, int ticks, const juce::String& title);

    std::function<void (HardwareKnob&)> onUserChange;

    /*  Gira desde lo que se ve hasta el valor actual; delay escalona el panel. */
    void animateToCurrentValue (double now, double delay = 0.0);
    void tick (double now);
    bool isAnimating (double now) const noexcept { return now < animStart + animDur; }
    double getShownProportion() const noexcept   { return shownPos; }

    void paint (juce::Graphics&) override;
    void valueChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;
    void enablementChanged() override;

    static constexpr double kPresetSeconds = 0.45;   // la maqueta: .45 s ease-out cubico
    static constexpr double kDetentSeconds = 0.07;
    static constexpr double kStepSeconds   = 0.10;
    static constexpr double kJumpSeconds   = 0.16;
    static constexpr int    kDragPixels    = 190;
    static constexpr int    kFineDragPixels = 700;

private:
    void startAnimation (double start, double duration);
    double positionAt (double now);
    bool isDiscrete() const;

    double shownPos = -1.0;
    double animFrom = 0.0, animStart = -10.0, animDur = 0.0;
    double lastNow = 0.0, lastNorm = -1.0;
    bool touching = false, dragging = false;
    juce::Point<float> lastDragPos, virtualPos;
    float wheelAccum = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardwareKnob)
};
}
