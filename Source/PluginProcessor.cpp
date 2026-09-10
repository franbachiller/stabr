#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params/ParameterIDs.h"
#include "Params/ParameterLayout.h"
#include "Common/StateVersion.h"

namespace stabr
{
StabrProcessor::StabrProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, state::rootType, createParameterLayout())
{
    pAmpAttack      = apvts.getRawParameterValue (pid::ampAttack);
    pAmpDecay       = apvts.getRawParameterValue (pid::ampDecay);
    pAmpSustain     = apvts.getRawParameterValue (pid::ampSustain);
    pAmpRelease     = apvts.getRawParameterValue (pid::ampRelease);
    pOsc1Level      = apvts.getRawParameterValue (pid::osc1Level);
    pMasterGain     = apvts.getRawParameterValue (pid::masterGain);
    pPolyphony      = apvts.getRawParameterValue (pid::polyphony);
    pPitchBendRange = apvts.getRawParameterValue (pid::pitchBendRange);

    jassert (pAmpAttack != nullptr && pMasterGain != nullptr);
}

void StabrProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Todas las reservas de memoria viven aca. processBlock no asigna nada.
    voices.prepare (sampleRate, samplesPerBlock);
    silenceGate.prepare (sampleRate);

    masterGainSmoothed.reset (sampleRate, 0.02);
    masterGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (pMasterGain->load(), -60.0f));

    currentBendNormalised = 0.0f;
}

void StabrProcessor::releaseResources()
{
}

void StabrProcessor::reset()
{
    voices.reset();
    silenceGate.reset();
    currentBendNormalised = 0.0f;
}

double StabrProcessor::getTailLengthSeconds() const
{
    // Fase 1: solo la cola de la envolvente. En Fase 4 pasa a depender del
    // tiempo de delay y del decay de la reverb.
    return 2.0;
}

bool StabrProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannels() != 0)
        return false;

    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
}

void StabrProcessor::updateParametersForBlock() noexcept
{
    voices.setMaxPolyphony ((int) pPolyphony->load());
    voices.setEnvelopeParameters (pAmpAttack->load(),
                                  pAmpDecay->load(),
                                  pAmpSustain->load(),
                                  pAmpRelease->load());
    voices.setLevel (pOsc1Level->load());
    voices.setPitchBendSemitones (currentBendNormalised * pPitchBendRange->load());

    masterGainSmoothed.setTargetValue (
        juce::Decibels::decibelsToGain (pMasterGain->load(), -60.0f));
}

void StabrProcessor::handleMidiMessage (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
    {
        voices.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        voices.noteOff (m.getNoteNumber());
    }
    else if (m.isAllNotesOff())
    {
        voices.allNotesOff (true);
    }
    else if (m.isAllSoundOff())
    {
        voices.allNotesOff (false);
    }
    else if (m.isPitchWheel())
    {
        // 0..16383, centro 8192.
        currentBendNormalised = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
        voices.setPitchBendSemitones (currentBendNormalised * pPitchBendRange->load());
    }
}

void StabrProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // Limpieza de canales de salida que el host podria dejar sucios.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    buffer.clear();

    updateParametersForBlock();

    // Render por sub-bloques entre eventos MIDI: sin esto, un acorde disparado
    // a mitad de bloque suena cuantizado al tamaño del buffer, y el timing del
    // stab es exactamente lo que no se puede perder.
    int startSample = 0;

    for (const auto meta : midiMessages)
    {
        const int eventPos = juce::jlimit (0, numSamples, meta.samplePosition);
        const int numToRender = eventPos - startSample;

        if (numToRender > 0)
        {
            voices.renderAdd (buffer, startSample, numToRender);
            startSample = eventPos;
        }

        handleMidiMessage (meta.getMessage());
    }

    if (startSample < numSamples)
        voices.renderAdd (buffer, startSample, numSamples - startSample);

    // Ganancia de salida suavizada.
    if (masterGainSmoothed.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = masterGainSmoothed.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        buffer.applyGain (masterGainSmoothed.getCurrentValue());
    }

    // Silencio exacto: ver Common/SilenceGate.h
    silenceGate.setHasSound (voices.hasActiveVoices());
    if (silenceGate.processAndShouldClear (buffer))
        buffer.clear();
}

juce::AudioProcessorEditor* StabrProcessor::createEditor()
{
    return new StabrEditor (*this);
}

void StabrProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = apvts.copyState();
    tree.setProperty (state::versionProperty, state::kCurrentVersion, nullptr);

    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void StabrProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid())
        return;

    state::migrate (tree);
    apvts.replaceState (tree);
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new stabr::StabrProcessor();
}
