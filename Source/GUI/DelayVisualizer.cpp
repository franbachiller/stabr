#include "DelayVisualizer.h"
#include "Palette.h"
#include "../FX/Delay.h"

#include <cmath>

namespace stabr::gui
{
DelayVisualizer::DelayVisualizer()
{
    setTitle ("Delay display");
    setInterceptsMouseClicks (false, false);
    numEchoes = computeEchoes (info, echoes);
}

int DelayVisualizer::computeEchoes (const DelayViewInfo& in, std::array<Echo, kMaxEchoes>& out) noexcept
{
    constexpr float kAudible = 0.03f;   // -30 dB respecto de la primera repeticion

    const float fb = in.freeze ? 0.999f : juce::jlimit (0.0f, 0.98f, in.feedback);
    const float tL = juce::jmax (0.001f, in.timeL);
    const float tR = juce::jmax (0.001f, in.timeR);
    int n = 0;

    if (in.mode == (int) fx::DelayMode::pingPong)
    {
        // Entra por la izquierda y la realimentacion cruza: L, R, L, R...
        double t = 0.0;

        for (int k = 0; n < kMaxEchoes; ++k)
        {
            const float gain = std::pow (fb, (float) k);

            if (gain < kAudible)
                break;

            const int channel = k % 2;
            t += channel == 0 ? tL : tR;
            out[(size_t) n++] = { (float) t, channel, gain };
        }
    }
    else
    {
        for (int k = 0; n + 1 < kMaxEchoes; ++k)
        {
            const float gain = std::pow (fb, (float) k);

            if (gain < kAudible)
                break;

            out[(size_t) n++] = { (float) (k + 1) * tL, 0, gain };
            out[(size_t) n++] = { (float) (k + 1) * tR, 1, gain };
        }
    }

    return n;
}

float DelayVisualizer::windowSeconds() const noexcept
{
    float maxTime = 0.0f;

    for (int i = 0; i < numEchoes; ++i)
        maxTime = juce::jmax (maxTime, echoes[(size_t) i].time);

    return juce::jmax (0.2f, maxTime * 1.08f);
}

void DelayVisualizer::setInfo (const DelayViewInfo& newInfo, const juce::String& newCaption)
{
    if (newInfo == info && newCaption == caption)
        return;

    info = newInfo;
    caption = newCaption;
    numEchoes = computeEchoes (info, echoes);
    setDescription (caption);
    dirty = true;
}

void DelayVisualizer::onset (float vel, double t)
{
    onsetTime = t;
    velocity = juce::jlimit (0.2f, 1.0f, vel);
    dirty = true;
}

void DelayVisualizer::tick (double t)
{
    now = t;

    const bool animating = (t - onsetTime) < (double) windowSeconds() + 0.8;

    if (animating || dirty || wasAnimating)
        repaint();

    wasAnimating = animating;
    dirty = false;
}

void DelayVisualizer::paint (juce::Graphics& g)
{
    using namespace colours;

    auto b = getLocalBounds().toFloat();

    g.setFont (fonts::value (11.0f));
    g.setColour (textMuted);
    g.drawText (caption, b.removeFromTop (16.0f), juce::Justification::centredLeft, true);

    const auto area = b.withTrimmedTop (4.0f);
    g.setColour (background);
    g.fillRoundedRectangle (area, 4.0f);

    const float enabledAlpha = info.enabled ? 1.0f : 0.35f;
    const float laneL = area.getY() + area.getHeight() * 0.3f;
    const float laneR = area.getY() + area.getHeight() * 0.7f;
    const float x0 = area.getX() + 26.0f;
    const float x1 = area.getRight() - 12.0f;
    const float window = windowSeconds();
    const float elapsed = (float) (now - onsetTime);

    g.setColour (line);
    g.drawHorizontalLine (juce::roundToInt (laneL), x0, x1);
    g.drawHorizontalLine (juce::roundToInt (laneR), x0, x1);

    g.setColour (textMuted);
    g.drawText ("L", juce::Rectangle<float> (area.getX() + 6.0f, laneL - 7.0f, 14.0f, 14.0f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (area.getX() + 6.0f, laneR - 7.0f, 14.0f, 14.0f), juce::Justification::centred);

    // El golpe seco, en los dos canales.
    const float dryFlash = elapsed >= 0.0f ? std::exp (-elapsed / 0.12f) * velocity : 0.0f;
    g.setColour (text.withAlpha ((0.25f + 0.75f * dryFlash) * enabledAlpha));
    g.drawVerticalLine (juce::roundToInt (x0), laneL, laneR);
    g.fillEllipse (x0 - 4.0f, laneL - 4.0f, 8.0f, 8.0f);
    g.fillEllipse (x0 - 4.0f, laneR - 4.0f, 8.0f, 8.0f);

    const float mixScale = 0.45f + 0.55f * juce::jmin (1.0f, info.mix * 2.0f);
    const bool dub = info.mode == (int) fx::DelayMode::dub;

    for (int i = 0; i < numEchoes; ++i)
    {
        const auto& e = echoes[(size_t) i];
        const float x = x0 + e.time / window * (x1 - x0);
        const float y = e.channel == 0 ? laneL : laneR;
        const float r = juce::jmax (2.5f, (3.0f + 8.0f * e.gain) * mixScale);

        auto colour = e.channel == 0 ? accent : accentSoft;

        // En dub cada vuelta pierde brillo y color, como la cinta.
        if (dub)
            colour = colour.interpolatedWith (textMuted, 1.0f - e.gain);

        g.setColour (colour.withAlpha (0.35f * enabledAlpha));
        g.drawEllipse (x - r, y - r, 2.0f * r, 2.0f * r, 1.0f);

        if (elapsed >= e.time)
        {
            float brightness = std::exp (-(elapsed - e.time) / 0.5f) * velocity;

            if (info.freeze)
                brightness = juce::jmax (brightness, 0.5f);

            g.setColour (colour.withAlpha (juce::jlimit (0.0f, 1.0f, brightness) * enabledAlpha));
            g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
        }
    }

    if (elapsed >= 0.0f && elapsed < window)
    {
        g.setColour (text.withAlpha (0.3f));
        g.drawVerticalLine (juce::roundToInt (x0 + elapsed / window * (x1 - x0)), area.getY() + 4.0f, area.getBottom() - 4.0f);
    }

    if (! info.enabled)
    {
        g.setColour (textMuted);
        g.setFont (fonts::title (12.0f));
        g.drawText ("OFF", area, juce::Justification::centred);
    }
}
}
