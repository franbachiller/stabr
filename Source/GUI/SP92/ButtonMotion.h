#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "Anim.h"

namespace stabr::gui::sp92
{
/*  Repinta el componente y el margen alrededor donde caen su sombra y su
    brillo (los componentes con sombra pintan sin recorte). */
inline void repaintWithMargin (juce::Component& c, int margin)
{
    if (auto* parent = c.getParentComponent())
        parent->repaint (c.getBounds().expanded (margin));
    else
        c.repaint();
}

//==============================================================================
/*  Movimiento de las teclas de goma: hundirse, iluminarse al pasar el mouse y
    el LED que prende rapido y se apaga con cola.

    Deja los valores animados en propiedades del boton ("sp92Press",
    "sp92Hover", "sp92LitAmt") y el LookAndFeel las dibuja. Un clic mas corto
    que un cuadro igual se ve: la tecla queda abajo al menos kMinPress, como
    el .06 s de la maqueta.
*/
class ButtonMotion final : private juce::Button::Listener
{
public:
    ButtonMotion() = default;

    ~ButtonMotion() override
    {
        for (auto& s : states)
            if (s.button != nullptr)
                s.button->removeListener (this);
    }

    void add (juce::Button& b, int shadowMargin = 10)
    {
        b.addListener (this);
        b.setPaintingIsUnclipped (true);
        b.setMouseCursor (juce::MouseCursor::PointingHandCursor);

        State s;
        s.button = &b;
        s.margin = shadowMargin;
        s.lit = isLit (b) ? 1.0f : 0.0f;
        states.push_back (s);
        write (states.back());
    }

    void tick (double now, float dt)
    {
        for (auto& s : states)
        {
            if (s.button == nullptr)
                continue;

            auto& b = *s.button;
            const bool held = b.isDown() || now - s.downAt < kMinPress;

            const float press = anim::approach (s.press, held ? 1.0f : 0.0f, dt, 0.020f, 0.032f);
            const float hover = anim::approach (s.hover, b.isOver() && b.isEnabled() ? 1.0f : 0.0f, dt, 0.040f, 0.090f);
            const float lit   = anim::approach (s.lit, isLit (b) ? 1.0f : 0.0f, dt, 0.025f, 0.080f);

            if (press != s.press || hover != s.hover || lit != s.lit)
            {
                s.press = press;
                s.hover = hover;
                s.lit = lit;
                write (s);
                repaintWithMargin (b, s.margin);
            }
        }
    }

    static constexpr double kMinPress = 0.07;

private:
    struct State
    {
        juce::Component::SafePointer<juce::Button> button;
        int margin = 10;
        float press = 0.0f, hover = 0.0f, lit = 0.0f;
        double downAt = -10.0;
    };

    static bool isLit (juce::Button& b)
    {
        return b.getToggleState() || (bool) b.getProperties().getWithDefault ("sp92Lit", false);
    }

    static void write (State& s)
    {
        auto& props = s.button->getProperties();
        props.set ("sp92Press", s.press);
        props.set ("sp92Hover", s.hover);
        props.set ("sp92LitAmt", s.lit);
    }

    void buttonClicked (juce::Button*) override {}

    void buttonStateChanged (juce::Button* b) override
    {
        if (! b->isDown())
            return;

        for (auto& s : states)
            if (s.button == b)
                s.downAt = juce::Time::getMillisecondCounterHiRes() * 0.001;
    }

    std::vector<State> states;

    JUCE_DECLARE_NON_COPYABLE (ButtonMotion)
};
}
