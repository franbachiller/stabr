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

    // --- Oscilador (Fase 1: solo sierra; se expande en Fase 3) ---------------
    inline constexpr auto osc1Level   = "osc1_level";

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
