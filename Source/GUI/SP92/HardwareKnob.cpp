#include "HardwareKnob.h"

#include <cmath>

namespace stabr::gui::sp92
{
HardwareKnob::HardwareKnob (const juce::String& style, int ticks, const juce::String& title)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setWantsKeyboardFocus (true);
    setTitle (title);
    setMouseDragSensitivity (style == "big" ? 260 : 200);

    getProperties().set ("sp92Knob", style);
    getProperties().set ("ticks", ticks);
}

void HardwareKnob::animateToCurrentValue (double now)
{
    if (shownPos < 0.0)
        return;

    animFrom = shownPos;
    animStart = now;
    lastNow = now;
    repaint();
}

void HardwareKnob::tick (double now)
{
    lastNow = now;

    if (now - animStart < kAnimSeconds + 0.05)
        repaint();
}

void HardwareKnob::paint (juce::Graphics& g)
{
    const double target = valueToProportionOfLength (getValue());
    double pos = target;
    const double t = (lastNow - animStart) / kAnimSeconds;

    if (t >= 0.0 && t < 1.0)
    {
        const double eased = 1.0 - std::pow (1.0 - t, 3.0);
        pos = animFrom + (target - animFrom) * eased;
    }

    shownPos = pos;

    const auto rotary = getRotaryParameters();
    getLookAndFeel().drawRotarySlider (g, 0, 0, getWidth(), getHeight(), (float) pos,
                                       rotary.startAngleRadians, rotary.endAngleRadians, *this);
}

void HardwareKnob::valueChanged()
{
    if (touching || hasKeyboardFocus (false))
    {
        animStart = -10.0;   // si la toca alguien, la animacion se corta

        if (onUserChange)
            onUserChange (*this);
    }
}

void HardwareKnob::mouseDown (const juce::MouseEvent& e)
{
    touching = true;
    animStart = -10.0;
    juce::Slider::mouseDown (e);

    if (onUserChange)
        onUserChange (*this);
}

void HardwareKnob::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e);
    touching = false;
}

void HardwareKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    touching = true;
    juce::Slider::mouseWheelMove (e, w);
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
