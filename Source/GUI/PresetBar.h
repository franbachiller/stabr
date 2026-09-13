#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Presets/PresetManager.h"

namespace stabr::gui
{
/*  Controles fuera de parametros que igual cuentan para el layout y la
    accesibilidad: los tests de la GUI los revisan con esta propiedad. */
inline const juce::Identifier layoutItemProperty { "stabrLayoutItem" };

/*  Cabecera de sonidos: navegador de presets, guardar, CREATE STAB, deshacer y
    los candados de lo que Create STAB no toca. */
class PresetBar final : public juce::Component
{
public:
    static constexpr int kNumControls = 10;

    explicit PresetBar (presets::PresetManager&);
    ~PresetBar() override;

    void resized() override;

    /*  Llamado en cada VBlank: refleja el nombre cargado, el historial y si hay
        una generacion en curso. */
    void tick();

private:
    void rebuildList();
    void loadSelected();
    void step (int delta);
    void save();
    void create();

    presets::PresetManager& manager;

    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    juce::ComboBox presetBox;
    juce::TextButton createButton { "CREATE STAB" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::Label lockLabel;
    juce::ToggleButton lockChord { "Chord" }, lockFx { "FX" }, lockSeq { "Seq" };

    juce::Array<juce::File> userFiles;
    juce::String shownName;
    bool lastGenerating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
}
