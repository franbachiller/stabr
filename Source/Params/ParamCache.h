#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterIDs.h"
#include "../DSP/VoiceParams.h"
#include "../Chord/ChordGenerator.h"
#include "../FX/FxChain.h"

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

        fxOrder        = get (pid::fxOrder);

        satEnabled     = get (pid::satEnabled);
        satType        = get (pid::satType);
        satDrive       = get (pid::satDrive);
        satBits        = get (pid::satBits);
        satRateReduce  = get (pid::satRateReduce);
        satMix         = get (pid::satMix);

        chorusEnabled  = get (pid::chorusEnabled);
        chorusEnsemble = get (pid::chorusEnsemble);
        chorusRate     = get (pid::chorusRate);
        chorusDepth    = get (pid::chorusDepth);
        chorusWidth    = get (pid::chorusWidth);
        chorusMix      = get (pid::chorusMix);

        delayEnabled   = get (pid::delayEnabled);
        delayMode      = get (pid::delayMode);
        delaySync      = get (pid::delaySync);
        delayDivision  = get (pid::delayDivision);
        delayFreeMs    = get (pid::delayFreeMs);
        delayOffset    = get (pid::delayOffset);
        delayFeedback  = get (pid::delayFeedback);
        delayFreeze    = get (pid::delayFreeze);
        delayHighpass  = get (pid::delayHighpass);
        delayLowpass   = get (pid::delayLowpass);
        delayModDepth  = get (pid::delayModDepth);
        delayGlide     = get (pid::delayGlide);
        delayDucking   = get (pid::delayDucking);
        delayMix       = get (pid::delayMix);

        reverbEnabled      = get (pid::reverbEnabled);
        reverbMode         = get (pid::reverbMode);
        reverbSize         = get (pid::reverbSize);
        reverbDecay        = get (pid::reverbDecay);
        reverbDamping      = get (pid::reverbDamping);
        reverbModDepth     = get (pid::reverbModDepth);
        reverbPreDelaySync = get (pid::reverbPreDelaySync);
        reverbPreDelayDiv  = get (pid::reverbPreDelayDiv);
        reverbPreDelayMs   = get (pid::reverbPreDelayMs);
        reverbLowCut       = get (pid::reverbLowCut);
        reverbDucking      = get (pid::reverbDucking);
        reverbMix          = get (pid::reverbMix);

        pumpEnabled  = get (pid::pumpEnabled);
        pumpDivision = get (pid::pumpDivision);
        pumpDepth    = get (pid::pumpDepth);
        pumpShape    = get (pid::pumpShape);
        pumpAttack   = get (pid::pumpAttack);

        outputWidth     = get (pid::outputWidth);
        outputMonoBelow = get (pid::outputMonoBelow);
        outputLimiter   = get (pid::outputLimiter);
        outputGain      = get (pid::outputGain);
    }

    //==========================================================================
    void fillFxParams (fx::FxParams& f) const noexcept
    {
        f.order = (fx::FxOrder) (int) fxOrder->load();

        f.saturation.enabled = satEnabled->load() > 0.5f;
        f.saturation.type    = (fx::SaturationType) (int) satType->load();
        f.saturation.drive   = satDrive->load();
        f.saturation.bits    = satBits->load();
        f.saturation.rateReduction = satRateReduce->load();
        f.saturation.mix     = satMix->load();

        f.chorus.enabled  = chorusEnabled->load() > 0.5f;
        f.chorus.ensemble = chorusEnsemble->load() > 0.5f;
        f.chorus.rate     = chorusRate->load();
        f.chorus.depth    = chorusDepth->load();
        f.chorus.width    = chorusWidth->load();
        f.chorus.mix      = chorusMix->load();

        f.delay.enabled     = delayEnabled->load() > 0.5f;
        f.delay.mode        = (fx::DelayMode) (int) delayMode->load();
        f.delay.tempoSync   = delaySync->load() > 0.5f;
        f.delay.division    = (fx::Division) (int) delayDivision->load();
        f.delay.freeMs      = delayFreeMs->load();
        f.delay.offsetRight = delayOffset->load();
        f.delay.feedback    = delayFeedback->load();
        f.delay.freeze      = delayFreeze->load() > 0.5f;
        f.delay.highpassHz  = delayHighpass->load();
        f.delay.lowpassHz   = delayLowpass->load();
        f.delay.modDepth    = delayModDepth->load();
        f.delay.glide       = (fx::DelayLine::GlideMode) (int) delayGlide->load();
        f.delay.ducking     = delayDucking->load();
        f.delay.mix         = delayMix->load();

        f.reverb.enabled      = reverbEnabled->load() > 0.5f;
        f.reverb.mode         = (fx::ReverbMode) (int) reverbMode->load();
        f.reverb.size         = reverbSize->load();
        f.reverb.decay        = reverbDecay->load();
        f.reverb.damping      = reverbDamping->load();
        f.reverb.modDepth     = reverbModDepth->load();
        f.reverb.preDelaySync = reverbPreDelaySync->load() > 0.5f;
        f.reverb.preDelayDivision = (fx::Division) (int) reverbPreDelayDiv->load();
        f.reverb.preDelayMs   = reverbPreDelayMs->load();
        f.reverb.lowCutHz     = reverbLowCut->load();
        f.reverb.ducking      = reverbDucking->load();
        f.reverb.mix          = reverbMix->load();

        f.pump.enabled  = pumpEnabled->load() > 0.5f;
        f.pump.division = (fx::Division) (int) pumpDivision->load();
        f.pump.depth    = pumpDepth->load();
        f.pump.shape    = pumpShape->load();
        f.pump.attackMs = pumpAttack->load();

        f.output.width       = outputWidth->load();
        f.output.monoBelowHz = outputMonoBelow->load();
        f.output.limiterOn   = outputLimiter->load() > 0.5f;
        f.output.outputGainDb = outputGain->load();
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

    std::atomic<float>* fxOrder = nullptr;

    std::atomic<float>* satEnabled = nullptr;
    std::atomic<float>* satType = nullptr;
    std::atomic<float>* satDrive = nullptr;
    std::atomic<float>* satBits = nullptr;
    std::atomic<float>* satRateReduce = nullptr;
    std::atomic<float>* satMix = nullptr;

    std::atomic<float>* chorusEnabled = nullptr;
    std::atomic<float>* chorusEnsemble = nullptr;
    std::atomic<float>* chorusRate = nullptr;
    std::atomic<float>* chorusDepth = nullptr;
    std::atomic<float>* chorusWidth = nullptr;
    std::atomic<float>* chorusMix = nullptr;

    std::atomic<float>* delayEnabled = nullptr;
    std::atomic<float>* delayMode = nullptr;
    std::atomic<float>* delaySync = nullptr;
    std::atomic<float>* delayDivision = nullptr;
    std::atomic<float>* delayFreeMs = nullptr;
    std::atomic<float>* delayOffset = nullptr;
    std::atomic<float>* delayFeedback = nullptr;
    std::atomic<float>* delayFreeze = nullptr;
    std::atomic<float>* delayHighpass = nullptr;
    std::atomic<float>* delayLowpass = nullptr;
    std::atomic<float>* delayModDepth = nullptr;
    std::atomic<float>* delayGlide = nullptr;
    std::atomic<float>* delayDucking = nullptr;
    std::atomic<float>* delayMix = nullptr;

    std::atomic<float>* reverbEnabled = nullptr;
    std::atomic<float>* reverbMode = nullptr;
    std::atomic<float>* reverbSize = nullptr;
    std::atomic<float>* reverbDecay = nullptr;
    std::atomic<float>* reverbDamping = nullptr;
    std::atomic<float>* reverbModDepth = nullptr;
    std::atomic<float>* reverbPreDelaySync = nullptr;
    std::atomic<float>* reverbPreDelayDiv = nullptr;
    std::atomic<float>* reverbPreDelayMs = nullptr;
    std::atomic<float>* reverbLowCut = nullptr;
    std::atomic<float>* reverbDucking = nullptr;
    std::atomic<float>* reverbMix = nullptr;

    std::atomic<float>* pumpEnabled = nullptr;
    std::atomic<float>* pumpDivision = nullptr;
    std::atomic<float>* pumpDepth = nullptr;
    std::atomic<float>* pumpShape = nullptr;
    std::atomic<float>* pumpAttack = nullptr;

    std::atomic<float>* outputWidth = nullptr;
    std::atomic<float>* outputMonoBelow = nullptr;
    std::atomic<float>* outputLimiter = nullptr;
    std::atomic<float>* outputGain = nullptr;
};
}
