#include "StabrLookAndFeel.h"
#include "Palette.h"

namespace stabr::gui
{
StabrLookAndFeel::StabrLookAndFeel()
{
    using namespace colours;

    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, text);

    setColour (juce::ComboBox::backgroundColourId, raised);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, textMuted);
    setColour (juce::ComboBox::focusedOutlineColourId, accent);

    setColour (juce::PopupMenu::backgroundColourId, raised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textMuted);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId, background);

    setColour (juce::TextButton::buttonColourId, raised);
    setColour (juce::TextButton::buttonOnColourId, raised);
    setColour (juce::TextButton::textColourOffId, textMuted);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::ToggleButton::textColourId, text);

    setColour (juce::TooltipWindow::backgroundColourId, raised);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
}

//==============================================================================
void StabrLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    using namespace colours;

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float radius = juce::jmax (4.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f);
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float thickness = juce::jmax (2.0f, radius * 0.11f);
    const float alpha = slider.isEnabled() ? 1.0f : 0.4f;

    // Pista y arco de valor. Los bipolares (envolvente del filtro, offset)
    // dibujan el arco desde el centro: el signo es la informacion.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (line.withMultipliedAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                             juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent.withMultipliedAlpha (alpha));
        g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Cuerpo de la perilla.
    const float bodyRadius = radius * 0.74f;
    g.setColour (knobBody.withMultipliedAlpha (alpha));
    g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));
    g.setColour (knobEdge.withMultipliedAlpha (alpha));
    g.drawEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre), 1.2f);

    // Indicador.
    const auto tip   = centre.getPointOnCircumference (bodyRadius * 0.80f, angle);
    const auto inner = centre.getPointOnCircumference (bodyRadius * 0.25f, angle);
    g.setColour (text.withMultipliedAlpha (alpha));
    g.drawLine ({ inner, tip }, juce::jmax (2.0f, radius * 0.08f));

    if (slider.hasKeyboardFocus (false))
    {
        g.setColour (text.withAlpha (0.8f));
        g.drawEllipse (juce::Rectangle<float> ((radius + thickness) * 2.0f, (radius + thickness) * 2.0f).withCentre (centre), 1.2f);
    }
}

//==============================================================================
void StabrLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    using namespace colours;

    const auto b = button.getLocalBounds().toFloat().reduced (1.0f);
    const float alpha = button.isEnabled() ? 1.0f : 0.4f;
    const bool on = button.getToggleState();

    g.setColour ((highlighted ? raised.brighter (0.05f) : raised).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (b, 4.0f);

    // LED
    const float led = juce::jmin (10.0f, b.getHeight() - 10.0f);
    const auto ledBounds = juce::Rectangle<float> (led, led).withCentre ({ b.getX() + 8.0f + led * 0.5f, b.getCentreY() });

    if (on)
    {
        g.setColour (accent.withAlpha (0.22f * alpha));
        g.fillRoundedRectangle (ledBounds.expanded (3.0f), 4.0f);
    }

    g.setColour ((on ? accent : ledOff).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (ledBounds, 2.0f);

    g.setColour ((on ? text : textMuted).withMultipliedAlpha (alpha));
    g.setFont (fonts::label (12.0f));
    g.drawFittedText (button.getButtonText(),
                      b.withTrimmedLeft (led + 16.0f).withTrimmedRight (4.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.8f);

    if (button.hasKeyboardFocus (false))
    {
        g.setColour (text.withAlpha (0.8f));
        g.drawRoundedRectangle (b, 4.0f, 1.2f);
    }
}

//==============================================================================
void StabrLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    using namespace colours;

    const auto b = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
    const float alpha = box.isEnabled() ? 1.0f : 0.4f;

    g.setColour (raised.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (b, 4.0f);

    g.setColour ((box.hasKeyboardFocus (true) ? accent : line).withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    // Chevron.
    const float s = juce::jlimit (4.0f, 7.0f, (float) height * 0.16f);
    const juce::Point<float> c { b.getRight() - 12.0f, b.getCentreY() };
    juce::Path arrow;
    arrow.startNewSubPath (c.x - s, c.y - s * 0.5f);
    arrow.lineTo (c.x, c.y + s * 0.5f);
    arrow.lineTo (c.x + s, c.y - s * 0.5f);
    g.setColour (textMuted.withMultipliedAlpha (alpha));
    g.strokePath (arrow, juce::PathStrokeType (1.6f));
}

void StabrLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const auto font = getComboBoxFont (box);

    // Alto de UNA linea: con el alto entero de la caja, el Label parte un
    // texto largo ("Natural Minor") en dos renglones en vez de comprimirlo.
    const int lineHeight = juce::jmin (box.getHeight() - 2, juce::roundToInt (font.getHeight() * 1.4f));
    label.setBounds (4, (box.getHeight() - lineHeight) / 2, juce::jmax (0, box.getWidth() - 26), lineHeight);
    label.setFont (font);
    label.setMinimumHorizontalScale (0.7f);
}

juce::Font StabrLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    // El tamano lo decide el control (Selector), no el alto de la celda: si
    // no, todos los selectores de una misma fila salen con la letra del mas
    // importante.
    const float height = (float) box.getProperties().getWithDefault ("fontHeight", 13.0f);
    return fonts::label (height);
}

juce::Font StabrLookAndFeel::getPopupMenuFont()
{
    return fonts::label (14.0f);
}

//==============================================================================
void StabrLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool highlighted, bool down)
{
    using namespace colours;

    auto b = button.getLocalBounds().toFloat().reduced (1.0f);

    if ((bool) button.getProperties().getWithDefault ("tab", false))
    {
        if (button.getToggleState())
        {
            g.setColour (panel);
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (accent);
            g.fillRect (b.removeFromBottom (2.0f));
        }
        else if (highlighted)
        {
            g.setColour (panel.withAlpha (0.6f));
            g.fillRoundedRectangle (b, 4.0f);
        }
    }
    else
    {
        g.setColour (down ? line : (highlighted ? raised.brighter (0.05f) : raised));
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (button.getToggleState() ? accent : line);
        g.drawRoundedRectangle (b, 4.0f, 1.0f);
    }

    if (button.hasKeyboardFocus (false))
    {
        g.setColour (text.withAlpha (0.8f));
        g.drawRoundedRectangle (button.getLocalBounds().toFloat().reduced (1.0f), 4.0f, 1.2f);
    }
}

juce::Font StabrLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return fonts::title (juce::jlimit (11.0f, 14.0f, (float) buttonHeight * 0.42f));
}
}
