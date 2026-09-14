#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "ButtonMotion.h"
#include "ChassisLayer.h"
#include "DotMatrix.h"
#include "Hardware.h"
#include "HardwareKnob.h"
#include "../StepGrid.h"
#include "../../Params/ParamCache.h"
#include "../../Chord/ChordTypes.h"

namespace stabr
{
class StabrProcessor;

namespace presets { class PresetManager; }
}

namespace stabr::gui::sp92
{
/*  Todo componente que ocupa lugar en el panel lleva esta propiedad: el test de
    layout verifica que ninguno se salga del aparato ni se pise con otro. */
inline const juce::Identifier layoutItemProperty { "stabrLayoutItem" };

/*  El parametro atado a un control. */
inline const juce::Identifier paramIdProperty { "stabrParamID" };

/*  El SP-92 entero, disenado a 1100 x 780. El editor lo escala con una
    transformacion afin. Ver DECISIONS.md D-049 y D-050. */
class DeviceView final : public juce::Component
{
public:
    static constexpr int kBaseWidth  = 1100;
    static constexpr int kBaseHeight = 780;

    explicit DeviceView (StabrProcessor&);
    ~DeviceView() override;

    void resized() override;

    /*  Llamado en cada VBlank desde el editor: el pulso de todo lo que se mueve. */
    void tick (double nowSeconds);

    void setFinish (int index);

    std::function<void (int)> onFinishRequested;
    std::function<void (float)> onScaleRequested;

    /*  Pagina y subpagina del LCD; engancha los 6 encoders a sus parametros. */
    void selectPage (int page, int subPage);
    int getPage() const noexcept    { return page; }
    int getSubPage() const noexcept { return subPage; }

    /*  Todos los parametros que se pueden tocar desde el panel, en cualquier
        pagina. */
    juce::StringArray getControlledParameterIDs() const;

    /*  Cada pieza visible del panel con el lugar que ocupa a la vista (textos
        medidos, perillas como circulo con sus marcas, teclas, pantallas). El
        test de separaciones exige aire entre todas. */
    struct VisualItem
    {
        juce::String name;
        juce::Rectangle<float> area;
        bool round = false;
    };

    std::vector<VisualItem> getVisualItems() const;

    juce::String getEncoderParamID (int index) const;
    StepGrid& getStepGrid() noexcept { return steps; }
    DotMatrix& getLcd() noexcept { return lcd; }
    juce::String getLcdChordName() const { return shownChordName; }

    /*  Retardo del giro al cargar un sonido: las perillas arrancan de
        izquierda a derecha, como una ola. */
    static constexpr double kStaggerSeconds = 0.07;

private:
    HardwareKnob& addKnob (const char* paramID, const juce::String& silk, const juce::String& style, int ticks,
                           float cx, float cy, float diameter);
    void place (juce::Component&, int x, int y, int w, int h);
    void setupKey (juce::TextButton&, const juce::String& text, bool dark, bool led, const juce::String& title);

    void bindEncoders();
    void onSoundChanged();
    void updateDisk();
    void syncLights();

    void stepSound (int delta);
    void showSoundMenu();
    void showSetupMenu();
    void saveSound();

    void setParam (const char* paramID, float plainValue);
    float raw (const char* paramID) const;

    void drawMainLcd (double now);
    void drawVoicing (const chord::ChordResult&, double now);
    void drawGraph();
    void drawCells();
    void drawEchoLcd (double now);

    StabrProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    presets::PresetManager& presets;
    ParamCache cache;

    ChassisLayer chassis;

    PowerButton power;
    FloppyDrive disk;
    juce::TextButton diskPrev, diskNext, diskSave;

    DotMatrix lcd { 190, 72 };
    std::array<juce::TextButton, 7> softKeys;
    std::array<std::unique_ptr<HardwareKnob>, 6> encoders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> encoderAttachments;

    std::vector<std::unique_ptr<HardwareKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;

    DotMatrix echo { 96, 22, juce::BorderSize<int> (5, 6, 5, 6) };
    std::array<juce::TextButton, 3> modeKeys;
    VuMeter vu;

    SectionHead seqHead;
    StepGrid steps;
    juce::TextButton seqKey;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqAttachment;
    PatternSwitch patternSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> patternAttachment;

    StabPad stab;
    juce::TextButton undoKey, redoKey;
    std::array<std::unique_ptr<BatSwitch>, 3> bats;
    TapeLabel tape;

    // Ultimo miembro a proposito: se destruye antes que los botones que escucha.
    ButtonMotion motion;

    struct Overlay
    {
        juce::String label;
        juce::String paramID;
        double until = -10.0;
    };

    int page = 0, subPage = 0;
    Overlay overlay;
    double lastNow = 0.0, lastTick = 0.0, generateStarted = -10.0, lastOnset = -10.0;
    chord::ChordResult lastNotes;
    juce::String shownChordName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceView)
};
}
