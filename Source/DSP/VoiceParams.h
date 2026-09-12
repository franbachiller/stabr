#pragma once

#include "Oscillators/Oscillator.h"
#include "Filters/SvfTpt.h"

namespace stabr
{
/*  Todos los parametros de una voz, en una sola estructura.

    El procesador la llena UNA vez por bloque desde los atomicos del APVTS y se
    la pasa por referencia constante a las voces. La alternativa (un setter por
    parametro, multiplicado por 70 voces) seria ~1500 llamadas por bloque para
    escribir siempre los mismos valores.
*/
struct VoiceParams
{
    // --- Osciladores --------------------------------------------------------
    Waveform osc1Wave   = Waveform::saw;
    Waveform osc2Wave   = Waveform::saw;
    Waveform subWave    = Waveform::sine;

    float osc1Level     = 0.8f;
    float osc2Level     = 0.0f;
    float subLevel      = 0.0f;

    float osc2DetuneCents = 0.0f;
    int   osc2Octave      = 0;
    int   subOctave       = -1;
    float pulseWidth      = 0.5f;

    // --- Unisono ------------------------------------------------------------
    int   unisonCount       = 1;
    float unisonDetuneCents = 12.0f;
    float unisonWidth       = 0.7f;
    bool  unisonRandomPhase = false;

    // --- Filtro -------------------------------------------------------------
    float cutoffHz   = 12000.0f;
    float resonance  = 0.0f;
    FilterType filterType  = FilterType::lowpass;
    StereoLadder::Slope slope = StereoLadder::Slope::db24;
    bool  peakCompensation = false;

    // Modulacion del cutoff, toda en OCTAVAS para que sea musical:
    // duplicar la frecuencia siempre suena como el mismo salto.
    float envAmount  = 0.0f;   // -1..1 bipolar, escalado por kEnvOctaves
    float keyTrack   = 0.0f;   // 0..1, 1 = sigue la nota exactamente
    float velAmount  = 0.0f;   // 0..1, escalado por kVelOctaves

    // --- Envolventes --------------------------------------------------------
    float ampAttack = 0.5f, ampDecay = 220.0f, ampSustain = 0.0f, ampRelease = 120.0f;
    float filtAttack = 0.5f, filtDecay = 180.0f, filtSustain = 0.0f, filtRelease = 120.0f;

    // --- Drive --------------------------------------------------------------
    float driveAmount = 0.0f;
    bool  highQuality = false;

    // --- Global -------------------------------------------------------------
    float pitchBendSemitones = 0.0f;

    static constexpr float kEnvOctaves = 5.0f;
    static constexpr float kVelOctaves = 3.0f;
};
}
