#include "HardwareKnob.h"
#include "Anim.h"
#include "ButtonMotion.h"

#include <cmath>

namespace stabr::gui::sp92
{
namespace
{
    double realNow() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }
}

HardwareKnob::HardwareKnob (const juce::String& style, int ticks, const juce::String& title)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setWantsKeyboardFocus (true);
    setTitle (title);
    setMouseDragSensitivity (kDragPixels);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);

    // La sombra cae mas abajo que la caja: sin esto se veia cortada.
    setPaintingIsUnclipped (true);

    getProperties().set ("sp92Knob", style);
    getProperties().set ("ticks", ticks);
}

void HardwareKnob::focusGained (FocusChangeType) { repaintWithMargin (*this, 16); }
void HardwareKnob::focusLost (FocusChangeType)   { repaintWithMargin (*this, 16); }
void HardwareKnob::enablementChanged()           { repaintWithMargin (*this, 24); }

bool HardwareKnob::isDiscrete() const
{
    return getInterval() > 0.0 && (getMaximum() - getMinimum()) / getInterval() <= 48.0;
}

double HardwareKnob::positionAt (double now)
{
    const double target = valueToProportionOfLength (getValue());

    if (now < animStart)
        return animFrom;

    if (animDur <= 0.0)
        return target;

    const double t = (now - animStart) / animDur;
    return t >= 1.0 ? target : animFrom + (target - animFrom) * anim::easeOutCubic ((float) t);
}

void HardwareKnob::startAnimation (double start, double duration)
{
    if (shownPos < 0.0)
        return;

    animFrom = shownPos;
    animStart = start;
    animDur = duration;
    repaint();
}

void HardwareKnob::animateToCurrentValue (double now, double delay)
{
    lastNow = juce::jmax (lastNow, now);
    startAnimation (now + delay, kPresetSeconds);
}

void HardwareKnob::tick (double now)
{
    lastNow = now;

    if (now < animStart + animDur + 0.02)
        repaint();
}

void HardwareKnob::paint (juce::Graphics& g)
{
    const double pos = positionAt (lastNow);
    shownPos = pos;

    const auto rotary = getRotaryParameters();
    getLookAndFeel().drawRotarySlider (g, 0, 0, getWidth(), getHeight(), (float) pos,
                                       rotary.startAngleRadians, rotary.endAngleRadians, *this);
}

void HardwareKnob::valueChanged()
{
    const double norm = valueToProportionOfLength (getValue());
    const double jump = lastNorm < 0.0 ? 0.0 : std::abs (norm - lastNorm);
    lastNorm = norm;

    if (! touching)
        return;   // automatizacion o sonido nuevo: lo decide quien lo cambio

    const double now = realNow();
    lastNow = juce::jmax (lastNow, now);

    if (dragging && ! isDiscrete() && jump < 0.08)
    {
        animDur = 0.0;          // arrastrar es directo: la mano manda
        animStart = -10.0;
    }
    else
    {
        startAnimation (now, dragging ? (isDiscrete() ? kDetentSeconds : kJumpSeconds) : kStepSeconds);
    }

    if (onUserChange)
        onUserChange (*this);
}

void HardwareKnob::mouseDown (const juce::MouseEvent& e)
{
    touching = dragging = true;
    animDur = 0.0;
    animStart = -10.0;
    lastDragPos = virtualPos = e.position;
    juce::Slider::mouseDown (e);

    if (onUserChange)
        onUserChange (*this);
}

void HardwareKnob::mouseDrag (const juce::MouseEvent& e)
{
    // Posicion virtual: con Shift el mouse recorre menos, sin saltos al
    // apretarlo o soltarlo en medio del arrastre.
    const auto delta = e.position - lastDragPos;
    lastDragPos = e.position;
    virtualPos += delta * (e.mods.isShiftDown() ? (float) kDragPixels / (float) kFineDragPixels : 1.0f);

    juce::Slider::mouseDrag (e.withNewPosition (virtualPos));
}

void HardwareKnob::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e.withNewPosition (virtualPos));
    touching = dragging = false;
}

void HardwareKnob::mouseDoubleClick (const juce::MouseEvent& e)
{
    touching = true;
    juce::Slider::mouseDoubleClick (e);
}

void HardwareKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! isEnabled() || ! isScrollWheelEnabled())
    {
        juce::Slider::mouseWheelMove (e, w);
        return;
    }

    float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : w.deltaY;
    if (w.isReversed)
        d = -d;

    if (d == 0.0f)
        return;

    constexpr float notch = 120.0f / 256.0f;   // un clic de rueda en Windows
    touching = true;

    if (isDiscrete())
    {
        // Un paso por clic de rueda; los trackpads acumulan hasta completarlo.
        wheelAccum += d;
        int steps = 0;

        while (std::abs (wheelAccum) >= notch * 0.9f)
        {
            const int sign = wheelAccum > 0.0f ? 1 : -1;
            steps += sign;
            wheelAccum -= (float) sign * notch;
        }

        if (steps != 0)
            setValue (getValue() + steps * getInterval(), juce::sendNotificationSync);
    }
    else
    {
        const double pos = valueToProportionOfLength (getValue()) + d / notch * 0.025;
        setValue (proportionOfLengthToValue (juce::jlimit (0.0, 1.0, pos)), juce::sendNotificationSync);
    }

    touching = false;
}

bool HardwareKnob::keyPressed (const juce::KeyPress& key)
{
    touching = true;
    const bool used = juce::Slider::keyPressed (key);
    touching = false;
    return used;
}
}
