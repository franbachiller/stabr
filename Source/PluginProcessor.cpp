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
    p.resolve (apvts);

    startTimer (100);
}

StabrProcessor::~StabrProcessor()
{
    stopTimer();
}

void StabrProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Todas las reservas de memoria viven aca. processBlock no asigna nada.
    voices.prepare (sampleRate, samplesPerBlock);
    router.prepare (sampleRate);
    silenceGate.prepare (sampleRate);

    masterGainSmoothed.reset (sampleRate, 0.02);
    masterGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (p.masterGain->load(), -60.0f));

    currentBendNormalised = 0.0f;
}

void StabrProcessor::releaseResources()
{
}

void StabrProcessor::reset()
{
    voices.reset();
    router.reset();
    silenceGate.reset();
    currentBendNormalised = 0.0f;
}

double StabrProcessor::getTailLengthSeconds() const
{
    // Fase 2: solo la cola de la envolvente. En Fase 4 pasa a depender del
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

//==============================================================================
void StabrProcessor::timerCallback()
{
    // Hilo de mensajes: aca si se puede tocar la latencia declarada.
    const int wanted = desiredLatency.load();

    if (wanted != getLatencySamples())
        setLatencySamples (wanted);
}

//==============================================================================
void StabrProcessor::updateParametersForBlock() noexcept
{
    voices.setMaxPolyphony ((int) p.polyphony->load());

    p.fillVoiceParams (voiceParams, currentBendNormalised * p.pitchBendRange->load());
    voices.setParams (voiceParams);

    p.fillChordParams (chordParams);

    router.setNoteMode ((chord::NoteMode) (int) p.noteMode->load());
    desiredLatency.store (router.getLatencySamples());

    masterGainSmoothed.setTargetValue (
        juce::Decibels::decibelsToGain (p.masterGain->load(), -60.0f));
}

//==============================================================================
void StabrProcessor::applyEvent (const NoteRouter::Event& e) noexcept
{
    using Type = NoteRouter::Event::Type;

    switch (e.type)
    {
        case Type::chordOn:
        {
            // UNA TECLA -> UN ACORDE. La interaccion central del plugin.
            const auto notes = chord::generateChord (e.note, chordParams);
            voices.noteOnChord (e.note, notes, e.velocity);
            break;
        }

        case Type::noteOn:
            voices.noteOnSingle (e.note, e.velocity);
            break;

        case Type::noteOff:
            voices.noteOff (e.note);
            break;

        case Type::allNotesOff:
            voices.allNotesOff (true);
            break;

        case Type::allSoundOff:
            voices.allNotesOff (false);
            break;

        case Type::pitchBend:
            currentBendNormalised = ((float) e.pitchWheel - 8192.0f) / 8192.0f;
            voiceParams.pitchBendSemitones = currentBendNormalised * p.pitchBendRange->load();
            voices.setParams (voiceParams);
            break;
    }
}

//==============================================================================
void StabrProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    buffer.clear();

    updateParametersForBlock();

    // El router resuelve el modo de nota (Chord / Passthrough / Auto) y deja
    // los eventos ya listos, con su posicion exacta dentro del bloque.
    router.process (midiMessages, numSamples, events);

    // Render por sub-bloques entre eventos: sin esto, un stab disparado a mitad
    // de bloque suena cuantizado al tamano del buffer. Ver DECISIONS.md D-008.
    int startSample = 0;

    for (int i = 0; i < events.count; ++i)
    {
        const auto& e = events[i];
        const int eventPos = juce::jlimit (startSample, numSamples, e.offset);

        if (eventPos > startSample)
        {
            voices.renderAdd (buffer, startSample, eventPos - startSample);
            startSample = eventPos;
        }

        applyEvent (e);
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

//==============================================================================
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
