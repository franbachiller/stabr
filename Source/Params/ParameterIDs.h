#pragma once

/*  STABR - IDs de parametros.

    REGLA DURA: un ID publicado NUNCA se renombra ni se reutiliza. Cambiarlo
    rompe todos los proyectos guardados de todos los usuarios. Si un parametro
    queda obsoleto se deja el ID muerto y se agrega uno nuevo; la migracion se
    hace en StateMigration.

    El sufijo numerico de kVersionHint es el "version hint" de JUCE: agrupa los
    parametros por la version del plugin en la que aparecieron. AU y AAX derivan
    IDs numericos de esto, asi que cada parametro nuevo en una version futura
    debe llevar el hint de ESA version, no el de la 1.
*/

namespace stabr::pid
{
    // Version del layout de parametros. Los parametros nuevos que se agreguen
    // en la v2 usan kVersionHintV2, y asi.
    inline constexpr int kVersionHintV1 = 1;

    // --- Envolvente de amplitud ----------------------------------------------
    inline constexpr auto ampAttack   = "amp_attack";
    inline constexpr auto ampDecay    = "amp_decay";
    inline constexpr auto ampSustain  = "amp_sustain";
    inline constexpr auto ampRelease  = "amp_release";

    // --- Osciladores ----------------------------------------------------------
    inline constexpr auto osc1Level   = "osc1_level";
    inline constexpr auto osc1Wave    = "osc1_wave";
    inline constexpr auto osc2Wave    = "osc2_wave";
    inline constexpr auto osc2Level   = "osc2_level";
    inline constexpr auto osc2Detune  = "osc2_detune";
    inline constexpr auto osc2Octave  = "osc2_octave";
    inline constexpr auto subWave     = "sub_wave";
    inline constexpr auto subLevel    = "sub_level";
    inline constexpr auto subOctave   = "sub_octave";
    inline constexpr auto pulseWidth  = "osc_pulse_width";

    // --- Unisono --------------------------------------------------------------
    inline constexpr auto unisonCount       = "uni_count";
    inline constexpr auto unisonDetune      = "uni_detune";
    inline constexpr auto unisonWidth       = "uni_width";
    inline constexpr auto unisonRandomPhase = "uni_phase_random";

    // --- Filtro ---------------------------------------------------------------
    inline constexpr auto filterCutoff    = "filt_cutoff";
    inline constexpr auto filterResonance = "filt_reso";
    inline constexpr auto filterType      = "filt_type";
    inline constexpr auto filterSlope     = "filt_slope";
    inline constexpr auto filterEnvAmount = "filt_env_amount";
    inline constexpr auto filterKeyTrack  = "filt_key_track";
    inline constexpr auto filterVelAmount = "filt_vel_amount";
    inline constexpr auto filterPeakComp  = "filt_peak_comp";

    // --- Envolvente de filtro -------------------------------------------------
    inline constexpr auto filtEnvAttack  = "fenv_attack";
    inline constexpr auto filtEnvDecay   = "fenv_decay";
    inline constexpr auto filtEnvSustain = "fenv_sustain";
    inline constexpr auto filtEnvRelease = "fenv_release";

    // --- Drive ----------------------------------------------------------------
    inline constexpr auto driveAmount = "drive_amount";

    // --- Cadena de efectos (Fase 4) -------------------------------------------
    inline constexpr auto fxOrder = "fx_order";

    inline constexpr auto satEnabled     = "sat_enabled";
    inline constexpr auto satType        = "sat_type";
    inline constexpr auto satDrive       = "sat_drive";
    inline constexpr auto satBits        = "sat_bits";
    inline constexpr auto satRateReduce  = "sat_rate_reduce";
    inline constexpr auto satMix         = "sat_mix";

    inline constexpr auto chorusEnabled  = "chorus_enabled";
    inline constexpr auto chorusEnsemble = "chorus_ensemble";
    inline constexpr auto chorusRate     = "chorus_rate";
    inline constexpr auto chorusDepth    = "chorus_depth";
    inline constexpr auto chorusWidth    = "chorus_width";
    inline constexpr auto chorusMix      = "chorus_mix";

    inline constexpr auto delayEnabled   = "delay_enabled";
    inline constexpr auto delayMode      = "delay_mode";
    inline constexpr auto delaySync      = "delay_sync";
    inline constexpr auto delayDivision  = "delay_division";
    inline constexpr auto delayFreeMs    = "delay_free_ms";
    inline constexpr auto delayOffset    = "delay_offset";
    inline constexpr auto delayFeedback  = "delay_feedback";
    inline constexpr auto delayFreeze    = "delay_freeze";
    inline constexpr auto delayHighpass  = "delay_hp";
    inline constexpr auto delayLowpass   = "delay_lp";
    inline constexpr auto delayModDepth  = "delay_mod";
    inline constexpr auto delayGlide     = "delay_glide";
    inline constexpr auto delayDucking   = "delay_duck";
    inline constexpr auto delayMix       = "delay_mix";

    inline constexpr auto reverbEnabled      = "rev_enabled";
    inline constexpr auto reverbMode         = "rev_mode";
    inline constexpr auto reverbSize         = "rev_size";
    inline constexpr auto reverbDecay        = "rev_decay";
    inline constexpr auto reverbDamping      = "rev_damping";
    inline constexpr auto reverbModDepth     = "rev_mod";
    inline constexpr auto reverbPreDelaySync = "rev_predelay_sync";
    inline constexpr auto reverbPreDelayDiv  = "rev_predelay_div";
    inline constexpr auto reverbPreDelayMs   = "rev_predelay_ms";
    inline constexpr auto reverbLowCut       = "rev_lowcut";
    inline constexpr auto reverbDucking      = "rev_duck";
    inline constexpr auto reverbMix          = "rev_mix";

    inline constexpr auto pumpEnabled  = "pump_enabled";
    inline constexpr auto pumpDivision = "pump_division";
    inline constexpr auto pumpDepth    = "pump_depth";
    inline constexpr auto pumpShape    = "pump_shape";
    inline constexpr auto pumpAttack   = "pump_attack";

    inline constexpr auto outputWidth     = "out_width";
    inline constexpr auto outputMonoBelow = "out_mono_below";
    inline constexpr auto outputLimiter   = "out_limiter";
    inline constexpr auto outputGain      = "out_gain";

    // --- Motor de acordes (Fase 2) -------------------------------------------
    inline constexpr auto chordType         = "chord_type";
    inline constexpr auto chordVoicing      = "chord_voicing";
    inline constexpr auto chordInversion    = "chord_inversion";
    inline constexpr auto chordSpread       = "chord_spread";
    inline constexpr auto chordDensity      = "chord_density";
    inline constexpr auto chordOctaveDouble = "chord_octave_double";
    inline constexpr auto scaleLock         = "scale_lock_mode";
    inline constexpr auto scaleKey          = "scale_key";
    inline constexpr auto scaleMode         = "scale_mode";
    inline constexpr auto noteMode          = "note_mode";

    // --- Global ---------------------------------------------------------------
    inline constexpr auto masterGain      = "master_gain";
    inline constexpr auto polyphony       = "polyphony";
    inline constexpr auto pitchBendRange  = "pitch_bend_range";
    inline constexpr auto hqMode          = "hq_mode";

    /*  Espacio de nombres reservado para fases siguientes. Se declara aca para
        que nadie invente un ID incompatible mas adelante:

        Fase 3 (caracter): osc1_wave, osc2_wave, osc2_level, osc2_detune,
                           osc2_octave, sub_level, sub_wave, uni_count,
                           uni_detune, uni_width, uni_phase_mode,
                           filt_cutoff, filt_reso, filt_type, filt_slope,
                           filt_env_amount, filt_key_track, filt_vel_amount,
                           fenv_attack..fenv_release, drive_amount, drive_type
        Fase 4 (fx):       sat_*, chorus_*, delay_*, reverb_*, pump_*,
                           width_*, mono_freq, limiter_*
        Fase 5 (seq):      seq_enabled, seq_steps, seq_swing, seq_rate,
                           seq_gate, seq_pattern, seq_step_on_00..15,
                           seq_step_vel_00..15, seq_step_prob_00..15,
                           seq_step_ratchet_00..15
        Fase 6 (gui):      no son parametros: van en el ValueTree de estado
                           (ui_scale, ui_page, ui_theme)
    */
}
