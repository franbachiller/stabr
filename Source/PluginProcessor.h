#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/VoiceManager.h"
#include "Common/SilenceGate.h"
#include "Chord/ChordGenerator.h"
#include "Chord/NoteRouter.h"

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

    /*  Solo para tests y mediciones: no llamar desde la GUI sin un FIFO. */
    int getActiveVoiceCount() const noexcept { return voices.getActiveVoiceCount(); }

private:
    void timerCallback() override;
    void applyEvent (const NoteRouter::Event&) noexcept;
    void updateParametersForBlock() noexcept;

    juce::AudioProcessorValueTreeState apvts;

    // Punteros cacheados: buscar por string en processBlock seria hashing en el
    // hilo de audio. Se resuelven una vez, en el constructor.
    std::atomic<float>* pAmpAttack         = nullptr;
    std::atomic<float>* pAmpDecay          = nullptr;
    std::atomic<float>* pAmpSustain        = nullptr;
    std::atomic<float>* pAmpRelease        = nullptr;
    std::atomic<float>* pOsc1Level         = nullptr;
    std::atomic<float>* pMasterGain        = nullptr;
    std::atomic<float>* pPolyphony         = nullptr;
    std::atomic<float>* pPitchBendRange    = nullptr;

    std::atomic<float>* pChordType         = nullptr;
    std::atomic<float>* pChordVoicing      = nullptr;
    std::atomic<float>* pChordInversion    = nullptr;
    std::atomic<float>* pChordSpread       = nullptr;
    std::atomic<float>* pChordDensity      = nullptr;
    std::atomic<float>* pChordOctaveDouble = nullptr;
    std::atomic<float>* pScaleLock         = nullptr;
    std::atomic<float>* pScaleKey          = nullptr;
    std::atomic<float>* pScaleMode         = nullptr;
    std::atomic<float>* pNoteMode          = nullptr;

    VoiceManager voices;
    NoteRouter router;
    NoteRouter::EventList events;
    chord::ChordParams chordParams;

    SilenceGate silenceGate;
    juce::SmoothedValue<float> masterGainSmoothed;

    float currentBendNormalised = 0.0f;   // -1..1

    /*  El modo Auto introduce latencia y hay que declararsela al host, pero
        setLatencySamples() no se puede llamar desde el hilo de audio. El
        procesador publica el valor deseado en un atomico y un Timer en el hilo
        de mensajes lo aplica. Ver DECISIONS.md D-012. */
    std::atomic<int> desiredLatency { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StabrProcessor)
};
}
