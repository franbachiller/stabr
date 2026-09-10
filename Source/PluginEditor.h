#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>
#include <memory>

namespace stabr
{
class StabrProcessor;

/*  EDITOR PLACEHOLDER DE FASE 1.

    Es deliberadamente feo y generico. Existe solo para poder tocar los
    parametros mientras se desarrolla el DSP y para que pluginval tenga una
    ventana que abrir y cerrar. Se tira entero en Fase 6, donde arranca la
    identidad visual real (LookAndFeel propio, vectorial, sin rotarys estandar).
*/
class StabrEditor : public juce::AudioProcessorEditor
{
public:
    explicit StabrEditor (StabrProcessor&);
    ~StabrEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    StabrProcessor& proc;
    juce::Label title;
    juce::Viewport viewport;
    juce::Component rowHolder;
    std::vector<std::unique_ptr<Row>> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StabrEditor)
};
}
