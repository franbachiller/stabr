#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "Preset.h"
#include "StabGenerator.h"

namespace stabr
{
class StabrProcessor;
}

namespace stabr::presets
{
/*  Presets de fabrica y de usuario, Create STAB, y el historial para deshacer.
    Todo se usa desde el hilo de mensajes; la generacion corre en un hilo de
    fondo y aplica el resultado de vuelta en el de mensajes.
*/
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);
    ~PresetManager();

    //==========================================================================
    const std::vector<Preset>& getFactoryPresets() const;
    void loadFactoryPreset (int index);

    static juce::File getDefaultUserDirectory();
    void setUserDirectory (const juce::File&);
    juce::File getUserDirectory() const { return userDirectory; }

    juce::Array<juce::File> getUserPresetFiles() const;
    bool loadUserPreset (const juce::File&);
    juce::Result saveUserPreset (const juce::String& name);

    juce::String getCurrentPresetName() const { return currentName; }
    void setCurrentPresetName (const juce::String& name) { currentName = name; }

    static constexpr const char* fileExtension = ".stabrpreset";

    //==========================================================================
    /*  Asincronico: vuelve enseguida, y el sonido nuevo se aplica cuando esta
        verificado. Ignora la llamada si ya hay una generacion en curso. */
    void createStab (GeneratorLocks, std::function<void (bool accepted)> onDone = {});

    /*  Sincronico, con semilla explicita. Para tests. */
    GeneratedStab createStabNow (juce::int64 seed, GeneratorLocks);

    bool isGenerating() const noexcept { return generating.load(); }

    //==========================================================================
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    void undo();
    void redo();

    /*  Se llama cuando cambia el sonido cargado (preset, stab, deshacer). */
    std::function<void()> onChange;

private:
    void applyWithHistory (const Preset&);
    void applyGenerated (const GeneratedStab&);
    std::vector<Preset> avoidList() const;
    StabrProcessor& analysisEngine();

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userDirectory;
    juce::String currentName { "Init" };

    std::vector<Preset> undoStack, redoStack, generated;

    /*  Procesador dedicado al analisis de Create STAB. Se crea recien la
        primera vez que se usa, y en el hilo de mensajes: ver D-047. */
    std::unique_ptr<StabrProcessor> engine;

    std::atomic<bool> generating { false };
    std::atomic<juce::int64> seedCounter { 0 };

    // Ultimo miembro: se destruye primero y espera al trabajo en curso, que
    // usa 'engine' y 'apvts'.
    juce::ThreadPool pool { 1 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (PresetManager)
    JUCE_DECLARE_NON_COPYABLE (PresetManager)
};
}
