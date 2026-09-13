#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "GUI/MainView.h"
#include "GUI/StabrLookAndFeel.h"

namespace stabr
{
class StabrProcessor;

/*  El editor solo hace tres cosas: pone el LookAndFeel, escala la vista, y
    le da el pulso de animacion en cada VBlank. Toda la interfaz vive en
    GUI/MainView.

    VBlankAttachment y no un Timer: sincroniza con el refresco real del
    monitor, y el Timer a 60 Hz repintando todo es exactamente lo que el brief
    pide evitar. Los visualizadores ademas solo repintan si hay animacion.
*/
class StabrEditor final : public juce::AudioProcessorEditor
{
public:
    static constexpr float kMinScale = 0.75f;
    static constexpr float kMaxScale = 2.0f;

    explicit StabrEditor (StabrProcessor&);
    ~StabrEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setUiScale (float scale);
    gui::MainView& getView() noexcept { return view; }

private:
    gui::StabrLookAndFeel lookAndFeel;
    StabrProcessor& proc;
    gui::MainView view;
    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StabrEditor)
};
}
