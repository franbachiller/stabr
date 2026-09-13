#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

#include "Palette.h"

namespace stabr::gui
{
/*  Medidor de salida L/R, de -48 a 0 dBFS. Lee picos que el hilo de audio
    deja en atomicos (StereoOutput::readAndClearMeters). */
class OutputMeter final : public juce::Component
{
public:
    OutputMeter()
    {
        setTitle ("Output level");
        setInterceptsMouseClicks (false, false);
    }

    void push (float peakL, float peakR)
    {
        auto normalise = [] (float x)
        {
            const float db = 20.0f * std::log10 (std::max (x, 1.0e-5f));
            return std::clamp ((db + 48.0f) / 48.0f, 0.0f, 1.0f);
        };

        // Caida de ~60 dB por segundo a 60 fps: se lee, no parpadea.
        const float l = std::max (normalise (peakL), levelL - 0.02f);
        const float r = std::max (normalise (peakR), levelR - 0.02f);

        if (std::abs (l - levelL) > 0.001f || std::abs (r - levelR) > 0.001f)
        {
            levelL = std::max (0.0f, l);
            levelR = std::max (0.0f, r);
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        auto bar = [&g] (juce::Rectangle<float> r, float level)
        {
            g.setColour (colours::raised);
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (level > 0.94f ? colours::accent : colours::accentSoft);
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * level), 2.0f);
        };

        bar (b.removeFromTop (b.getHeight() * 0.5f).reduced (0.0f, 1.5f), levelL);
        bar (b.reduced (0.0f, 1.5f), levelR);
    }

private:
    float levelL = 0.0f, levelR = 0.0f;
};
}
