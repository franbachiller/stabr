#include "Sp92LookAndFeel.h"
#include "Sp92Fonts.h"

namespace stabr::gui::sp92
{
namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    juce::Point<float> onCircle (juce::Point<float> c, float r, float angle)
    {
        // Angulos de JUCE: 0 arriba, sentido horario.
        return { c.x + std::sin (angle) * r, c.y - std::cos (angle) * r };
    }
}

Sp92LookAndFeel::Sp92LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff201d1a));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffece4d4));
    setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xffa99f8e));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffff6b1c));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff17130f));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff201d1a));
    setColour (juce::TooltipWindow::textColourId, juce::Colour (0xffece4d4));
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff3a3127));
}

juce::Font Sp92LookAndFeel::getPopupMenuFont()
{
    return fonts::label (15.0f, false);
}

//==============================================================================
void Sp92LookAndFeel::paintKnob (juce::Graphics& g, juce::Rectangle<float> knob, float angle,
                                 const juce::String& style, int ticks, const Finish& f, bool enabled, bool focused)
{
    const float d = juce::jmin (knob.getWidth(), knob.getHeight());
    knob = knob.withSizeKeepingCentre (d, d);
    const auto c = knob.getCentre();
    const float r = d * 0.5f;

    if (! enabled)
        g.beginTransparencyLayer (0.45f);

    // Marcas serigrafiadas alrededor: 270 grados, la del centro y los extremos mas largas.
    if (ticks > 1)
    {
        const float start = pi * 1.25f, end = pi * 2.75f;

        for (int i = 0; i < ticks; ++i)
        {
            const float a = start + (end - start) * (float) i / (float) (ticks - 1);
            const bool major = i == 0 || i == ticks - 1 || (ticks % 2 == 1 && i == ticks / 2);
            g.setColour (f.silk);
            g.drawLine ({ onCircle (c, r + 4.0f, a), onCircle (c, r + (major ? 10.0f : 8.0f), a) }, major ? 1.6f : 1.1f);
        }
    }

    // Sombra en el panel: varias elipses suaves corridas hacia abajo.
    for (int k = 0; k < 4; ++k)
    {
        g.setColour (juce::Colours::black.withAlpha (0.11f));
        g.fillEllipse (knob.translated (0.0f, 2.0f + (float) k * 1.3f).expanded ((float) k));
    }

    // Cuerpo moleteado.
    {
        const juce::Point<float> light { knob.getX() + d * 0.36f, knob.getY() + d * 0.28f };
        juce::ColourGradient body (juce::Colour (0xff5d5c5a), light.x, light.y, juce::Colour (0xff161514), light.x + d * 0.72f, light.y, true);
        body.addColour (0.62, juce::Colour (0xff242322));
        g.setGradientFill (body);
        g.fillEllipse (knob);

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int i = 0; i < 36; i += 2)
        {
            const float a = (float) i * pi / 18.0f;
            g.drawLine ({ onCircle (c, r * 0.80f, a), onCircle (c, r * 0.97f, a) }, juce::jmax (1.0f, d * 0.018f));
        }

        juce::ColourGradient shade (juce::Colours::transparentBlack, 0.0f, knob.getY() + d * 0.35f,
                                    juce::Colours::black.withAlpha (0.45f), 0.0f, knob.getBottom(), false);
        g.setGradientFill (shade);
        g.fillEllipse (knob);

        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawEllipse (knob.reduced (1.0f), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (knob, 1.0f);
    }

    if (style == "switch")
    {
        // Selector rotativo: tapa oscura y un indicador largo del color del pad STAB.
        const auto inner = knob.reduced (d * 0.30f);
        juce::ColourGradient cap (juce::Colour (0xff4a4a4a), inner.getX() + inner.getWidth() * 0.4f, inner.getY() + inner.getHeight() * 0.35f,
                                  juce::Colour (0xff1b1b1b), inner.getRight(), inner.getBottom(), true);
        g.setGradientFill (cap);
        g.fillEllipse (inner);

        g.setColour (f.stab);
        g.drawLine ({ onCircle (c, r * 0.94f, angle), onCircle (c, r * 0.26f, angle) }, juce::jmax (3.0f, d * 0.07f));
    }
    else if (style == "encoder")
    {
        const auto inner = knob.reduced (d * 0.18f);
        juce::ColourGradient cap (juce::Colour (0xff4b4a48), inner.getX() + inner.getWidth() * 0.4f, inner.getY() + inner.getHeight() * 0.35f,
                                  juce::Colour (0xff1e1d1c), inner.getRight(), inner.getBottom(), true);
        g.setGradientFill (cap);
        g.fillEllipse (inner);

        g.setColour (juce::Colour (0xfff6f3ea));
        g.drawLine ({ onCircle (c, r * 0.92f, angle), onCircle (c, r * 0.66f, angle) }, 2.5f);
    }
    else
    {
        // Tapa de aluminio: degrade radial y cunas alternadas que imitan el cepillado.
        const auto cap = knob.reduced (d * 0.24f);
        const auto cc = cap.getCentre();
        juce::ColourGradient metal (juce::Colour (0xfff3f1ec), cap.getX() + cap.getWidth() * 0.3f, cap.getY() + cap.getHeight() * 0.25f,
                                    juce::Colour (0xff8e8c87), cap.getRight(), cap.getBottom(), true);
        g.setGradientFill (metal);
        g.fillEllipse (cap);

        for (int i = 0; i < 12; ++i)
        {
            juce::Path wedge;
            const float a0 = (float) i * pi / 6.0f + 0.35f;
            wedge.addPieSegment (cap, a0, a0 + pi / 6.0f, 0.0f);
            g.setColour (i % 2 == 0 ? juce::Colours::white.withAlpha (0.10f) : juce::Colours::black.withAlpha (0.07f));
            g.fillPath (wedge);
        }

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (cap, 0.8f);
        juce::ignoreUnused (cc);

        g.setColour (juce::Colour (0xfff6f3ea));
        g.drawLine ({ onCircle (c, r * 0.92f, angle), onCircle (c, r * 0.50f, angle) }, juce::jmax (2.5f, d * 0.036f));
    }

    if (! enabled)
        g.endTransparencyLayer();

    if (focused)
    {
        g.setColour (f.led.withAlpha (0.9f));
        g.drawEllipse (knob.expanded (2.5f), 1.5f);
    }
}

void Sp92LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto style = slider.getProperties().getWithDefault ("sp92Knob", "small").toString();
    const int ticks = (int) slider.getProperties().getWithDefault ("ticks", 11);
    const float margin = (style == "encoder" || ticks <= 1) ? 0.0f : 12.0f;

    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (margin);
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    paintKnob (g, area, angle, style, style == "encoder" ? 0 : ticks, *fin, slider.isEnabled(), slider.hasKeyboardFocus (false));
}

//==============================================================================
void Sp92LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool highlighted, bool down)
{
    const auto& props = button.getProperties();
    const bool dark = (bool) props.getWithDefault ("sp92Dark", false);

    auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    b.removeFromBottom (3.0f);
    const bool pressed = down;
    const float radius = (float) props.getWithDefault ("sp92Radius", 6.0);

    if (! button.isEnabled())
        g.beginTransparencyLayer (0.55f);

    if (! pressed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (b.translated (0.0f, 3.0f), radius);
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (b.translated (0.0f, 5.0f).expanded (1.0f), radius + 1.0f);
    }

    const auto face = pressed ? b.translated (0.0f, 2.0f) : b;
    const auto base = dark ? darkKey::base : fin->key;
    auto hi = dark ? darkKey::hi : fin->keyHi;
    const auto lo = dark ? darkKey::lo : fin->keyLo;

    if (highlighted && ! pressed)
        hi = hi.brighter (0.06f);

    juce::ColourGradient grad (hi, 0.0f, face.getY(), lo, 0.0f, face.getBottom(), false);
    grad.addColour (0.45, base);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, radius);

    g.setColour (juce::Colours::white.withAlpha (pressed ? 0.18f : 0.32f));
    g.drawLine (face.getX() + radius, face.getY() + 1.0f, face.getRight() - radius, face.getY() + 1.0f, 1.0f);

    if (! button.isEnabled())
        g.endTransparencyLayer();

    if (button.hasKeyboardFocus (false))
    {
        g.setColour (fin->led.withAlpha (0.9f));
        g.drawRoundedRectangle (face.expanded (1.5f), radius + 1.0f, 1.5f);
    }
}

void Sp92LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool down)
{
    const auto& props = button.getProperties();
    const bool dark = (bool) props.getWithDefault ("sp92Dark", false);
    const bool hasLed = (bool) props.getWithDefault ("sp92Led", false);
    const bool lit = button.getToggleState() || (bool) props.getWithDefault ("sp92Lit", false);
    const auto arrow = props.getWithDefault ("sp92Arrow", "").toString();
    const bool play = (bool) props.getWithDefault ("sp92Play", false);

    auto face = button.getLocalBounds().toFloat().reduced (0.5f);
    face.removeFromBottom (3.0f);
    if (down) face.translate (0.0f, 2.0f);

    const auto ink = (dark ? darkKey::ink : fin->keyInk).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.55f);

    if (arrow.isNotEmpty())
    {
        const float s = juce::jmin (face.getWidth(), face.getHeight()) * 0.22f;
        const auto c = face.getCentre();
        juce::Path tri;

        if (arrow == "left")
            tri.addTriangle (c.x + s * 0.8f, c.y - s, c.x + s * 0.8f, c.y + s, c.x - s * 0.9f, c.y);
        else
            tri.addTriangle (c.x - s * 0.8f, c.y - s, c.x - s * 0.8f, c.y + s, c.x + s * 0.9f, c.y);

        g.setColour (ink);
        g.fillPath (tri);
        return;
    }

    const auto font = fonts::label (juce::jlimit (10.0f, 13.0f, face.getHeight() * 0.42f)).withExtraKerningFactor (0.14f);
    const auto text = button.getButtonText().toUpperCase();

    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, 0.0f, 0.0f);
    const float textW = ga.getBoundingBox (0, -1, true).getWidth();

    const float ledW = hasLed ? 13.0f : 0.0f;
    const float playW = play ? 12.0f : 0.0f;
    float x = face.getCentreX() - (ledW + playW + textW) * 0.5f;
    const float cy = face.getCentreY();

    if (hasLed)
    {
        const juce::Rectangle<float> led { x, cy - 3.0f, 6.0f, 6.0f };

        if (lit)
        {
            g.setColour (fin->led.withAlpha (0.35f));
            g.fillEllipse (led.expanded (3.0f));
            g.setColour (fin->led);
        }
        else
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
        }

        g.fillEllipse (led);
        x += ledW;
    }

    if (play)
    {
        juce::Path tri;
        tri.addTriangle (x, cy - 4.5f, x, cy + 4.5f, x + 7.5f, cy);
        g.setColour (ink);
        g.fillPath (tri);
        x += playW;
    }

    g.setColour (ink);
    g.setFont (font);
    g.drawText (text, juce::Rectangle<float> (x, face.getY(), textW + 4.0f, face.getHeight()), juce::Justification::centredLeft, false);
}
}
