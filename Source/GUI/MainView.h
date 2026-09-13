#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>

#include "Controls.h"
#include "ChordVisualizer.h"
#include "DelayVisualizer.h"
#include "StepGrid.h"
#include "OutputMeter.h"
#include "PresetBar.h"

namespace stabr
{
class StabrProcessor;
}

namespace stabr::gui
{
/*  Toda la interfaz, disenada a tamano base. El editor la escala con una
    transformacion afin: como todo es vectorial, se ve nitida de 0.75x a 2x y
    el layout no tiene que saber nada de escalas.

    JERARQUIA (brief, seccion 7): lo que mas se toca va arriba y grande, siempre
    a la vista: sonidos y Create STAB, acorde y tonalidad, cutoff / resonancia
    / decay, el delay, y el secuenciador. Lo demas, en pestanas.
*/
class MainView final : public juce::Component
{
public:
    static constexpr int kBaseWidth  = 1100;
    static constexpr int kBaseHeight = 700;

    explicit MainView (StabrProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /*  Llamado en cada VBlank desde el editor. */
    void tick (double nowSeconds);

    void showPage (int index);
    int getNumPages() const noexcept { return (int) pages.size(); }

    void setScaleIndicator (float scale);
    std::function<void (float)> onScaleRequested;

    ChordVisualizer& getChordVisualizer() noexcept { return chordViz; }
    DelayVisualizer& getDelayVisualizer() noexcept { return delayViz; }
    StepGrid& getStepGrid() noexcept { return stepGrid; }

private:
    std::unique_ptr<ParamPanel> panel (const char* title, std::initializer_list<ControlSpec>);
    std::unique_ptr<juce::Component> control (Panel& parent, const ControlSpec&);
    float raw (const char* paramID) const;

    void buildPages();
    void refreshChordDetail();
    void refreshDelayInfo();

    StabrProcessor& proc;
    APVTS& apvts;

    // Cabecera
    Panel header;
    juce::Label logo, subtitle;
    PresetBar presetBar;
    juce::ComboBox sizeBox;
    OutputMeter meter;

    // Fila principal
    Panel chordPanel, tonePanel, delayPanel;
    std::unique_ptr<juce::Component> chordType, scaleKey, scaleMode;
    ChordVisualizer chordViz;
    std::unique_ptr<juce::Component> cutoff, resonance, ampDecay, envAmount, envDecay, drive;
    DelayVisualizer delayViz;
    std::unique_ptr<juce::Component> delayTime, delayMode, delayFeedback, delayMix;

    // Pestanas
    juce::OwnedArray<juce::TextButton> tabs;
    std::vector<std::unique_ptr<ParamPage>> pages;

    // Secuenciador
    Panel seqPanel;
    std::unique_ptr<juce::Component> seqOn, seqPattern, seqRate, seqLength, seqSwing, seqGate;
    StepGrid stepGrid;

    // Cache para no reconstruir textos en cada cuadro
    std::array<int, 3> chordDetailKey { -1, -1, -1 };
    std::array<int, 8> delayCaptionKey { -1, -1, -1, -1, -1, -1, -1, -1 };
    juce::String delayCaption;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
}
