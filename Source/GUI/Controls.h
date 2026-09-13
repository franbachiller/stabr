#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <initializer_list>
#include <memory>
#include <vector>

namespace stabr::gui
{
using APVTS = juce::AudioProcessorValueTreeState;

enum class ControlSize { small, medium, large };

struct ControlSpec
{
    const char* paramID = nullptr;
    const char* label = nullptr;          // nullptr = nombre del parametro
    ControlSize size = ControlSize::small;
};

/*  Todo control atado a un parametro lleva su ID en esta propiedad. El test de
    la GUI la usa para verificar que ningun parametro quede sin control. */
inline const juce::Identifier paramIdProperty { "stabrParamID" };

//==============================================================================
class Knob final : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& paramID, const juce::String& label, ControlSize);

    void resized() override;
    void focusOfChildComponentChanged (FocusChangeType) override { slider.repaint(); }

    juce::Slider& getSlider() noexcept { return slider; }

private:
    void refreshValueText();

    ControlSize size;
    juce::Slider slider;
    juce::Label nameLabel, valueLabel;
    std::unique_ptr<APVTS::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

class Selector final : public juce::Component
{
public:
    Selector (APVTS&, const juce::String& paramID, const juce::String& label, ControlSize);

    void resized() override;

    juce::ComboBox& getComboBox() noexcept { return box; }

private:
    juce::ComboBox box;
    juce::Label nameLabel;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Selector)
};

class LedToggle final : public juce::Component
{
public:
    LedToggle (APVTS&, const juce::String& paramID, const juce::String& label);

    void resized() override;

private:
    juce::ToggleButton button;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LedToggle)
};

/*  El tipo de control sale del tipo de parametro: Choice -> selector,
    Bool -> LED, el resto -> perilla. Asi un parametro nuevo no necesita
    codigo de GUI, solo aparecer en una lista. */
std::unique_ptr<juce::Component> makeControl (APVTS&, const ControlSpec&);

juce::Point<int> preferredSize (const juce::Component&);

//==============================================================================
/*  Recuadro con titulo. */
class Panel : public juce::Component
{
public:
    explicit Panel (const juce::String& title = {});

    void paint (juce::Graphics&) override;
    juce::Rectangle<int> getContentBounds() const;

    static constexpr int kPadding = 8;
    static constexpr int kTitleHeight = 16;

private:
    juce::String title;
};

/*  Panel de parametros secundarios, armado desde una lista de IDs. */
class ParamPanel final : public Panel
{
public:
    ParamPanel (APVTS&, const juce::String& title, std::initializer_list<ControlSpec>);

    void resized() override;

    int getPreferredWidth() const;
    int getMinimumWidth() const;

    static constexpr int kItemGap = 4;

private:
    std::vector<std::unique_ptr<juce::Component>> controls;
};

/*  Una pestana: paneles en fila que se reparten el ancho. */
class ParamPage final : public juce::Component
{
public:
    void addPanel (std::unique_ptr<ParamPanel>);
    void resized() override;

private:
    std::vector<std::unique_ptr<ParamPanel>> panels;
};
}
