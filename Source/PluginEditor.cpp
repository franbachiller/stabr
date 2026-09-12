#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Common/Utf8.h"

namespace stabr
{
StabrEditor::StabrEditor (StabrProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    /*  El numero de version sale del CMake, no de un literal: la primera
        version de esto decia "fase 1" hardcodeado y quedo mintiendo despues de
        la Fase 2. Un texto que hay que acordarse de actualizar a mano siempre
        termina desactualizado. */
    title.setText (utf8 ("STABR  \xc2\xb7  v") + JucePlugin_VersionString
                       + utf8 ("  \xc2\xb7  UI placeholder"),
                   juce::dontSendNotification);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    for (auto* param : proc.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr)
            continue;

        auto row = std::make_unique<Row>();

        row->label.setText (ranged->getName (32), juce::dontSendNotification);
        row->label.setJustificationType (juce::Justification::centredRight);

        row->slider.setSliderStyle (juce::Slider::LinearHorizontal);
        row->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 20);
        row->slider.setTitle (ranged->getName (32));   // nombre accesible

        rowHolder.addAndMakeVisible (row->label);
        rowHolder.addAndMakeVisible (row->slider);

        row->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.getAPVTS(), ranged->getParameterID(), row->slider);

        rows.push_back (std::move (row));
    }

    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setResizable (true, true);
    setResizeLimits (420, 260, 900, 800);
    setSize (560, 420);
}

void StabrEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141416));
    g.setColour (juce::Colour (0xff2a2a2e));
    g.drawHorizontalLine (40, 0.0f, (float) getWidth());
}

void StabrEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    title.setBounds (area.removeFromTop (30));
    area.removeFromTop (6);
    viewport.setBounds (area);

    constexpr int rowHeight = 30;
    rowHolder.setSize (juce::jmax (0, viewport.getWidth() - 12),
                       (int) rows.size() * rowHeight);

    int y = 0;
    for (auto& row : rows)
    {
        auto r = juce::Rectangle<int> (0, y, rowHolder.getWidth(), rowHeight).reduced (0, 3);
        row->label.setBounds (r.removeFromLeft (140));
        r.removeFromLeft (8);
        row->slider.setBounds (r);
        y += rowHeight;
    }
}
}
