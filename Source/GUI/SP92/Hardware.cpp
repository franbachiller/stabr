#include "Hardware.h"
#include "ButtonMotion.h"
#include "Sp92LookAndFeel.h"
#include "Sp92Fonts.h"

#include <cmath>

namespace stabr::gui::sp92
{
namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    juce::Point<float> onCircle (juce::Point<float> c, float r, float angle)
    {
        return { c.x + std::sin (angle) * r, c.y - std::cos (angle) * r };
    }

    float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
    double realNow() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    void focusRing (juce::Graphics& g, juce::Component& c, juce::Rectangle<float> r, float radius)
    {
        if (c.hasKeyboardFocus (false))
        {
            g.setColour (currentFinish (c).led);
            g.drawRoundedRectangle (r.expanded (4.0f), radius + 4.0f, 2.0f);
        }
    }

    /*  box-shadow inset sobre un rectangulo redondeado (dy > 0 brillo arriba,
        dy < 0 sombra abajo). */
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

    /*  Sombra inset desenfocada hacia abajo, en capas. */
    void insetShadeBottom (juce::Graphics& g, juce::Rectangle<float> r, float radius, float offset, float blur, juce::Colour c)
    {
        constexpr int layers = 6;

        for (int k = 0; k < layers; ++k)
        {
            const float dy = -juce::jmax (0.5f, offset + blur * ((float) k / (float) (layers - 1) - 0.5f));
            insetRounded (g, r, radius, dy, c.withMultipliedAlpha (1.2f / (float) layers));
        }
    }

    /*  Gradiente lineal con angulo CSS (0 = hacia arriba, 90 = hacia la derecha). */
    juce::ColourGradient cssLinear (juce::Rectangle<float> r, float degrees, juce::Colour from, juce::Colour to)
    {
        const float a = degrees * pi / 180.0f;
        const juce::Point<float> dir { std::sin (a), -std::cos (a) };
        const float len = std::abs (r.getWidth() * dir.x) + std::abs (r.getHeight() * dir.y);
        const auto c = r.getCentre();
        return { from, c - dir * (len * 0.5f), to, c + dir * (len * 0.5f), false };
    }
}

//==============================================================================
StabPad::StabPad() : juce::Button ("STAB")
{
    setTitle ("Create STAB: generate a new sound");
    setWantsKeyboardFocus (true);
    setPaintingIsUnclipped (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void StabPad::flash (double now)
{
    heldUntil = juce::jmax (heldUntil, now + 0.12);
    lastNow = now;
}

void StabPad::buttonStateChanged()
{
    if (isDown())
        heldUntil = realNow() + 0.12;   // la maqueta deja el golpe visible 120 ms
}

void StabPad::tick (double now, float dt)
{
    lastNow = now;
    const bool held = isDown() || now < heldUntil;
    const float target = held ? 1.0f : 0.0f;

    const float before = depth.pos, glowBefore = glow, hoverBefore = hover;

    if (held)
        depth.step (target, dt, 20.0f, 1.0f);   // se hunde firme
    else
        depth.step (target, dt, 9.0f, 0.5f);    // la goma vuelve y pasa apenas de largo

    if (depth.settled (target))
        depth.snap (target);

    glow  = anim::approach (glow, held ? 1.0f : 0.0f, dt, 0.02f, 0.18f);
    hover = anim::approach (hover, isOver() && isEnabled() ? 1.0f : 0.0f, dt, 0.05f, 0.12f);

    if (std::abs (depth.pos - before) > 0.0004f || glow != glowBefore || hover != hoverBefore)
        repaintWithMargin (*this, 26);
}

void StabPad::paintButton (juce::Graphics& g, bool, bool)
{
    const auto& f = currentFinish (*this);
    const juce::Rectangle<float> rest { 8.0f, 4.0f, 196.0f, 196.0f };
    constexpr float radius = 22.0f;

    const float p = juce::jlimit (0.0f, 1.0f, depth.pos);
    const auto face = rest.translated (0.0f, 6.0f * depth.pos);

    // Resplandor del golpe, sombra difusa y sombra dura (de abajo hacia arriba).
    anim::blurredRoundedRect (g, face, radius, f.stabHi.withAlpha (0.7f * glow), 34.0f);
    anim::blurredRoundedRect (g, face.translated (0.0f, 14.0f), radius, juce::Colours::black.withAlpha (0.35f * (1.0f - p)), 22.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (face.translated (0.0f, lerp (8.0f, 2.0f, p)), radius);

    // Goma: radial-gradient(120% 90% at 30% 20%, hi, stab 45%, lo).
    const juce::Point<float> centre { face.getX() + face.getWidth() * 0.3f, face.getY() + face.getHeight() * 0.2f };
    juce::ColourGradient rubber (f.stabHi, centre.x, centre.y, f.stabLo, centre.x + face.getWidth() * 1.2f, centre.y, true);
    rubber.addColour (0.45, f.stab);
    juce::FillType fill (rubber);
    fill.transform = juce::AffineTransform::scale (1.0f, (0.9f * face.getHeight()) / (1.2f * face.getWidth()), centre.x, centre.y);
    g.setFillType (fill);
    g.fillRoundedRectangle (face, radius);

    if (hover > 0.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.035f * hover));
        g.fillRoundedRectangle (face, radius);
    }

    insetRounded (g, face, radius, 2.0f, juce::Colours::white.withAlpha (lerp (0.35f, 0.25f, p)));
    insetShadeBottom (g, face, radius, lerp (8.0f, 4.0f, p), lerp (16.0f, 10.0f, p), juce::Colours::black.withAlpha (0.35f));

    const auto ink = juce::Colour (40, 12, 2);

    juce::Path border, dashed;
    border.addRoundedRectangle (face.reduced (14.5f), 13.5f);
    const float dashes[] { 3.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (ink.withAlpha (0.22f));
    g.fillPath (dashed);

    const auto title = fonts::display (30.0f, 0.18f);
    const juce::Rectangle<float> titleBox { face.getX(), face.getY() + 73.0f, face.getWidth(), 30.0f };
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    fonts::drawText (g, "STAB", title, titleBox.translated (0.0f, 1.0f), juce::Justification::centred);
    g.setColour (ink.withAlpha (0.55f));
    fonts::drawText (g, "STAB", title, titleBox, juce::Justification::centred);

    fonts::drawText (g, "HIT FOR A NEW SOUND", fonts::labelBold (10.0f, 0.3f),
                     { face.getX(), face.getY() + 113.0f, face.getWidth(), 10.0f }, juce::Justification::centred);

    focusRing (g, *this, face, radius);
}

//==============================================================================
BatSwitch::BatSwitch (const juce::String& title) : juce::ToggleButton (title)
{
    setTitle (title);
    setClickingTogglesState (true);
    setPaintingIsUnclipped (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void BatSwitch::tick (float dt)
{
    const float target = getToggleState() ? 1.0f : 0.0f;

    if (flip < 0.0f)
    {
        flip = lamp = target;
        repaint();
        return;
    }

    const float flipBefore = flip, lampBefore = lamp;

    // La maqueta: transform .12s. Avance lineal y curva al dibujar.
    if (flip < target)      flip = juce::jmin (target, flip + dt / 0.12f);
    else if (flip > target) flip = juce::jmax (target, flip - dt / 0.12f);

    lamp = anim::approach (lamp, target, dt, 0.03f, 0.09f);

    if (flip != flipBefore || lamp != lampBefore)
        repaintWithMargin (*this, 8);
}

void BatSwitch::paintButton (juce::Graphics& g, bool, bool)
{
    const auto& f = currentFinish (*this);
    const juce::Rectangle<float> plate { 0.0f, 0.0f, 46.0f, 64.0f };
    const float flipNow = juce::jlimit (0.0f, 1.0f, flip < 0.0f ? (getToggleState() ? 1.0f : 0.0f) : flip);
    const float lampNow = lamp < 0.0f ? flipNow : lamp;

    anim::blurredRoundedRect (g, plate.translated (0.0f, 5.0f), 6.0f, juce::Colours::black.withAlpha (0.25f), 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (plate.translated (0.0f, 3.0f), 6.0f);

    juce::ColourGradient metal (juce::Colour (0xffdcdad4), 0.0f, 0.0f, juce::Colour (0xff9d9a93), 0.0f, 64.0f, false);
    g.setGradientFill (metal);
    g.fillRoundedRectangle (plate, 6.0f);
    insetRounded (g, plate, 6.0f, 1.0f, juce::Colours::white);

    const juce::Point<float> pivot { 23.0f, 32.0f };
    const auto nut = juce::Rectangle<float> (26.0f, 26.0f).withCentre (pivot);
    anim::blurredEllipse (g, nut.translated (0.0f, 1.0f), juce::Colours::black.withAlpha (0.5f), 2.0f);
    juce::ColourGradient nutGrad (juce::Colour (0xfff4f2ee), nut.getX() + nut.getWidth() * 0.4f, nut.getY() + nut.getHeight() * 0.35f,
                                  juce::Colour (0xff8e8b85), nut.getX() + nut.getWidth() * (0.4f + 0.885f * 0.7f), nut.getY() + nut.getHeight() * 0.35f, true);
    g.setGradientFill (nutGrad);
    g.fillEllipse (nut);

    // Palanca: gira sobre la tuerca. Arriba 6..36; abajo, su reflejo 28..58.
    const float s = 1.0f - 2.0f * anim::easeInOutCubic (flipNow);
    const float yTip = pivot.y + (6.0f - pivot.y) * s;
    const float yBase = pivot.y + (36.0f - pivot.y) * s;
    const float top = juce::jmin (yTip, yBase), bottom = juce::jmax (yTip, yBase);

    if (bottom - top > 0.6f)
    {
        const juce::Rectangle<float> lever { pivot.x - 5.0f, top, 10.0f, bottom - top };
        const float r = juce::jmin (4.0f, lever.getHeight() * 0.5f);

        anim::blurredRoundedRect (g, lever.translated (0.0f, 3.0f), r, juce::Colours::black.withAlpha (0.35f), 3.0f);

        juce::ColourGradient chrome (juce::Colour (0xff8d8a84), lever.getX(), 0.0f, juce::Colour (0xffa7a49e), lever.getRight(), 0.0f, false);
        chrome.addColour (0.45, juce::Colour (0xfffbfaf6));
        g.setGradientFill (chrome);
        g.fillRoundedRectangle (lever, r);

        // De frente al girar se ve menos luz: la cara de abajo es mas oscura.
        const float underside = juce::jlimit (0.0f, 1.0f, -s);
        if (underside > 0.0f || std::abs (s) < 1.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.10f * underside + 0.08f * (1.0f - std::abs (s))));
            g.fillRoundedRectangle (lever, r);
        }

        const float tipY = yTip < yBase ? lever.getY() + 1.0f : lever.getBottom() - 5.0f;
        g.setColour (juce::Colours::white.withAlpha (0.35f * std::abs (s)));
        g.fillEllipse (lever.getX() + 2.5f, tipY, 5.0f, 4.0f);
    }

    const juce::Rectangle<float> lampRect { pivot.x - 3.0f, 72.0f, 6.0f, 6.0f };
    anim::ledGlow (g, lampRect, f.led, lampNow, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.3f).interpolatedWith (f.led, lampNow));
    g.fillEllipse (lampRect);

    focusRing (g, *this, plate, 6.0f);
}

//==============================================================================
PowerButton::PowerButton() : juce::Button ("Setup")
{
    setTitle ("Setup: finish, interface size and tape label");
    setTooltip ("Setup: finish, size, tape label");
    setWantsKeyboardFocus (true);
}

void PowerButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto& f = currentFinish (*this);
    const auto b = getLocalBounds().toFloat();
    const float hover = (float) (double) getProperties().getWithDefault ("sp92Hover", highlighted ? 1.0 : 0.0);
    const juce::Rectangle<float> led { b.getCentreX() - 4.5f, 8.0f, 9.0f, 9.0f };
    const auto red = juce::Colour (0xffff3b1f);

    anim::ledGlow (g, led, red, 1.0f + 0.4f * hover, 10.0f);
    g.setColour (red);
    g.fillEllipse (led);

    {
        juce::Path clip, shade;
        clip.addEllipse (led);
        shade.addEllipse (led);
        shade.addEllipse (led.translated (0.0f, -2.0f));
        shade.setUsingNonZeroWinding (false);
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (clip);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillPath (shade);
    }

    g.setColour (f.stripInk);
    fonts::drawText (g, "POWER", fonts::label (9.0f, 0.2f), { 0.0f, 25.0f, b.getWidth(), 9.0f }, juce::Justification::centred);

    if (hasKeyboardFocus (false))
    {
        g.setColour (f.led);
        g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 2.0f);
    }
}

//==============================================================================
FloppyDrive::FloppyDrive() : juce::Button ("Disk")
{
    setTitle ("Sound disk: open the sound list");
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void FloppyDrive::setDisk (const juce::String& name, const juce::String& header, juce::Colour colour)
{
    if (firstDisk)
    {
        firstDisk = false;
        shownName = name; shownHeader = header; shownColour = colour;
        setDescription (name);
        repaint();
        return;
    }

    if (name == (hasPending ? pendingName : shownName) && header == (hasPending ? pendingHeader : shownHeader))
        return;

    pendingName = name; pendingHeader = header; pendingColour = colour;
    hasPending = true;

    const double t = lastNow - swapStart;

    if (t >= kHalfSwap * 2.0)
    {
        swapStart = lastNow;       // disquetera libre: expulsa ya
        pendingThisSwap = true;
    }
    else
    {
        // Saliendo: el disco nuevo entra en esta misma pasada. Volviendo: en la proxima.
        pendingThisSwap = t < kHalfSwap;
    }

    setDescription (name);
}

float FloppyDrive::outAmount (double t) const
{
    if (t < 0.0 || t >= kHalfSwap * 2.0)
        return 0.0f;

    if (t < kHalfSwap)
        return anim::cubicBezier (0.3f, 0.7f, 0.3f, 1.0f, (float) (t / kHalfSwap));

    return 1.0f - anim::cubicBezier (0.3f, 0.7f, 0.3f, 1.0f, (float) ((t - kHalfSwap) / kHalfSwap));
}

void FloppyDrive::tick (double now, float dt)
{
    lastNow = now;
    double t = now - swapStart;

    if (hasPending)
    {
        if (pendingThisSwap && t >= kHalfSwap)
        {
            shownName = pendingName; shownHeader = pendingHeader; shownColour = pendingColour;
            hasPending = false;
        }
        else if (! pendingThisSwap && t >= kHalfSwap * 2.0)
        {
            swapStart = now;
            pendingThisSwap = true;
            t = 0.0;
        }
    }

    const float ledBefore = led, hoverBefore = hover;
    led = anim::approach (led, t >= 0.0 && t < 0.65 ? 1.0f : 0.0f, dt, 0.02f, 0.08f);
    hover = anim::approach (hover, isOver() ? 1.0f : 0.0f, dt, 0.05f, 0.12f);

    if (t < kHalfSwap * 2.0 + 0.05 || led != ledBefore || hover != hoverBefore)
        repaint();
}

void FloppyDrive::paintButton (juce::Graphics& g, bool, bool)
{
    const juce::Rectangle<float> bay { kBayX, 0.0f, 246.0f, 70.0f };
    const float out = outAmount (lastNow - swapStart);

    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.fillRoundedRectangle (bay.translated (0.0f, 1.0f), 6.0f);
    g.setColour (juce::Colour (0xff0d0b09));
    g.fillRoundedRectangle (bay, 6.0f);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (bay, 6.0f);
        g.reduceClipRegion (clip);

        // Sombra interna del hueco (queda detras del disquete).
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.9f), 0.0f, bay.getY(), juce::Colours::transparentBlack, 0.0f, bay.getY() + 11.0f, false);
        g.setGradientFill (inner);
        g.fillRect (bay);

        const float opacity = 1.0f - 0.8f * out;
        g.beginTransparencyLayer (opacity);

        const juce::Rectangle<float> disk { bay.getX() + 22.0f, 8.0f + out * 72.0f, 202.0f, 150.0f };
        juce::ColourGradient plastic (shownColour.interpolatedWith (juce::Colours::white, 0.2f), 0.0f, disk.getY(),
                                      shownColour, 0.0f, disk.getY() + disk.getHeight() * 0.3f, false);
        g.setGradientFill (plastic);
        g.fillRoundedRectangle (disk, 4.0f);

        const juce::Rectangle<float> label { disk.getX() + 14.0f, disk.getY() + 6.0f, disk.getWidth() - 28.0f, 48.0f };
        g.setColour (juce::Colour (0xfff4f0e6));
        g.fillRoundedRectangle (label, 2.0f);

        g.setColour (juce::Colour (40, 80, 190).withAlpha (0.18f));
        for (float y = label.getY() + 15.0f; y < label.getBottom(); y += 16.0f)
            g.fillRect (label.getX(), y, label.getWidth(), 1.0f);

        g.setColour (juce::Colour (0xffc3321b));
        fonts::drawText (g, shownHeader, fonts::labelBold (7.0f, 0.2f), { label.getX() + 8.0f, label.getY() + 4.0f, label.getWidth() - 16.0f, 7.0f },
                         juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff1c3494));
        g.setFont (fonts::hand (25.0f));
        g.drawText (shownName, juce::Rectangle<float> (label.getX() + 8.0f, label.getY() + 13.0f, label.getWidth() - 16.0f, 25.0f),
                    juce::Justification::centredLeft, true);

        g.endTransparencyLayer();

        // Acrilico ahumado con reflejo.
        g.setColour (juce::Colour (40, 24, 10).withAlpha (0.22f));
        g.fillRect (bay);
        auto glare = cssLinear (bay, 170.0f, juce::Colours::white.withAlpha (0.14f + 0.06f * hover), juce::Colours::white.withAlpha (0.0f));
        glare.clearColours();
        glare.addColour (0.0, juce::Colours::white.withAlpha (0.14f + 0.06f * hover));
        glare.addColour (0.38, juce::Colours::white.withAlpha (0.0f));
        glare.addColour (1.0, juce::Colours::white.withAlpha (0.0f));
        g.setGradientFill (glare);
        g.fillRect (bay);
    }

    // Piloto de lectura, junto a la disquetera como en la maqueta.
    const juce::Rectangle<float> ledRect { 2.0f, 58.0f, 6.0f, 6.0f };
    const auto green = juce::Colour (0xff6dff4a);
    anim::ledGlow (g, ledRect, green, led, 8.0f);
    g.setColour (juce::Colour (0xff3a2a18).interpolatedWith (green, led));
    g.fillEllipse (ledRect);

    if (hasKeyboardFocus (false))
    {
        g.setColour (currentFinish (*this).led);
        g.drawRoundedRectangle (bay.reduced (1.0f), 6.0f, 2.0f);
    }
}

//==============================================================================
VuMeter::VuMeter()
{
    setTitle ("Output level meter");
    setInterceptsMouseClicks (false, false);
}

void VuMeter::push (float peakL, float peakR)
{
    peak = juce::jmax (peak, peakL, peakR);
}

void VuMeter::tick (float dt)
{
    // 0 VU = -12 dBFS de pico: un stab calibrado a -18 dB de golpe mueve la
    // aguja alrededor del cero, que es donde se lee un VU.
    const float vu = 20.0f * std::log10 (juce::jmax (peak, 1.0e-5f)) + 12.0f;
    peak = 0.0f;

    // La escala de la cara no es lineal: se interpola entre sus marcas.
    constexpr float marks[][2] { { -20.0f, 0.0f }, { -10.0f, 0.25f }, { -7.0f, 0.4f }, { -5.0f, 0.52f }, { -3.0f, 0.66f }, { 0.0f, 0.82f }, { 3.0f, 1.0f } };
    float n = vu <= marks[0][0] ? 0.0f : 1.04f;

    for (int i = 1; i < 7; ++i)
        if (vu > marks[i - 1][0] && vu <= marks[i][0])
            n = marks[i - 1][1] + (marks[i][1] - marks[i - 1][1]) * (vu - marks[i - 1][0]) / (marks[i][0] - marks[i - 1][0]);

    envelope = anim::approach (envelope, n, dt, 0.0f, 0.10f);
    needle.step (envelope, dt, 2.1f, 0.78f);

    // Topes mecanicos.
    if (needle.pos < 0.0f)  { needle.pos = 0.0f; needle.vel = juce::jmax (0.0f, needle.vel) * 0.3f; }
    if (needle.pos > 1.03f) { needle.pos = 1.03f; needle.vel = juce::jmin (0.0f, needle.vel) * 0.3f; }

    if (std::abs (needle.pos - drawn) > 0.0008f)
    {
        drawn = needle.pos;
        repaint();
    }
}

void VuMeter::paint (juce::Graphics& g)
{
    const auto housing = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff14110e));
    g.fillRoundedRectangle (housing, 8.0f);

    const auto face = housing.reduced (7.0f);
    g.setColour (juce::Colour (0xffefe6c8));
    g.fillRoundedRectangle (face, 3.0f);

    // Coordenadas del SVG de la maqueta: cara de 164 x 96, pivote en (82, 104).
    const float sx = face.getWidth() / 164.0f, sy = face.getHeight() / 96.0f;
    auto P = [&] (float x, float y) { return juce::Point<float> (face.getX() + x * sx, face.getY() + y * sy); };
    const auto pivot = P (82.0f, 104.0f);
    const auto ink = juce::Colour (0xff3a332a), red = juce::Colour (0xffc7361d);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (face.toNearestInt());

        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, 82.0f * sx, 82.0f * sx, 0.0f, -48.0f * pi / 180.0f, 48.0f * pi / 180.0f, true);
        g.setColour (ink);
        g.strokePath (arc, juce::PathStrokeType (1.2f));

        juce::Path hot;
        hot.addCentredArc (pivot.x, pivot.y, 74.0f * sx, 74.0f * sx, 0.0f, 28.6f * pi / 180.0f, 55.0f * pi / 180.0f, true);
        g.setColour (red);
        g.strokePath (hot, juce::PathStrokeType (4.0f));

        const std::pair<const char*, float> marks[] { { "-20", 0.0f }, { "-10", 0.25f }, { "-7", 0.4f }, { "-5", 0.52f }, { "-3", 0.66f }, { "0", 0.82f }, { "+3", 1.0f } };
        const auto numbers = fonts::label (8.0f);
        g.setFont (numbers);

        for (const auto& [text, n] : marks)
        {
            const float a = (-48.0f + 96.0f * n) * pi / 180.0f;
            g.setColour (ink);
            g.drawLine ({ onCircle (pivot, 82.0f * sx, a), onCircle (pivot, 90.0f * sx, a) }, 1.2f);

            const auto at = onCircle (pivot, 98.0f * sx, a);
            g.setColour (n >= 0.82f ? red : ink);
            const float w = fonts::textWidth (numbers, text);
            g.drawSingleLineText (text, juce::roundToInt (at.x - w * 0.5f), juce::roundToInt (at.y + 3.0f));
        }

        g.setColour (ink);
        const auto vuFont = fonts::display (11.0f);
        g.setFont (vuFont);
        g.drawSingleLineText ("VU", juce::roundToInt (P (82.0f, 0.0f).x - fonts::textWidth (vuFont, "VU") * 0.5f), juce::roundToInt (P (0.0f, 82.0f).y));

        g.setColour (red);
        const auto plus = fonts::labelBold (7.0f);
        g.setFont (plus);
        g.drawSingleLineText ("+", juce::roundToInt (P (150.0f, 0.0f).x - fonts::textWidth (plus, "+")), juce::roundToInt (P (0.0f, 88.0f).y));

        const float a = (-48.0f + 96.0f * juce::jlimit (0.0f, 1.03f, needle.pos)) * pi / 180.0f;
        juce::Path needlePath;
        needlePath.startNewSubPath (pivot);
        needlePath.lineTo (onCircle (pivot, 82.0f * sy, a));
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.strokePath (needlePath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (1.2f, 1.6f));
        g.setColour (juce::Colour (0xff1d1a16));
        g.strokePath (needlePath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0xff2a251f));
        g.fillEllipse (juce::Rectangle<float> (24.0f * sx, 24.0f * sx).withCentre (pivot));
    }

    auto glass = cssLinear (face, 160.0f, juce::Colours::white.withAlpha (0.22f), juce::Colours::white.withAlpha (0.0f));
    glass.clearColours();
    glass.addColour (0.0, juce::Colours::white.withAlpha (0.22f));
    glass.addColour (0.4, juce::Colours::white.withAlpha (0.0f));
    glass.addColour (1.0, juce::Colours::white.withAlpha (0.0f));
    g.setGradientFill (glass);
    g.fillRoundedRectangle (face, 3.0f);

    juce::ColourGradient innerShade (juce::Colours::black.withAlpha (0.8f), 0.0f, housing.getY(), juce::Colours::transparentBlack, 0.0f, housing.getY() + 9.0f, false);
    g.setGradientFill (innerShade);
    g.fillRoundedRectangle (housing.withHeight (9.0f), 8.0f);
}

//==============================================================================
TapeLabel::TapeLabel()
{
    setTitle ("Tape label");
    setTooltip ("Double-click to write on the tape");
    setEditable (false, true, false);
    setJustificationType (juce::Justification::centredLeft);
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

juce::Rectangle<float> TapeLabel::tapeArea() const
{
    // La maqueta: padding 3 px 14 px, letra de 27 px, ancho segun el texto.
    const float textW = fonts::textWidth (fonts::hand (27.0f), getText());
    const float w = juce::jlimit (70.0f, (float) getWidth() - 12.0f, textW + 28.0f);
    return { 6.0f, 8.0f, w, 33.0f };
}

void TapeLabel::paint (juce::Graphics& g)
{
    const auto b = tapeArea();

    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::rotation (-2.2f * pi / 180.0f, b.getCentreX(), b.getCentreY()));

    // clip-path de la maqueta: bordes cortados a mano.
    juce::Path tape;
    tape.startNewSubPath (b.getX() + b.getWidth() * 0.01f, b.getY() + b.getHeight() * 0.08f);
    tape.lineTo (b.getRight() - b.getWidth() * 0.01f, b.getY());
    tape.lineTo (b.getRight(), b.getY() + b.getHeight() * 0.90f);
    tape.lineTo (b.getX() + b.getWidth() * 0.97f, b.getBottom());
    tape.lineTo (b.getX(), b.getY() + b.getHeight() * 0.96f);
    tape.lineTo (b.getX() + b.getWidth() * 0.02f, b.getCentreY());
    tape.closeSubPath();

    g.setColour (juce::Colours::black.withAlpha (0.10f));
    g.fillPath (tape, juce::AffineTransform::translation (0.0f, 1.0f));

    juce::ColourGradient paper (juce::Colour (236, 222, 178).withAlpha (0.96f), 0.0f, b.getY(),
                                juce::Colour (222, 206, 160).withAlpha (0.96f), 0.0f, b.getBottom(), false);
    g.setGradientFill (paper);
    g.fillPath (tape);

    if (! isBeingEdited())
    {
        const auto handFont = fonts::hand (27.0f);
        const auto box = juce::Rectangle<float> (b.getX() + 14.0f, b.getY() + 3.0f, b.getWidth() - 28.0f, 27.0f);
        g.setColour (juce::Colour (0xff1d2c70));
        g.setFont (handFont);

        if (fonts::textWidth (handFont, getText()) <= box.getWidth() + 0.5f)
            g.drawText (getText(), box.withWidth (box.getWidth() + 4.0f), juce::Justification::centredLeft, false);
        else
            g.drawFittedText (getText(), box.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }
}

void TapeLabel::resized()
{
    juce::Label::resized();

    if (auto* ed = getCurrentTextEditor())
        ed->setBounds (tapeArea().withWidth ((float) getWidth() - 12.0f).toNearestInt());
}

void TapeLabel::editorShown (juce::TextEditor* ed)
{
    ed->setBounds (tapeArea().withWidth ((float) getWidth() - 12.0f).toNearestInt());
}

juce::TextEditor* TapeLabel::createEditorComponent()
{
    auto* ed = juce::Label::createEditorComponent();
    ed->setFont (fonts::hand (24.0f));
    ed->setJustification (juce::Justification::centredLeft);
    ed->setIndents (14, 0);
    ed->setColour (juce::TextEditor::backgroundColourId, juce::Colour (236, 222, 178));
    ed->setColour (juce::TextEditor::textColourId, juce::Colour (0xff1d2c70));
    ed->setColour (juce::TextEditor::highlightColourId, juce::Colour (0xff1d2c70).withAlpha (0.18f));
    ed->setColour (juce::CaretComponent::caretColourId, juce::Colour (0xff1d2c70));
    ed->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    ed->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    ed->setInputRestrictions (28);
    return ed;
}

//==============================================================================
PatternSwitch::PatternSwitch (const juce::StringArray& pos, const juce::String& title)
    : knob ("switch", pos.size(), title), positions (pos), amounts ((size_t) pos.size(), -1.0f)
{
    setPaintingIsUnclipped (true);
    addAndMakeVisible (knob);
}

void PatternSwitch::resized()
{
    knob.setBounds (getLocalBounds());
}

void PatternSwitch::tick (float dt)
{
    const int active = juce::roundToInt (knob.getValue());
    bool changed = false;

    for (size_t i = 0; i < amounts.size(); ++i)
    {
        const float target = (int) i == active ? 1.0f : 0.0f;
        const float next = amounts[i] < 0.0f ? target : anim::approach (amounts[i], target, dt, 0.05f);
        changed = changed || next != amounts[i];
        amounts[i] = next;
    }

    if (changed)
        repaintWithMargin (*this, 72);
}

juce::Rectangle<float> PatternSwitch::getLabelBounds (int index) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float a = (-135.0f + 270.0f * (float) index / (float) juce::jmax (1, positions.size() - 1)) * pi / 180.0f;
    const auto at = onCircle (c, kLabelRadius, a);
    const float w = fonts::textWidth (fonts::label (9.0f, 0.1f), positions[index].toUpperCase());
    return { at.x - w * 0.5f, at.y - 4.5f, w, 9.0f };
}

void PatternSwitch::paint (juce::Graphics& g)
{
    const auto& f = currentFinish (*this);
    const auto font = fonts::label (9.0f, 0.1f);

    for (int i = 0; i < positions.size(); ++i)
    {
        const float amt = juce::jmax (0.0f, amounts[(size_t) i]);
        g.setColour (f.silkMuted.interpolatedWith (f.silk, amt));
        fonts::drawText (g, positions[i].toUpperCase(), font, getLabelBounds (i), juce::Justification::centredLeft);
    }
}

//==============================================================================
SectionHead::SectionHead()
{
    setInterceptsMouseClicks (false, false);
}

void SectionHead::setText (const juce::String& t)
{
    if (t != text)
    {
        text = t;
        repaint();
    }
}

void SectionHead::paint (juce::Graphics& g)
{
    draw (g, text, getLocalBounds().toFloat(), currentFinish (*this).silk);
}

void SectionHead::draw (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> area, juce::Colour colour)
{
    const auto font = fonts::labelBold (11.0f, 0.24f);
    const float w = fonts::textWidth (font, t);

    g.setColour (colour);
    fonts::drawText (g, t, font, area.withHeight (11.0f), juce::Justification::centredLeft);

    const float x = area.getX() + w + 10.0f;
    g.setColour (colour.withMultipliedAlpha (0.35f));
    g.fillRect (x, area.getY() + 5.0f, juce::jmax (0.0f, area.getRight() - x), 1.0f);
}
}
