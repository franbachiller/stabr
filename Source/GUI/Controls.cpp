#include "Controls.h"
#include "Palette.h"
#include "../Common/Utf8.h"

namespace stabr::gui
{
namespace
{
    const juce::Identifier prefWidthProperty  { "stabrPrefW" };
    const juce::Identifier prefHeightProperty { "stabrPrefH" };

    void setPreferredSize (juce::Component& c, int w, int h)
    {
        c.getProperties().set (prefWidthProperty, w);
        c.getProperties().set (prefHeightProperty, h);
    }

    void styleNameLabel (juce::Label& l, const juce::String& text, float height)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (fonts::label (height));
        l.setColour (juce::Label::textColourId, colours::textMuted);
        l.setInterceptsMouseClicks (false, false);
        l.setMinimumHorizontalScale (0.8f);
    }
}

juce::Point<int> preferredSize (const juce::Component& c)
{
    return { (int) c.getProperties().getWithDefault (prefWidthProperty, 64),
             (int) c.getProperties().getWithDefault (prefHeightProperty, 80) };
}

//==============================================================================
Knob::Knob (APVTS& apvts, const juce::String& paramID, const juce::String& label, ControlSize s)
    : size (s)
{
    auto* param = apvts.getParameter (paramID);
    jassert (param != nullptr);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setWantsKeyboardFocus (true);
    slider.setTitle (label);
    slider.setMouseDragSensitivity (s == ControlSize::large ? 260 : 200);
    addAndMakeVisible (slider);

    const bool small = s == ControlSize::small;
    styleNameLabel (nameLabel, label, small ? 11.5f : 13.0f);
    addAndMakeVisible (nameLabel);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (fonts::value (small ? 11.0f : 12.5f));
    valueLabel.setColour (juce::Label::textColourId, colours::text);
    valueLabel.setInterceptsMouseClicks (false, false);
    valueLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (valueLabel);

    attachment = std::make_unique<APVTS::SliderAttachment> (apvts, paramID, slider);

    if (param != nullptr)
    {
        slider.setDescription (param->getName (64));

        const auto range = param->getNormalisableRange();
        slider.getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f);

        // Doble clic vuelve al default, como en cualquier hardware con memoria.
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    }

    slider.onValueChange = [this] { refreshValueText(); };
    refreshValueText();

    setTitle (label);

    switch (s)
    {
        case ControlSize::large:  setPreferredSize (*this, 116, 134); break;
        case ControlSize::medium: setPreferredSize (*this, 88, 106);  break;
        case ControlSize::small:
        default:                  setPreferredSize (*this, 64, 80);   break;
    }
}

void Knob::refreshValueText()
{
    valueLabel.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    const int textHeight = size == ControlSize::small ? 14 : 17;

    valueLabel.setBounds (b.removeFromBottom (textHeight));
    nameLabel.setBounds (b.removeFromBottom (textHeight));

    const int diameter = juce::jmin (b.getWidth(), b.getHeight());
    slider.setBounds (b.withSizeKeepingCentre (diameter, diameter));
}

//==============================================================================
Selector::Selector (APVTS& apvts, const juce::String& paramID, const juce::String& label, ControlSize s)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID));
    jassert (choice != nullptr);

    // Los items tienen que existir ANTES de la attachment, o arranca vacio.
    if (choice != nullptr)
        box.addItemList (choice->choices, 1);

    box.setTitle (label);
    box.getProperties().set ("fontHeight", s == ControlSize::large ? 19.0f : (s == ControlSize::medium ? 15.0f : 13.0f));
    addAndMakeVisible (box);

    styleNameLabel (nameLabel, label, s == ControlSize::small ? 11.5f : 13.0f);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (nameLabel);

    attachment = std::make_unique<APVTS::ComboBoxAttachment> (apvts, paramID, box);

    setTitle (label);

    switch (s)
    {
        case ControlSize::large:  setPreferredSize (*this, 180, 62); break;
        case ControlSize::medium: setPreferredSize (*this, 140, 52); break;
        case ControlSize::small:
        default:                  setPreferredSize (*this, 118, 46); break;
    }
}

void Selector::resized()
{
    auto b = getLocalBounds();
    nameLabel.setBounds (b.removeFromTop (14));
    b.removeFromTop (2);
    box.setBounds (b);
}

//==============================================================================
LedToggle::LedToggle (APVTS& apvts, const juce::String& paramID, const juce::String& label)
{
    button.setButtonText (label);
    button.setTitle (label);
    addAndMakeVisible (button);

    attachment = std::make_unique<APVTS::ButtonAttachment> (apvts, paramID, button);

    setTitle (label);
    setPreferredSize (*this, 104, 28);
}

void LedToggle::resized()
{
    button.setBounds (getLocalBounds());
}

//==============================================================================
std::unique_ptr<juce::Component> makeControl (APVTS& apvts, const ControlSpec& spec)
{
    auto* param = apvts.getParameter (spec.paramID);
    jassert (param != nullptr);

    const juce::String label = spec.label != nullptr ? utf8 (spec.label)
                                                     : (param != nullptr ? param->getName (32) : juce::String (spec.paramID));

    std::unique_ptr<juce::Component> control;

    if (dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr)
        control = std::make_unique<Selector> (apvts, spec.paramID, label, spec.size);
    else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
        control = std::make_unique<LedToggle> (apvts, spec.paramID, label);
    else
        control = std::make_unique<Knob> (apvts, spec.paramID, label, spec.size);

    control->getProperties().set (paramIdProperty, juce::String (spec.paramID));
    return control;
}

//==============================================================================
Panel::Panel (const juce::String& t) : title (t)
{
    setInterceptsMouseClicks (false, true);
}

void Panel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    if (title.isNotEmpty())
    {
        g.setColour (colours::textMuted);
        g.setFont (fonts::title (11.0f));
        g.drawText (title.toUpperCase(),
                    getLocalBounds().reduced (kPadding + 2, 0).withTrimmedTop (4).removeFromTop (kTitleHeight),
                    juce::Justification::centredLeft, true);
    }
}

juce::Rectangle<int> Panel::getContentBounds() const
{
    auto b = getLocalBounds().reduced (kPadding);

    if (title.isNotEmpty())
        b.removeFromTop (kTitleHeight);

    return b;
}

//==============================================================================
ParamPanel::ParamPanel (APVTS& apvts, const juce::String& t, std::initializer_list<ControlSpec> specs)
    : Panel (t)
{
    for (const auto& spec : specs)
    {
        controls.push_back (makeControl (apvts, spec));
        addAndMakeVisible (*controls.back());
    }
}

int ParamPanel::getPreferredWidth() const
{
    int w = 0;

    for (const auto& c : controls)
        w += preferredSize (*c).x + kItemGap;

    return w + 2 * kPadding;
}

int ParamPanel::getMinimumWidth() const
{
    int w = 0;

    for (const auto& c : controls)
        w = juce::jmax (w, preferredSize (*c).x);

    return w + kItemGap + 2 * kPadding;
}

void ParamPanel::resized()
{
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.flexWrap = juce::FlexBox::Wrap::wrap;
    fb.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    fb.alignContent = juce::FlexBox::AlignContent::flexStart;
    fb.alignItems = juce::FlexBox::AlignItems::flexStart;

    for (const auto& c : controls)
    {
        const auto p = preferredSize (*c);
        fb.items.add (juce::FlexItem (*c)
                          .withWidth ((float) p.x)
                          .withHeight ((float) p.y)
                          .withMargin (juce::FlexItem::Margin (0.0f, (float) kItemGap, (float) kItemGap, 0.0f)));
    }

    fb.performLayout (getContentBounds());
}

//==============================================================================
void ParamPage::addPanel (std::unique_ptr<ParamPanel> panel)
{
    addAndMakeVisible (*panel);
    panels.push_back (std::move (panel));
}

void ParamPage::resized()
{
    constexpr float gap = 8.0f;

    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.flexWrap = juce::FlexBox::Wrap::noWrap;
    fb.alignItems = juce::FlexBox::AlignItems::stretch;

    for (size_t i = 0; i < panels.size(); ++i)
    {
        auto& p = *panels[i];
        fb.items.add (juce::FlexItem (p)
                          .withFlex (1.0f, 1.0f, (float) p.getPreferredWidth())
                          .withMinWidth ((float) p.getMinimumWidth())
                          .withMargin (juce::FlexItem::Margin (0.0f, i + 1 < panels.size() ? gap : 0.0f, 0.0f, 0.0f)));
    }

    fb.performLayout (getLocalBounds());
}
}
