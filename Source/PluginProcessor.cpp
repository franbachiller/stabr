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
    fxChain.prepare (sampleRate, samplesPerBlock);
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
    fxChain.reset();
    silenceGate.reset();
    currentBendNormalised = 0.0f;
}

double StabrProcessor::getTailLengthSeconds() const
{
    /*  La cola ya no es solo la envolvente: con feedback alto y una reverb
        larga, el sonido sigue mucho despues del ultimo note-off.

        Se declara generoso a proposito. Si esto queda corto, un host que
        renderiza offline corta la cola en seco y el archivo exportado suena
        distinto de lo que se escucho. */
    return 12.0;
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

    /*  Tempo y posicion del host.

        El delay, la reverb y el pump se sincronizan a esto. Si el host no
        informa tempo (algunos no lo hacen fuera del transporte), se usa el
        ultimo valido: quedarse en 0 haria que todas las divisiones dieran
        infinito. */
    double bpm = lastKnownBpm;
    double ppq = 0.0;
    bool playing = false;
    bool positionValid = false;

    if (auto* host = getPlayHead())
    {
        if (const auto position = host->getPosition())
        {
            if (const auto hostBpm = position->getBpm())
                if (*hostBpm > 0.0)
                    bpm = *hostBpm;

            if (const auto hostPpq = position->getPpqPosition())
            {
                ppq = *hostPpq;
                positionValid = true;
            }

            playing = position->getIsPlaying();
        }
    }

    lastKnownBpm = bpm;

    p.fillFxParams (fxParams);
    fxChain.setParams (fxParams, bpm);
    fxChain.setTransport (ppq, bpm, playing, positionValid);

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

    // Ganancia de las voces antes de los efectos: mover el master DESPUES del
    // delay cambiaria el nivel de las repeticiones ya en el lazo.
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

    /*  El gate se consulta ANTES de los efectos, no despues.

        La cadena de efectos es estereo, asi que su costo no depende de la
        polifonia: cuesta lo mismo con 56 voces que con ninguna. Procesarla en
        reposo medía 1.13% de un core POR INSTANCIA sin producir nada; en un
        proyecto con veinte instancias son 22% de un core tirados.

        Saltearla es seguro porque el gate solo se activa cuando ya no hay
        voces Y el pico lleva ~200 ms por debajo de -120 dBFS. En ese punto las
        colas del delay y de la reverb ya son inaudibles, asi que congelar su
        estado no produce ningun salto al volver a sonar: la primera nota nueva
        levanta el gate en el mismo bloque. */
    silenceGate.setHasSound (voices.hasActiveVoices());

    if (! silenceGate.isSilent())
    {
        const int numChannels = buffer.getNumChannels();
        float* left  = buffer.getWritePointer (0);
        float* right = numChannels >= 2 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i];
            float r = right != nullptr ? right[i] : l;

            fxChain.process (l, r);

            if (right != nullptr)
            {
                left[i]  = l;
                right[i] = r;
            }
            else
            {
                left[i] = (l + r) * 0.5f;   // salida mono: se suma
            }
        }
    }

    /*  Silencio exacto. Desde la Fase 4 esto SI hace falta: las colas del delay
        y la reverb decaen asintoticamente y nunca llegan a cero solas.
        Ver DECISIONS.md D-007. */
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
