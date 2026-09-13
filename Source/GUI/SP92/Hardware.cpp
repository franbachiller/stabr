#include "Hardware.h"
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

    void focusRing (juce::Graphics& g, juce::Component& c, juce::Rectangle<float> r, float radius)
    {
        if (c.hasKeyboardFocus (false))
        {
            g.setColour (currentFinish (c).led.withAlpha (0.9f));
            g.drawRoundedRectangle (r.expanded (2.0f), radius + 2.0f, 1.5f);
        }
    }
}

//==============================================================================
StabPad::StabPad() : juce::Button ("STAB")
{
    setTitle ("Create STAB: generate a new sound");
    setWantsKeyboardFocus (true);
}

void StabPad::tick (double now)
{
    lastNow = now;

    if (now - flashAt < 0.25)
        repaint();
}

void StabPad::paintButton (juce::Graphics& g, bool, bool down)
{
    const auto& f = currentFinish (*this);
    auto b = getLocalBounds().toFloat().reduced (8.0f, 4.0f);
    b.removeFromBottom (10.0f);

    const bool pressed = down || (lastNow - flashAt) < 0.12;

    if (! pressed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.fillRoundedRectangle (b.translated (0.0f, 12.0f).expanded (3.0f), 25.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (b.translated (0.0f, 8.0f), 22.0f);
    }

    const auto face = pressed ? b.translated (0.0f, 6.0f) : b;

    if (pressed)
    {
        g.setColour (f.stabHi.withAlpha (0.35f));
        g.fillRoundedRectangle (face.expanded (6.0f), 28.0f);
    }

    juce::ColourGradient rubber (f.stabHi, face.getX() + face.getWidth() * 0.3f, face.getY() + face.getHeight() * 0.2f,
                                 f.stabLo, face.getRight(), face.getBottom(), true);
    rubber.addColour (0.45, f.stab);
    g.setGradientFill (rubber);
    g.fillRoundedRectangle (face, 22.0f);

    juce::ColourGradient shade (juce::Colours::transparentBlack, 0.0f, face.getCentreY(),
                                juce::Colours::black.withAlpha (0.25f), 0.0f, face.getBottom(), false);
    g.setGradientFill (shade);
    g.fillRoundedRectangle (face, 22.0f);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (face.getX() + 22.0f, face.getY() + 1.5f, face.getRight() - 22.0f, face.getY() + 1.5f, 1.5f);

    juce::Path border, dashed;
    border.addRoundedRectangle (face.reduced (14.0f), 14.0f);
    const float dashes[] { 5.0f, 4.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, border, dashes, 2);
    const auto ink = juce::Colour (40, 12, 2);
    g.setColour (ink.withAlpha (0.22f));
    g.fillPath (dashed);

    g.setColour (ink.withAlpha (0.58f));
    g.setFont (fonts::display (30.0f).withExtraKerningFactor (0.18f));
    g.drawText ("STAB", face.withTrimmedBottom (face.getHeight() * 0.18f), juce::Justification::centred, false);

    g.setFont (fonts::label (10.5f).withExtraKerningFactor (0.3f));
    g.drawText ("HIT FOR A NEW SOUND", face.withTrimmedTop (face.getHeight() * 0.52f).withHeight (20.0f), juce::Justification::centred, false);

    focusRing (g, *this, face, 22.0f);
}

//==============================================================================
BatSwitch::BatSwitch (const juce::String& title) : juce::ToggleButton (title)
{
    setTitle (title);
    setClickingTogglesState (true);
}

void BatSwitch::paintButton (juce::Graphics& g, bool, bool)
{
    const auto& f = currentFinish (*this);
    const juce::Rectangle<float> plate { 0.0f, 0.0f, 46.0f, 64.0f };
    const bool on = getToggleState();

    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (plate.translated (0.0f, 5.0f), 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (plate.translated (0.0f, 3.0f), 6.0f);

    juce::ColourGradient metal (juce::Colour (0xffdcdad4), 0.0f, 0.0f, juce::Colour (0xff9d9a93), 0.0f, 64.0f, false);
    g.setGradientFill (metal);
    g.fillRoundedRectangle (plate, 6.0f);
    g.setColour (juce::Colours::white);
    g.drawLine (6.0f, 1.0f, 40.0f, 1.0f, 1.0f);

    const auto centre = plate.getCentre();
    const auto nut = juce::Rectangle<float> (26.0f, 26.0f).withCentre (centre);
    juce::ColourGradient nutGrad (juce::Colour (0xfff4f2ee), nut.getX() + 10.0f, nut.getY() + 9.0f, juce::Colour (0xff8e8b85), nut.getRight(), nut.getBottom(), true);
    g.setGradientFill (nutGrad);
    g.fillEllipse (nut);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (nut, 0.8f);

    // Palanca: arriba apagada, abajo encendida.
    const juce::Rectangle<float> lever { centre.x - 5.0f, on ? centre.y : 6.0f, 10.0f, on ? 26.0f : 26.0f };
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (lever.translated (1.5f, 3.0f), 5.0f);
    juce::ColourGradient chrome (juce::Colour (0xff8d8a84), lever.getX(), 0.0f, juce::Colour (0xffa7a49e), lever.getRight(), 0.0f, false);
    chrome.addColour (0.45, juce::Colour (0xfffbfaf6));
    g.setGradientFill (chrome);
    g.fillRoundedRectangle (lever, 5.0f);

    const juce::Rectangle<float> lamp { centre.x - 3.0f, 72.0f, 6.0f, 6.0f };

    if (on)
    {
        g.setColour (f.led.withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (3.5f));
        g.setColour (f.led);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.3f));
    }

    g.fillEllipse (lamp);
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
    const juce::Rectangle<float> led { b.getCentreX() - 4.5f, 8.0f, 9.0f, 9.0f };
    const auto red = juce::Colour (0xffff3b1f);

    g.setColour (red.withAlpha (highlighted ? 0.55f : 0.35f));
    g.fillEllipse (led.expanded (5.0f));
    g.setColour (red);
    g.fillEllipse (led);

    g.setColour (f.stripInk);
    g.setFont (fonts::label (9.0f).withExtraKerningFactor (0.2f));
    g.drawText ("POWER", b.withTrimmedTop (22.0f).withHeight (12.0f), juce::Justification::centred, false);

    focusRing (g, *this, b.reduced (2.0f), 4.0f);
}

//==============================================================================
FloppyDrive::FloppyDrive() : juce::Button ("Disk")
{
    setTitle ("Sound disk: open the sound list");
    setWantsKeyboardFocus (true);
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

    if (lastNow - swapStart > kHalfSwap * 2.0)
        swapStart = lastNow;

    setDescription (name);
}

void FloppyDrive::tick (double now)
{
    lastNow = now;
    const double t = now - swapStart;

    if (hasPending && t >= kHalfSwap)
    {
        shownName = pendingName; shownHeader = pendingHeader; shownColour = pendingColour;
        hasPending = false;
    }

    if (t < kHalfSwap * 2.0 + 0.05)
        repaint();
}

void FloppyDrive::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto bay = getLocalBounds().toFloat();
    const double t = lastNow - swapStart;

    // Recorrido del disquete: baja en la primera mitad, sube en la segunda.
    float drop = 0.0f;
    if (t >= 0.0 && t < kHalfSwap)
        drop = (float) (t / kHalfSwap);
    else if (t >= kHalfSwap && t < kHalfSwap * 2.0)
        drop = 1.0f - (float) ((t - kHalfSwap) / kHalfSwap);

    g.setColour (juce::Colour (0xff0d0b09));
    g.fillRoundedRectangle (bay, 6.0f);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (bay.toNearestInt());

        const juce::Rectangle<float> disk { 22.0f, 8.0f + drop * 72.0f, 202.0f, 150.0f };
        juce::ColourGradient plastic (shownColour.interpolatedWith (juce::Colours::white, 0.2f), 0.0f, disk.getY(),
                                      shownColour, 0.0f, disk.getY() + 45.0f, false);
        g.setGradientFill (plastic);
        g.fillRoundedRectangle (disk, 4.0f);

        const juce::Rectangle<float> label { disk.getX() + 14.0f, disk.getY() + 6.0f, disk.getWidth() - 28.0f, 48.0f };
        g.setColour (juce::Colour (0xfff4f0e6));
        g.fillRoundedRectangle (label, 2.0f);

        g.setColour (juce::Colour (40, 80, 190).withAlpha (0.18f));
        for (float y = label.getY() + 16.0f; y < label.getBottom(); y += 16.0f)
            g.drawHorizontalLine (juce::roundToInt (y), label.getX(), label.getRight());

        g.setColour (juce::Colour (0xffc3321b));
        g.setFont (fonts::label (7.5f).withExtraKerningFactor (0.2f));
        g.drawText (shownHeader, label.reduced (8.0f, 3.0f).withHeight (10.0f), juce::Justification::centredLeft, false);

        g.setColour (juce::Colour (0xff1c3494));
        g.setFont (fonts::hand (25.0f));
        g.drawFittedText (shownName, label.reduced (8.0f, 0.0f).withTrimmedTop (13.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);

        if (drop > 0.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (drop * 0.6f));
            g.fillRect (bay);
        }
    }

    // Acrilico ahumado con reflejo.
    g.setColour (juce::Colour (40, 24, 10).withAlpha (0.22f));
    g.fillRoundedRectangle (bay, 6.0f);
    juce::ColourGradient reflection (juce::Colours::white.withAlpha (highlighted ? 0.2f : 0.14f), 0.0f, 0.0f,
                                     juce::Colours::white.withAlpha (0.0f), bay.getWidth() * 0.25f, bay.getHeight() * 1.2f, false);
    g.setGradientFill (reflection);
    g.fillRoundedRectangle (bay, 6.0f);

    juce::ColourGradient inner (juce::Colours::black.withAlpha (0.85f), 0.0f, 0.0f, juce::Colours::transparentBlack, 0.0f, 8.0f, false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (bay, 6.0f);

    // Piloto de lectura mientras carga.
    const bool reading = t >= 0.0 && t < kHalfSwap * 2.0 + 0.35;
    const juce::Rectangle<float> led { bay.getRight() - 10.0f, bay.getBottom() - 10.0f, 5.0f, 5.0f };
    g.setColour (reading ? juce::Colour (0xff6dff4a) : juce::Colour (0xff3a2a18));
    g.fillEllipse (led);

    focusRing (g, *this, bay.reduced (1.0f), 6.0f);
}

//==============================================================================
VuMeter::VuMeter()
{
    setTitle ("Output level meter");
    setInterceptsMouseClicks (false, false);
}

void VuMeter::push (float peakL, float peakR)
{
    const float peak = juce::jmax (peakL, peakR);
    const float db = 20.0f * std::log10 (juce::jmax (peak, 1.0e-5f));
    const float level = juce::jlimit (0.0f, 1.0f, (db + 20.0f) / 23.0f);   // -20 a +3 VU
    target = juce::jmax (level, target * 0.9f);
}

void VuMeter::tick()
{
    // Balistica de aguja: sube rapido, cae despacio.
    const float speed = target > needle ? 0.35f : 0.08f;
    needle += (target - needle) * speed;
    target *= 0.92f;

    if (std::abs (needle - drawn) > 0.002f)
    {
        drawn = needle;
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

    // Coordenadas del dibujo original: cara de 164 x 96, pivote en (82, 104).
    const float sx = face.getWidth() / 164.0f, sy = face.getHeight() / 96.0f;
    auto P = [&] (float x, float y) { return juce::Point<float> (face.getX() + x * sx, face.getY() + y * sy); };
    const auto pivot = P (82.0f, 104.0f);
    const float radius = 82.0f * sx;

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (face.toNearestInt());

    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, -48.0f * pi / 180.0f, 48.0f * pi / 180.0f, true);
    g.setColour (juce::Colour (0xff3a332a));
    g.strokePath (arc, juce::PathStrokeType (1.2f));

    juce::Path red;
    red.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, (-48.0f + 96.0f * 0.82f) * pi / 180.0f, 48.0f * pi / 180.0f, true);
    g.setColour (juce::Colour (0xffc7361d));
    g.strokePath (red, juce::PathStrokeType (4.0f));

    const std::pair<const char*, float> marks[] { { "-20", 0.0f }, { "-10", 0.25f }, { "-7", 0.4f }, { "-5", 0.52f }, { "-3", 0.66f }, { "0", 0.82f }, { "+3", 1.0f } };
    g.setFont (fonts::label (8.5f));

    for (const auto& [text, n] : marks)
    {
        const float a = (-48.0f + 96.0f * n) * pi / 180.0f;
        g.setColour (juce::Colour (0xff3a332a));
        g.drawLine ({ onCircle (pivot, radius, a), onCircle (pivot, radius + 8.0f * sx, a) }, 1.2f);
        g.setColour (n >= 0.82f ? juce::Colour (0xffc7361d) : juce::Colour (0xff3a332a));
        const auto at = onCircle (pivot, radius + 16.0f * sx, a);
        g.drawText (text, juce::Rectangle<float> (at.x - 12.0f, at.y - 6.0f, 24.0f, 12.0f), juce::Justification::centred, false);
    }

    g.setColour (juce::Colour (0xff3a332a));
    g.setFont (fonts::display (11.0f));
    g.drawText ("VU", juce::Rectangle<float> (pivot.x - 20.0f, P (0.0f, 72.0f).y, 40.0f, 14.0f), juce::Justification::centred, false);

    const float a = (-48.0f + 96.0f * juce::jlimit (0.0f, 1.0f, needle)) * pi / 180.0f;
    g.setColour (juce::Colour (0xff1d1a16));
    g.drawLine ({ pivot, onCircle (pivot, 82.0f * sy, a) }, 1.4f);
    g.setColour (juce::Colour (0xff2a251f));
    g.fillEllipse (juce::Rectangle<float> (24.0f * sx, 24.0f * sx).withCentre (pivot));

    juce::ColourGradient glass (juce::Colours::white.withAlpha (0.22f), face.getX(), face.getY(),
                                juce::Colours::white.withAlpha (0.0f), face.getX() + face.getWidth() * 0.4f, face.getY() + face.getHeight() * 0.7f, false);
    g.setGradientFill (glass);
    g.fillRect (face);
}

//==============================================================================
TapeLabel::TapeLabel()
{
    setTitle ("Tape label");
    setTooltip ("Double-click to write on the tape");
    setEditable (false, true, false);
    setJustificationType (juce::Justification::centred);
}

void TapeLabel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (6.0f, 6.0f);

    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::rotation (-2.2f * pi / 180.0f, b.getCentreX(), b.getCentreY()));

    // Bordes cortados a mano, como la cinta arrancada del rollo.
    juce::Path tape;
    tape.startNewSubPath (b.getX() + b.getWidth() * 0.01f, b.getY() + b.getHeight() * 0.08f);
    tape.lineTo (b.getRight() - b.getWidth() * 0.01f, b.getY());
    tape.lineTo (b.getRight(), b.getBottom() - b.getHeight() * 0.1f);
    tape.lineTo (b.getRight() - b.getWidth() * 0.03f, b.getBottom());
    tape.lineTo (b.getX(), b.getBottom() - b.getHeight() * 0.04f);
    tape.lineTo (b.getX() + b.getWidth() * 0.02f, b.getCentreY());
    tape.closeSubPath();

    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.fillPath (tape, juce::AffineTransform::translation (0.0f, 1.5f));

    juce::ColourGradient paper (juce::Colour (236, 222, 178), 0.0f, b.getY(), juce::Colour (222, 206, 160), 0.0f, b.getBottom(), false);
    g.setGradientFill (paper);
    g.fillPath (tape);

    if (! isBeingEdited())
    {
        g.setColour (juce::Colour (0xff1d2c70));
        g.setFont (fonts::hand (27.0f));
        g.drawFittedText (getText(), b.reduced (12.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

juce::TextEditor* TapeLabel::createEditorComponent()
{
    auto* ed = juce::Label::createEditorComponent();
    ed->setFont (fonts::hand (22.0f));
    ed->setJustification (juce::Justification::centred);
    ed->setColour (juce::TextEditor::backgroundColourId, juce::Colour (236, 222, 178));
    ed->setColour (juce::TextEditor::textColourId, juce::Colour (0xff1d2c70));
    ed->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    ed->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffff6b1c));
    ed->setInputRestrictions (28);
    return ed;
}

//==============================================================================
PatternSwitch::PatternSwitch (const juce::StringArray& pos, const juce::String& title)
    : knob ("switch", pos.size(), title), positions (pos)
{
    addAndMakeVisible (knob);
    knob.onValueChange = [this] { repaint(); };
}

void PatternSwitch::resized()
{
    const int box = 58 + 24;
    knob.setBounds (getLocalBounds().withSizeKeepingCentre (box, box));
}

void PatternSwitch::paint (juce::Graphics& g)
{
    const auto& f = currentFinish (*this);
    const auto c = getLocalBounds().toFloat().getCentre();
    const int active = juce::roundToInt (knob.getValue());

    g.setFont (fonts::label (9.0f).withExtraKerningFactor (0.1f));

    for (int i = 0; i < positions.size(); ++i)
    {
        const float a = (-135.0f + 270.0f * (float) i / (float) juce::jmax (1, positions.size() - 1)) * pi / 180.0f;
        const auto at = onCircle (c, 58.0f, a);
        g.setColour (i == active ? f.silk : f.silkMuted);
        g.drawText (positions[i].toUpperCase(), juce::Rectangle<float> (at.x - 30.0f, at.y - 6.0f, 60.0f, 12.0f), juce::Justification::centred, false);
    }
}
}
