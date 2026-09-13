#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "GUI/SP92/DeviceView.h"
#include "GUI/SP92/Sp92LookAndFeel.h"

namespace stabr
{
class StabrProcessor;

/*  El editor pone el LookAndFeel del SP-92, escala el aparato y le da el pulso
    de animacion en cada VBlank. Todo el panel vive en GUI/SP92/DeviceView.

    VBlankAttachment y no un Timer: sincroniza con el refresco real del
    monitor. Las piezas solo repintan cuando algo cambia. */
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
    void setFinish (int index);

    gui::sp92::DeviceView& getView() noexcept { return view; }
    int getFinish() const noexcept;

private:
    gui::sp92::Sp92LookAndFeel lookAndFeel;
    StabrProcessor& proc;
    gui::sp92::DeviceView view;
    juce::TooltipWindow tooltips { this, 700 };
    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StabrEditor)
};
}
