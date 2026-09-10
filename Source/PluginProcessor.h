#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/VoiceManager.h"
#include "Common/SilenceGate.h"

namespace stabr
{
class StabrProcessor : public juce::AudioProcessor
{
public:
    StabrProcessor();
    ~StabrProcessor() override = default;

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

private:
    void handleMidiMessage (const juce::MidiMessage&) noexcept;
    void updateParametersForBlock() noexcept;

    juce::AudioProcessorValueTreeState apvts;

    // Punteros cacheados: buscar por string en processBlock seria I/O de hash
    // en el hilo de audio. Se resuelven una vez, en el constructor.
    std::atomic<float>* pAmpAttack       = nullptr;
    std::atomic<float>* pAmpDecay        = nullptr;
    std::atomic<float>* pAmpSustain      = nullptr;
    std::atomic<float>* pAmpRelease      = nullptr;
    std::atomic<float>* pOsc1Level       = nullptr;
    std::atomic<float>* pMasterGain      = nullptr;
    std::atomic<float>* pPolyphony       = nullptr;
    std::atomic<float>* pPitchBendRange  = nullptr;

    VoiceManager voices;
    SilenceGate silenceGate;
    juce::SmoothedValue<float> masterGainSmoothed;

    float currentBendNormalised = 0.0f;   // -1..1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StabrProcessor)
};
}
