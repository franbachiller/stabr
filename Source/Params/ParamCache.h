#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterIDs.h"
#include "../DSP/VoiceParams.h"
#include "../Chord/ChordGenerator.h"

namespace stabr
{
/*  Punteros a los valores crudos del APVTS, resueltos una sola vez.

    POR QUE EXISTE: buscar un parametro por su ID es un hash de string. Hacerlo
    en processBlock, por cada parametro y cada bloque, seria trabajo inutil en
    el hilo de audio. Se resuelven todos en el constructor y despues solo se
    lee el atomico.

    Tambien mantiene al procesador legible: sin esto serian ~35 miembros
    sueltos y dos pantallas de asignaciones.
*/
struct ParamCache
{
    void resolve (juce::AudioProcessorValueTreeState& apvts)
    {
        auto get = [&apvts] (const char* id)
        {
            auto* p = apvts.getRawParameterValue (id);
            jassert (p != nullptr);   // ID mal escrito o parametro no declarado
            return p;
        };

        ampAttack   = get (pid::ampAttack);
        ampDecay    = get (pid::ampDecay);
        ampSustain  = get (pid::ampSustain);
        ampRelease  = get (pid::ampRelease);

        filtEnvAttack  = get (pid::filtEnvAttack);
        filtEnvDecay   = get (pid::filtEnvDecay);
        filtEnvSustain = get (pid::filtEnvSustain);
        filtEnvRelease = get (pid::filtEnvRelease);

        osc1Wave   = get (pid::osc1Wave);
        osc1Level  = get (pid::osc1Level);
        osc2Wave   = get (pid::osc2Wave);
        osc2Level  = get (pid::osc2Level);
        osc2Detune = get (pid::osc2Detune);
        osc2Octave = get (pid::osc2Octave);
        subWave    = get (pid::subWave);
        subLevel   = get (pid::subLevel);
        subOctave  = get (pid::subOctave);
        pulseWidth = get (pid::pulseWidth);

        unisonCount       = get (pid::unisonCount);
        unisonDetune      = get (pid::unisonDetune);
        unisonWidth       = get (pid::unisonWidth);
        unisonRandomPhase = get (pid::unisonRandomPhase);

        filterCutoff    = get (pid::filterCutoff);
        filterResonance = get (pid::filterResonance);
        filterType      = get (pid::filterType);
        filterSlope     = get (pid::filterSlope);
        filterEnvAmount = get (pid::filterEnvAmount);
        filterKeyTrack  = get (pid::filterKeyTrack);
        filterVelAmount = get (pid::filterVelAmount);
        filterPeakComp  = get (pid::filterPeakComp);

        driveAmount = get (pid::driveAmount);

        chordType         = get (pid::chordType);
        chordVoicing      = get (pid::chordVoicing);
        chordInversion    = get (pid::chordInversion);
        chordSpread       = get (pid::chordSpread);
        chordDensity      = get (pid::chordDensity);
        chordOctaveDouble = get (pid::chordOctaveDouble);
        scaleLock         = get (pid::scaleLock);
        scaleKey          = get (pid::scaleKey);
        scaleMode         = get (pid::scaleMode);
        noteMode          = get (pid::noteMode);

        masterGain     = get (pid::masterGain);
        polyphony      = get (pid::polyphony);
        pitchBendRange = get (pid::pitchBendRange);
        hqMode         = get (pid::hqMode);
    }

    //==========================================================================
    void fillVoiceParams (VoiceParams& v, float pitchBendSemitones) const noexcept
    {
        v.osc1Wave = (Waveform) (int) osc1Wave->load();
        v.osc2Wave = (Waveform) (int) osc2Wave->load();
        v.subWave  = (Waveform) (int) subWave->load();

        v.osc1Level = osc1Level->load();
        v.osc2Level = osc2Level->load();
        v.subLevel  = subLevel->load();

        v.osc2DetuneCents = osc2Detune->load();
        v.osc2Octave      = (int) osc2Octave->load();
        v.subOctave       = (int) subOctave->load();
        v.pulseWidth      = pulseWidth->load();

        v.unisonCount       = (int) unisonCount->load();
        v.unisonDetuneCents = unisonDetune->load();
        v.unisonWidth       = unisonWidth->load();
        v.unisonRandomPhase = unisonRandomPhase->load() > 0.5f;

        v.cutoffHz        = filterCutoff->load();
        v.resonance       = filterResonance->load();
        v.filterType      = (FilterType) (int) filterType->load();
        v.slope           = (StereoLadder::Slope) (int) filterSlope->load();
        v.peakCompensation = filterPeakComp->load() > 0.5f;

        v.envAmount = filterEnvAmount->load();
        v.keyTrack  = filterKeyTrack->load();
        v.velAmount = filterVelAmount->load();

        v.ampAttack  = ampAttack->load();
        v.ampDecay   = ampDecay->load();
        v.ampSustain = ampSustain->load();
        v.ampRelease = ampRelease->load();

        v.filtAttack  = filtEnvAttack->load();
        v.filtDecay   = filtEnvDecay->load();
        v.filtSustain = filtEnvSustain->load();
        v.filtRelease = filtEnvRelease->load();

        v.driveAmount = driveAmount->load();
        v.highQuality = hqMode->load() > 0.5f;

        v.pitchBendSemitones = pitchBendSemitones;
    }

    void fillChordParams (chord::ChordParams& c) const noexcept
    {
        c.type            = (chord::ChordType)   (int) chordType->load();
        c.lock            = (chord::ScaleLock)   (int) scaleLock->load();
        c.keyRoot         =                      (int) scaleKey->load();
        c.mode            = (chord::ScaleMode)   (int) scaleMode->load();
        c.voicing         = (chord::VoicingType) (int) chordVoicing->load();
        c.inversion       =                      (int) chordInversion->load();
        c.spread          =                      (int) chordSpread->load();
        c.density         =                      (int) chordDensity->load();
        c.doubleRootBelow = chordOctaveDouble->load() > 0.5f;
    }

    //==========================================================================
    std::atomic<float>* ampAttack = nullptr;
    std::atomic<float>* ampDecay = nullptr;
    std::atomic<float>* ampSustain = nullptr;
    std::atomic<float>* ampRelease = nullptr;

    std::atomic<float>* filtEnvAttack = nullptr;
    std::atomic<float>* filtEnvDecay = nullptr;
    std::atomic<float>* filtEnvSustain = nullptr;
    std::atomic<float>* filtEnvRelease = nullptr;

    std::atomic<float>* osc1Wave = nullptr;
    std::atomic<float>* osc1Level = nullptr;
    std::atomic<float>* osc2Wave = nullptr;
    std::atomic<float>* osc2Level = nullptr;
    std::atomic<float>* osc2Detune = nullptr;
    std::atomic<float>* osc2Octave = nullptr;
    std::atomic<float>* subWave = nullptr;
    std::atomic<float>* subLevel = nullptr;
    std::atomic<float>* subOctave = nullptr;
    std::atomic<float>* pulseWidth = nullptr;

    std::atomic<float>* unisonCount = nullptr;
    std::atomic<float>* unisonDetune = nullptr;
    std::atomic<float>* unisonWidth = nullptr;
    std::atomic<float>* unisonRandomPhase = nullptr;

    std::atomic<float>* filterCutoff = nullptr;
    std::atomic<float>* filterResonance = nullptr;
    std::atomic<float>* filterType = nullptr;
    std::atomic<float>* filterSlope = nullptr;
    std::atomic<float>* filterEnvAmount = nullptr;
    std::atomic<float>* filterKeyTrack = nullptr;
    std::atomic<float>* filterVelAmount = nullptr;
    std::atomic<float>* filterPeakComp = nullptr;

    std::atomic<float>* driveAmount = nullptr;

    std::atomic<float>* chordType = nullptr;
    std::atomic<float>* chordVoicing = nullptr;
    std::atomic<float>* chordInversion = nullptr;
    std::atomic<float>* chordSpread = nullptr;
    std::atomic<float>* chordDensity = nullptr;
    std::atomic<float>* chordOctaveDouble = nullptr;
    std::atomic<float>* scaleLock = nullptr;
    std::atomic<float>* scaleKey = nullptr;
    std::atomic<float>* scaleMode = nullptr;
    std::atomic<float>* noteMode = nullptr;

    std::atomic<float>* masterGain = nullptr;
    std::atomic<float>* polyphony = nullptr;
    std::atomic<float>* pitchBendRange = nullptr;
    std::atomic<float>* hqMode = nullptr;
};
}
