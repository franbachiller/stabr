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
    sequencer.prepare (sampleRate);
    latch.reset();
    sequencerWasRunning = false;

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
    sequencer.reset();
    latch.reset();
    sequencerWasRunning = false;
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

        El delay, la reverb, el pump y el secuenciador se sincronizan a esto.
        Si el host no informa tempo (algunos no lo hacen fuera del transporte),
        se usa el ultimo valido: quedarse en 0 haria que todas las divisiones
        dieran infinito. */
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

    transport.bpm         = bpm;
    transport.ppq         = ppq;
    transport.isPlaying   = playing;
    transport.hasPosition = positionValid;

    p.fillSeqParams (seqParams);
    sequencer.setParams (seqParams);

    /*  Con latencia declarada (modo Auto) el host adelanta la salida del
        plugin. Todo lo que se alinea al transporte tiene que generarse esa
        misma cantidad mas tarde, o el pump caeria 20 ms antes del bombo. */
    const double latencyBeats = (double) router.getLatencySamples() * (bpm / 60.0)
                              / juce::jmax (1.0, getSampleRate());

    p.fillFxParams (fxParams);
    fxChain.setParams (fxParams, bpm);
    fxChain.setTransport (ppq - latencyBeats, bpm, playing, positionValid);

    masterGainSmoothed.setTargetValue (
        juce::Decibels::decibelsToGain (p.masterGain->load(), -60.0f));
}

//==============================================================================
void StabrProcessor::applyEvent (const NoteRouter::Event& e, bool sequencerRunning) noexcept
{
    using Type = NoteRouter::Event::Type;

    switch (e.type)
    {
        case Type::chordOn:
        case Type::noteOn:
        {
            // Toda tecla se latchea, suene o no: lo que se toca con el
            // transporte parado es lo que el secuenciador dispara al arrancar.
            latch.noteOn (e.note, e.velocity);

            /*  Con el secuenciador andando, la tecla ELIGE el acorde pero no
                lo dispara: el disparo es del patron. Si sonaran las dos cosas,
                cada tecla meteria un stab fuera de tiempo. Ver D-017. */
            if (sequencerRunning)
                break;

            if (e.type == Type::chordOn)
            {
                // UNA TECLA -> UN ACORDE. La interaccion central del plugin.
                const auto notes = chord::generateChord (e.note, chordParams);
                voices.noteOnChord (e.note, notes, e.velocity);
            }
            else
            {
                voices.noteOnSingle (e.note, e.velocity);
            }
            break;
        }

        case Type::noteOff:
            latch.noteOff (e.note);

            if (! sequencerRunning)
                voices.noteOff (e.note);
            break;

        case Type::allNotesOff:
            latch.releaseAllHeld();
            voices.allNotesOff (true);
            break;

        case Type::allSoundOff:
            latch.releaseAllHeld();
            voices.allNotesOff (false);
            break;

        case Type::pitchBend:
            currentBendNormalised = ((float) e.pitchWheel - 8192.0f) / 8192.0f;
            voiceParams.pitchBendSemitones = currentBendNormalised * p.pitchBendRange->load();
            voices.setParams (voiceParams);
            break;
    }
}

void StabrProcessor::applySequencerEvent (const seq::StepSequencer::Event& e) noexcept
{
    if (e.type == seq::StepSequencer::Event::Type::gateOn)
        triggerSequencedChord (e.velocity);
    else
        voices.noteOff (kSequencerTrigger);
}

void StabrProcessor::triggerSequencedChord (float velocity) noexcept
{
    /*  QUE dispara el secuenciador (D-017):

        - Sin nada latcheado: el acorde de la tonica de la tonalidad. El plugin
          suena apenas se enciende el secuenciador y se aprieta play, sin MIDI.
        - Chord: el acorde generado desde la ultima tecla.
        - Passthrough: las notas latcheadas tal cual (el acorde tocado a mano).
        - Auto: la misma regla que en vivo, una tecla genera y varias pasan.

        Todo entra al motor de voces como UN disparo con el mismo trigger: el
        acorde anterior se suelta solo y el robo sigue siendo por acorde. */
    if (latch.isEmpty())
    {
        const int root = kFallbackRootBase + chordParams.keyRoot;
        voices.noteOnChord (kSequencerTrigger, chord::generateChord (root, chordParams), velocity);
        return;
    }

    const auto mode = router.getNoteMode();
    const bool literal = mode == chord::NoteMode::passthrough
                      || (mode == chord::NoteMode::autoDetect && latch.size() > 1);

    if (literal)
        voices.noteOnChord (kSequencerTrigger, latch.getLatchedNotes(), velocity);
    else
        voices.noteOnChord (kSequencerTrigger,
                            chord::generateChord (latch.getLastNote(), chordParams), velocity);
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

    // El secuenciador emite sus gates con la misma precision de muestra.
    sequencer.process (transport, numSamples, router.getLatencySamples(), sequencerEvents);

    const bool sequencerRunning = sequencer.isRunning();

    // Al arrancar, lo que se estaba tocando en vivo se suelta: si no, un
    // acorde mantenido con el transporte parado seguiria sonando encima del
    // patron hasta que se soltara la tecla.
    if (sequencerRunning && ! sequencerWasRunning)
        voices.allNotesOff (true);

    sequencerWasRunning = sequencerRunning;

    /*  Render por sub-bloques entre eventos: sin esto, un stab disparado a
        mitad de bloque suena cuantizado al tamano del buffer. Ver D-008.

        Las dos listas (MIDI y secuenciador) ya vienen ordenadas y se mezclan
        en orden temporal. A igual muestra va primero el MIDI: una tecla
        apretada justo en un paso tiene que cambiar el acorde de ESE paso. */
    int startSample = 0;
    int midiIndex = 0;
    int seqIndex = 0;

    while (midiIndex < events.count || seqIndex < sequencerEvents.count)
    {
        const bool takeMidi = seqIndex >= sequencerEvents.count
                           || (midiIndex < events.count
                               && events[midiIndex].offset <= sequencerEvents[seqIndex].offset);

        const int offset = takeMidi ? events[midiIndex].offset
                                    : sequencerEvents[seqIndex].offset;
        const int eventPos = juce::jlimit (startSample, numSamples, offset);

        if (eventPos > startSample)
        {
            voices.renderAdd (buffer, startSample, eventPos - startSample);
            startSample = eventPos;
        }

        if (takeMidi)
            applyEvent (events[midiIndex++], sequencerRunning);
        else
            applySequencerEvent (sequencerEvents[seqIndex++]);
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

    /*  Los parametros que un proyecto viejo no trae (los del secuenciador en un
        proyecto de la v0.4) quedan en su DEFAULT, no en el valor que tuviera la
        instancia: JUCE crea el nodo que falta y al agregarlo le aplica el
        default. Hay un test que lo verifica. Ver DECISIONS.md D-040. */
    apvts.replaceState (tree);
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new stabr::StabrProcessor();
}
