#include "Sp92LookAndFeel.h"
#include "Sp92Assets.h"
#include "Sp92Fonts.h"
#include "Anim.h"

#include <map>

namespace stabr::gui::sp92
{
namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float deg = pi / 180.0f;

    juce::Point<float> onCircle (juce::Point<float> c, float r, float angle)
    {
        // Angulos de JUCE: 0 arriba, sentido horario.
        return { c.x + std::sin (angle) * r, c.y - std::cos (angle) * r };
    }

    float prop (juce::Component& c, const char* id, float fallback)
    {
        const auto& v = c.getProperties()[id];
        return v.isVoid() ? fallback : (float) (double) v;
    }

    float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

    /*  box-shadow inset sin desenfoque: la media luna que deja la forma corrida
        dy (positivo: brillo arriba; negativo: sombra abajo). */
    void insetEllipse (juce::Graphics& g, juce::Rectangle<float> e, float dy, juce::Colour c)
    {
        juce::Path clip, crescent;
        clip.addEllipse (e);
        crescent.addEllipse (e);
        crescent.addEllipse (e.translated (0.0f, dy));
        crescent.setUsingNonZeroWinding (false);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (clip);
        g.setColour (c);
        g.fillPath (crescent);
    }

    void insetRounded (juce::Graphics& g, juce::Rectangle<float> r, float radius, float dy, juce::Colour c)
    {
        juce::Path clip, crescent;
        clip.addRoundedRectangle (r, radius);
        crescent.addRoundedRectangle (r, radius);
        crescent.addRoundedRectangle (r.translated (0.0f, dy), radius);
        crescent.setUsingNonZeroWinding (false);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (clip);
        g.setColour (c);
        g.fillPath (crescent);
    }

    /*  Tapa de aluminio: el conic-gradient de la maqueta, pintado una vez por
        tamano fisico y despues solo rotado. */
    const juce::Image& metalCap (int sizePx)
    {
        auto& cache = Sp92Assets::get().metalCaps;   // no estatico: ver D-053

        if (cache.size() > 48)
            cache.clear();

        auto& img = cache[sizePx];

        if (img.isValid())
            return img;

        img = juce::Image (juce::Image::ARGB, sizePx, sizePx, true, juce::SoftwareImageType());
        juce::Graphics g (img);

        const juce::Colour stops[] { juce::Colour (0xffe9e7e2), juce::Colour (0xff9b9994), juce::Colour (0xfff3f1ec), juce::Colour (0xff8a8883),
                                     juce::Colour (0xffdcd9d3), juce::Colour (0xff9d9b96), juce::Colour (0xffe9e7e2) };
        const auto r = juce::Rectangle<float> ((float) sizePx, (float) sizePx);
        constexpr int segments = 240;

        for (int i = 0; i < segments; ++i)
        {
            const float t = ((float) i + 0.5f) / (float) segments;
            const float x = t * 6.0f;
            const int k = juce::jmin (5, (int) x);
            const auto colour = stops[k].interpolatedWith (stops[k + 1], x - (float) k);

            const float a0 = 20.0f * deg + (float) i / (float) segments * 2.0f * pi;
            juce::Path wedge;
            wedge.addPieSegment (r, a0, a0 + 2.0f * pi / (float) segments + 0.01f, 0.0f);
            g.setColour (colour);
            g.fillPath (wedge);
        }

        return img;
    }

    void pointer (juce::Graphics& g, juce::Point<float> c, float d, float top, float height, float width,
                  float angle, juce::Colour colour, float yOrigin)
    {
        juce::Path p;
        p.addRoundedRectangle (c.x - width * 0.5f, yOrigin + d * top, width, d * height, 2.0f);
        p.applyTransform (juce::AffineTransform::rotation (angle, c.x, c.y));

        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.strokePath (p, juce::PathStrokeType (2.2f));
        g.setColour (colour);
        g.fillPath (p);
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
    return fonts::label (15.0f);
}

//==============================================================================
void Sp92LookAndFeel::paintKnob (juce::Graphics& g, juce::Rectangle<float> knob, float angle,
                                 const juce::String& style, int ticks, const Finish& f, bool enabled, bool focused)
{
    const float d = juce::jmin (knob.getWidth(), knob.getHeight());
    knob = knob.withSizeKeepingCentre (d, d);
    const auto c = knob.getCentre();
    const float r = d * 0.5f;
    const bool isSwitch = style == "switch";
    const bool isEncoder = style == "encoder";

    if (! enabled)
        g.beginTransparencyLayer (0.45f);

    // Marcas serigrafiadas: 270 grados, extremos y centro mas largos.
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

    // Sombras sobre el panel. La luz del estudio no gira con la perilla.
    anim::blurredEllipse (g, knob.translated (0.0f, 8.0f), juce::Colours::black.withAlpha (0.45f), 12.0f);
    anim::blurredEllipse (g, knob.translated (0.0f, 2.0f), juce::Colours::black.withAlpha (0.50f), 3.0f);

    // Cuerpo.
    {
        const juce::Point<float> light { knob.getX() + d * 0.36f, knob.getY() + d * 0.28f };
        const float reach = d * 0.963f;   // farthest-corner desde 36 % 28 %
        juce::ColourGradient body (isSwitch ? juce::Colour (0xff6a6865) : juce::Colour (0xff5d5c5a), light.x, light.y,
                                   isSwitch ? juce::Colour (0xff1a1918) : juce::Colour (0xff161514), light.x + reach, light.y, true);
        body.addColour (isSwitch ? 0.60 : 0.62, isSwitch ? juce::Colour (0xff2a2927) : juce::Colour (0xff242322));
        g.setGradientFill (body);
        g.fillEllipse (knob);

        // Moleteado: cunas de 5 grados que giran con la perilla.
        if (! isSwitch)
        {
            juce::Path knurl;
            for (int i = 0; i < 36; ++i)
            {
                const float a0 = angle + (float) i * 10.0f * deg;
                knurl.addPieSegment (knob, a0, a0 + 5.0f * deg, 0.0f);
            }

            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillPath (knurl);
        }

        insetEllipse (g, knob, 1.0f, juce::Colours::white.withAlpha (0.20f));

        for (int k = 0; k < 5; ++k)
            insetEllipse (g, knob, -(0.5f + (float) k * 1.25f), juce::Colours::black.withAlpha (0.6f / 5.0f * 1.2f));
    }

    if (isSwitch)
    {
        const auto inner = knob.reduced (d * 0.30f);
        juce::ColourGradient cap (juce::Colour (0xff444444), inner.getX() + inner.getWidth() * 0.4f, inner.getY() + inner.getHeight() * 0.35f,
                                  juce::Colour (0xff1d1d1d), inner.getX() + inner.getWidth() * 1.285f, inner.getY() + inner.getHeight() * 0.35f, true);
        g.setGradientFill (cap);
        g.fillEllipse (inner);

        pointer (g, c, d, 0.02f, 0.36f, 4.0f, angle, f.stab, knob.getY());
    }
    else if (isEncoder)
    {
        const auto inner = knob.reduced (d * 0.18f);
        juce::ColourGradient cap (juce::Colour (0xff4b4a48), inner.getX() + inner.getWidth() * 0.4f, inner.getY() + inner.getHeight() * 0.35f,
                                  juce::Colour (0xff1e1d1c), inner.getX() + inner.getWidth() * 1.285f, inner.getY() + inner.getHeight() * 0.35f, true);
        g.setGradientFill (cap);
        g.fillEllipse (inner);

        pointer (g, c, d, 0.05f, 0.22f, 3.0f, angle, juce::Colour (0xfff6f3ea), knob.getY());
    }
    else
    {
        const auto cap = knob.reduced (d * 0.24f);
        anim::blurredEllipse (g, cap.translated (0.0f, 1.0f), juce::Colours::black.withAlpha (0.6f), 2.0f);

        // Aluminio: se pinta una vez y gira con la perilla, como en la maqueta.
        const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int px = juce::jmax (8, juce::roundToInt (cap.getWidth() * scale));
        const auto& metal = metalCap (px);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addEllipse (cap);
            g.reduceClipRegion (clip);
            g.setOpacity (1.0f);   // drawImage usa la opacidad del ultimo color: la de la sombra
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImageTransformed (metal, juce::AffineTransform::translation (-(float) px * 0.5f, -(float) px * 0.5f)
                                               .rotated (angle)
                                               .scaled (cap.getWidth() / (float) px)
                                               .translated (cap.getCentreX(), cap.getCentreY()));
        }

        insetEllipse (g, cap, 1.0f, juce::Colours::white.withAlpha (0.45f));

        pointer (g, c, d, 0.05f, 0.26f, 3.0f, angle, juce::Colour (0xfff6f3ea), knob.getY());
    }

    if (! enabled)
        g.endTransparencyLayer();

    if (focused)
    {
        g.setColour (f.led);
        g.drawEllipse (knob.expanded (4.0f), 2.0f);
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
    const float radius = (float) props.getWithDefault ("sp92Radius", 6.0);
    const float press = prop (button, "sp92Press", down ? 1.0f : 0.0f);
    const float hover = prop (button, "sp92Hover", highlighted ? 1.0f : 0.0f);

    auto rest = button.getLocalBounds().toFloat();
    rest.removeFromBottom (3.0f);
    const auto face = rest.translated (0.0f, 2.0f * press);

    if (! button.isEnabled())
        g.beginTransparencyLayer (0.55f);

    // box-shadow: 0 5px 8px .2 y 0 3px 0 .35; hundida, 0 2px 3px y 0 1px 0.
    anim::blurredRoundedRect (g, face.translated (0.0f, lerp (5.0f, 2.0f, press)), radius,
                              juce::Colours::black.withAlpha (0.2f), lerp (8.0f, 3.0f, press));
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (face.translated (0.0f, lerp (3.0f, 1.0f, press)), radius);

    const auto base = (dark ? darkKey::base : fin->key).brighter (0.05f * hover);
    const auto hi   = (dark ? darkKey::hi : fin->keyHi).brighter (0.10f * hover);
    const auto lo   = dark ? darkKey::lo : fin->keyLo;

    juce::ColourGradient grad (hi, 0.0f, face.getY(), lo, 0.0f, face.getBottom(), false);
    grad.addColour (0.45, base);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, radius);

    insetRounded (g, face, radius, 1.0f, juce::Colours::white.withAlpha (lerp (0.35f, 0.25f, press)));

    if (! button.isEnabled())
        g.endTransparencyLayer();

    if (button.hasKeyboardFocus (false))
    {
        g.setColour (fin->led);
        g.drawRoundedRectangle (face.expanded (4.0f), radius + 4.0f, 2.0f);
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
    const float press = prop (button, "sp92Press", down ? 1.0f : 0.0f);
    const float litAmt = prop (button, "sp92LitAmt", lit ? 1.0f : 0.0f);

    auto face = button.getLocalBounds().toFloat();
    face.removeFromBottom (3.0f);
    face.translate (0.0f, 2.0f * press);

    const auto ink = (dark ? darkKey::ink : fin->keyInk).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.55f);

    if (arrow.isNotEmpty())
    {
        // Del tamano del glifo de 11 px de la maqueta.
        const auto c = face.getCentre();
        juce::Path tri;

        if (arrow == "left")
            tri.addTriangle (c.x + 3.8f, c.y - 4.3f, c.x + 3.8f, c.y + 4.3f, c.x - 3.8f, c.y);
        else
            tri.addTriangle (c.x - 3.8f, c.y - 4.3f, c.x - 3.8f, c.y + 4.3f, c.x + 3.8f, c.y);

        g.setColour (ink);
        g.fillPath (tri);
        return;
    }

    const auto font = fonts::labelBold (11.0f, 0.14f);
    const auto text = button.getButtonText().toUpperCase();
    const float textW = fonts::textWidth (font, text);

    const float ledW = hasLed ? 6.0f + 7.0f : 0.0f;
    const float playW = play ? 7.5f + 4.5f : 0.0f;
    float x = face.getCentreX() - (ledW + playW + textW) * 0.5f;
    const float cy = face.getCentreY();

    if (hasLed)
    {
        const juce::Rectangle<float> led { x, cy - 3.0f, 6.0f, 6.0f };
        anim::ledGlow (g, led, fin->led, litAmt, 7.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f).interpolatedWith (fin->led, litAmt));
        g.fillEllipse (led);
        x += ledW;
    }

    if (play)
    {
        juce::Path tri;
        tri.addTriangle (x, cy - 4.3f, x, cy + 4.3f, x + 7.5f, cy);
        g.setColour (ink);
        g.fillPath (tri);
        x += playW;
    }

    g.setColour (ink);
    fonts::drawText (g, text, font, { x, face.getY(), textW, face.getHeight() }, juce::Justification::centredLeft);
}
}
