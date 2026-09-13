#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/VoiceManager.h"
#include "Common/SilenceGate.h"
#include "Chord/ChordGenerator.h"
#include "Chord/NoteRouter.h"
#include "Params/ParamCache.h"
#include "FX/FxChain.h"
#include "Sequencer/StepSequencer.h"
#include "Sequencer/NoteLatch.h"
#include "Common/GuiFeed.h"
#include "Presets/PresetManager.h"

namespace stabr
{
class StabrProcessor : public juce::AudioProcessor,
                       private juce::Timer
{
public:
    StabrProcessor();
    ~StabrProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    /*  Para el visualizador de acordes de la Fase 6. */
    chord::ChordResult getSoundingNotes() const noexcept { return voices.getSoundingNotes(); }
    fx::FxChain& getFxChain() noexcept { return fxChain; }

    /*  Paso del secuenciador sonando ahora (-1 = parado). Es un atomico: la GUI
        lo puede leer directo. */
    int getSequencerStep() const noexcept { return sequencer.getCurrentStep(); }

    /*  Presets y Create STAB. Solo desde el hilo de mensajes. */
    presets::PresetManager& getPresetManager() noexcept { return presetManager; }

    /*  Canal lock-free hacia la GUI: eventos de disparo y tiempos del delay. */
    GuiFeed& getGuiFeed() noexcept { return guiFeed; }

    /*  Escala de la interfaz. Atomico y no una propiedad del ValueTree: hay
        hosts que guardan el estado desde otro hilo mientras la GUI cambia de
        tamano. Se escribe en el estado al guardar. */
    float getUiScale() const noexcept     { return uiScale.load (std::memory_order_relaxed); }
    void  setUiScale (float s) noexcept   { uiScale.store (juce::jlimit (0.75f, 2.0f, s), std::memory_order_relaxed); }

    /*  Estado de la interfaz del SP-92 que se guarda con el proyecto: acabado,
        candados de Create STAB (bits: 1 acorde, 2 efectos, 4 secuenciador) y
        el texto de la cinta. */
    int  getUiFinish() const noexcept     { return uiFinish.load (std::memory_order_relaxed); }
    void setUiFinish (int f) noexcept     { uiFinish.store (juce::jlimit (0, 2, f), std::memory_order_relaxed); }
    int  getUiLocks() const noexcept      { return uiLocks.load (std::memory_order_relaxed); }
    void setUiLocks (int bits) noexcept   { uiLocks.store (bits & 7, std::memory_order_relaxed); }

    juce::String getTapeText() const
    {
        const juce::ScopedLock sl (uiTextLock);
        return tapeText;
    }

    void setTapeText (const juce::String& text)
    {
        const juce::ScopedLock sl (uiTextLock);
        tapeText = text;
    }

    /*  Solo para tests y mediciones: no llamar desde la GUI sin un FIFO. */
    int getActiveVoiceCount() const noexcept { return voices.getActiveVoiceCount(); }
    int getOscillatorCount()  const noexcept { return voices.getOscillatorCount(); }

    /*  Disparo del secuenciador para el motor de voces. No es una nota MIDI
        (esas van de 0 a 127), asi que soltar una tecla nunca puede apagar el
        acorde que dispara el secuenciador, ni al reves. */
    static constexpr int kSequencerTrigger = 1000;

    /*  Octava de la fundamental cuando el secuenciador no tiene nada latcheado:
        la tonica de la tonalidad en la octava 3 (La -> A3, nota 57). */
    static constexpr int kFallbackRootBase = 48;

private:
    void timerCallback() override;
    void applyEvent (const NoteRouter::Event&, bool sequencerRunning) noexcept;
    void applySequencerEvent (const seq::StepSequencer::Event&) noexcept;
    void triggerSequencedChord (float velocity) noexcept;
    void updateParametersForBlock() noexcept;
    void publishToGui() noexcept;

    GuiFeed guiFeed;
    chord::ChordResult lastPublishedNotes;
    int triggersThisBlock = 0;
    float lastTriggerVelocity = 0.0f;
    std::atomic<float> uiScale { 1.0f };
    std::atomic<int> uiFinish { 0 };
    std::atomic<int> uiLocks { 0 };

    // Lock y no atomico: un String no es atomico. Nunca se toca desde audio.
    mutable juce::CriticalSection uiTextLock;
    juce::String tapeText;

    juce::AudioProcessorValueTreeState apvts;
    presets::PresetManager presetManager { apvts };

    ParamCache p;

    VoiceManager voices;
    NoteRouter router;
    fx::FxChain fxChain;
    fx::FxParams fxParams;
    NoteRouter::EventList events;
    chord::ChordParams chordParams;
    VoiceParams voiceParams;

    seq::StepSequencer sequencer;
    seq::SeqParams seqParams;
    seq::StepSequencer::EventList sequencerEvents;
    seq::NoteLatch latch;
    seq::Transport transport;
    bool sequencerWasRunning = false;

    SilenceGate silenceGate;
    juce::SmoothedValue<float> masterGainSmoothed;

    float currentBendNormalised = 0.0f;   // -1..1

    // Ultimo tempo valido del host: si deja de informarlo, se sigue usando
    // este en vez de caer a cero y hacer explotar las divisiones.
    double lastKnownBpm = 120.0;

    /*  El modo Auto introduce latencia y hay que declararsela al host, pero
        setLatencySamples() no se puede llamar desde el hilo de audio. El
        procesador publica el valor deseado en un atomico y un Timer en el hilo
        de mensajes lo aplica. Ver DECISIONS.md D-012. */
    std::atomic<int> desiredLatency { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StabrProcessor)
};
}
