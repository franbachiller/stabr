#include "FactoryPresets.h"
#include "../Params/ParameterIDs.h"

namespace stabr::presets
{
namespace
{
    /*  Indices de las elecciones, con nombre: un preset con "3" en la forma de
        onda no se puede revisar a ojo. */
    namespace wave  { constexpr float saw = 0, pulse = 1, triangle = 2, organ = 3, sine = 4; }
    namespace chord { constexpr float power = 0, major = 1, minor = 2, sus2 = 3, sus4 = 4, min7 = 5, maj7 = 6,
                                      min9 = 7, maj9 = 8, min11 = 9, sixNine = 10, add9 = 11, dim7 = 12; }
    namespace voicing { constexpr float closed = 0, open = 1, drop2 = 2, drop3 = 3; }
    namespace filter  { constexpr float lowpass = 0, bandpass = 1, highpass = 2; constexpr float slope12 = 0, slope24 = 1; }
    namespace sat     { constexpr float tape = 0, transistor = 1, diode = 2, bitcrush = 3; }
    namespace dly     { constexpr float stereo = 0, pingPong = 1, dub = 2; }
    namespace rev     { constexpr float plate = 0, room = 1, hall = 2; }
    namespace div     { constexpr float n1_16 = 4, n1_8T = 5, n1_16D = 6, n1_8 = 7, n1_4T = 8, n1_8D = 9, n1_4 = 10, n1_4D = 12; }
    namespace pattern { constexpr float custom = 0, offbeat = 1, tresillo = 2, skippy = 3, sparse = 4; }

    struct Builder
    {
        Preset preset;

        Builder (const char* name, const char* category, const char* description)
        {
            preset.name = name;
            preset.category = category;
            preset.description = description;
        }

        Builder& set (const char* id, float value)
        {
            preset.values.push_back ({ id, value });
            return *this;
        }

        Builder& steps (std::initializer_list<int> onSteps, float velocity = 0.9f)
        {
            for (int i : onSteps)
            {
                set (pid::seqStepOn[(size_t) i], 1.0f);
                set (pid::seqStepVelocity[(size_t) i], velocity);
            }

            // Los pasos que no estan en la lista quedan apagados explicitamente:
            // el default de los pasos editables NO es vacio (D-039).
            for (int i = 0; i < 16; ++i)
                if (std::find (onSteps.begin(), onSteps.end(), i) == onSteps.end())
                    set (pid::seqStepOn[(size_t) i], 0.0f);

            return *this;
        }
    };

    std::vector<Preset> build()
    {
        using namespace pid;
        std::vector<Preset> list;

        //----------------------------------------------------------------------
        // HOUSE
        //----------------------------------------------------------------------
        list.push_back (Builder ("Classic Organ Stab", "House", "M1-style organ chord, the sound of early-90s house piano and organ hits.")
            .set (osc1Wave, wave::organ).set (osc1Level, 0.85f)
            .set (osc2Wave, wave::organ).set (osc2Level, 0.25f).set (osc2Octave, 1).set (osc2Detune, 3.0f)
            .set (filterCutoff, 9000.0f).set (filterResonance, 0.10f).set (filterEnvAmount, 0.25f).set (filtEnvDecay, 180.0f)
            .set (ampDecay, 350.0f).set (ampRelease, 90.0f)
            .set (chordType, chord::min7).set (chordDensity, 4)
            .set (satDrive, 0.15f)
            .set (chorusMix, 0.25f).set (chorusRate, 0.5f).set (chorusDepth, 0.35f)
            .set (delayDivision, div::n1_8D).set (delayFeedback, 0.25f).set (delayMix, 0.15f).set (delayLowpass, 5000.0f)
            .set (reverbMode, rev::room).set (reverbSize, 0.35f).set (reverbDecay, 0.35f).set (reverbMix, 0.12f)
            .preset);

        list.push_back (Builder ("Deep Min9", "House", "Soft detuned saws through a closed low-pass. Warm, wide, deep house.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.70f)
            .set (osc2Wave, wave::saw).set (osc2Level, 0.50f).set (osc2Detune, 9.0f)
            .set (unisonCount, 3).set (unisonDetune, 12.0f).set (unisonWidth, 0.8f)
            .set (filterCutoff, 1400.0f).set (filterResonance, 0.18f).set (filterEnvAmount, 0.45f).set (filtEnvDecay, 320.0f)
            .set (ampAttack, 3.0f).set (ampDecay, 600.0f).set (ampRelease, 300.0f)
            .set (chordType, chord::min9).set (chordVoicing, voicing::open).set (chordDensity, 5)
            .set (satDrive, 0.10f)
            .set (chorusMix, 0.35f).set (chorusRate, 0.35f).set (chorusDepth, 0.5f)
            .set (delayDivision, div::n1_8D).set (delayFeedback, 0.35f).set (delayMix, 0.22f)
            .set (delayHighpass, 250.0f).set (delayLowpass, 3500.0f).set (delayDucking, 0.5f)
            .set (reverbMode, rev::hall).set (reverbSize, 0.6f).set (reverbDecay, 0.55f).set (reverbMix, 0.22f).set (reverbLowCut, 250.0f)
            .preset);

        list.push_back (Builder ("Garage Swing", "House", "Short organ chord on a swung skippy pattern. Press play: it is already a 2-step groove.")
            .set (osc1Wave, wave::organ).set (osc1Level, 0.80f)
            .set (osc2Wave, wave::sine).set (osc2Level, 0.30f).set (osc2Octave, 1)
            .set (filterCutoff, 6000.0f).set (filterResonance, 0.12f).set (filterEnvAmount, 0.35f).set (filtEnvDecay, 110.0f)
            .set (ampDecay, 160.0f).set (ampRelease, 60.0f)
            .set (chordType, chord::min9).set (chordDensity, 4).set (chordInversion, 1)
            .set (seqEnabled, 1.0f).set (seqPattern, pattern::skippy).set (seqSwing, 0.62f).set (seqGate, 0.35f)
            .set (delayMode, dly::pingPong).set (delayDivision, div::n1_8).set (delayFeedback, 0.30f).set (delayMix, 0.18f)
            .set (reverbMode, rev::plate).set (reverbSize, 0.3f).set (reverbDecay, 0.3f).set (reverbMix, 0.12f)
            .preset);

        list.push_back (Builder ("French Filter", "House", "Wide unison saws breathing with the pump. Filter house, disco loops.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.80f)
            .set (osc2Wave, wave::saw).set (osc2Level, 0.60f).set (osc2Detune, 11.0f)
            .set (unisonCount, 5).set (unisonDetune, 18.0f).set (unisonWidth, 0.9f)
            .set (filterCutoff, 2600.0f).set (filterResonance, 0.35f).set (filterEnvAmount, 0.5f).set (filtEnvDecay, 400.0f)
            .set (ampDecay, 450.0f).set (ampSustain, 0.2f).set (ampRelease, 200.0f)
            .set (chordType, chord::maj7).set (chordVoicing, voicing::open).set (chordDensity, 4)
            .set (satType, sat::transistor).set (satDrive, 0.30f)
            .set (chorusMix, 0.20f)
            .set (delayDivision, div::n1_4).set (delayFeedback, 0.25f).set (delayMix, 0.12f)
            .set (reverbMode, rev::plate).set (reverbMix, 0.12f)
            .set (pumpEnabled, 1.0f).set (pumpDepth, 0.6f).set (pumpShape, 0.6f).set (pumpAttack, 6.0f)
            .preset);

        list.push_back (Builder ("Lo-Fi House", "House", "Crushed and wobbly, like a chord sampled off a worn cassette.")
            .set (osc1Wave, wave::triangle).set (osc1Level, 0.80f)
            .set (osc2Wave, wave::organ).set (osc2Level, 0.40f)
            .set (filterCutoff, 4200.0f).set (filterResonance, 0.10f).set (filterEnvAmount, 0.20f)
            .set (ampDecay, 300.0f)
            .set (chordType, chord::min9).set (chordDensity, 4)
            .set (satType, sat::bitcrush).set (satBits, 9.0f).set (satRateReduce, 3.0f).set (satMix, 0.55f)
            .set (chorusEnsemble, 1.0f).set (chorusMix, 0.30f)
            .set (delayDivision, div::n1_8D).set (delayModDepth, 0.7f).set (delayFeedback, 0.35f).set (delayMix, 0.20f).set (delayLowpass, 3000.0f)
            .set (reverbMode, rev::room).set (reverbDamping, 0.75f).set (reverbMix, 0.15f)
            .preset);

        list.push_back (Builder ("Disco Brass Hit", "House", "Pulse and saw with a snappy filter envelope. A horn section in one key.")
            .set (osc1Wave, wave::pulse).set (osc1Level, 0.80f).set (pulseWidth, 0.35f)
            .set (osc2Wave, wave::saw).set (osc2Level, 0.45f).set (osc2Detune, 8.0f)
            .set (unisonCount, 2).set (unisonDetune, 10.0f)
            .set (filterCutoff, 900.0f).set (filterResonance, 0.20f).set (filterEnvAmount, 0.75f)
            .set (filtEnvAttack, 12.0f).set (filtEnvDecay, 220.0f)
            .set (ampAttack, 6.0f).set (ampDecay, 280.0f).set (ampSustain, 0.15f).set (ampRelease, 120.0f)
            .set (chordType, chord::major).set (chordDensity, 3).set (chordOctaveDouble, 1.0f)
            .set (driveAmount, 0.35f)
            .set (chorusEnsemble, 1.0f).set (chorusMix, 0.25f)
            .set (delayMix, 0.10f)
            .set (reverbMode, rev::plate).set (reverbSize, 0.45f).set (reverbMix, 0.18f)
            .preset);

        list.push_back (Builder ("Detroit Strings", "House", "Slow-opening detuned saws with ensemble chorus. Deep, emotional, Detroit.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.75f)
            .set (osc2Wave, wave::saw).set (osc2Level, 0.65f).set (osc2Detune, 12.0f)
            .set (unisonCount, 6).set (unisonDetune, 24.0f).set (unisonWidth, 1.0f)
            .set (filterCutoff, 3000.0f).set (filterResonance, 0.10f).set (filterSlope, filter::slope12).set (filterEnvAmount, 0.20f)
            .set (filtEnvAttack, 30.0f).set (filtEnvDecay, 600.0f).set (filtEnvSustain, 0.5f)
            .set (ampAttack, 12.0f).set (ampDecay, 900.0f).set (ampSustain, 0.35f).set (ampRelease, 450.0f)
            .set (chordType, chord::min9).set (chordVoicing, voicing::open).set (chordSpread, 1).set (chordDensity, 5)
            .set (chorusEnsemble, 1.0f).set (chorusMix, 0.45f).set (chorusDepth, 0.6f).set (chorusRate, 0.3f)
            .set (delayDivision, div::n1_4).set (delayFeedback, 0.30f).set (delayMix, 0.15f)
            .set (reverbMode, rev::hall).set (reverbSize, 0.7f).set (reverbDecay, 0.6f).set (reverbMix, 0.25f)
            .preset);

        //----------------------------------------------------------------------
        // TECHNO
        //----------------------------------------------------------------------
        list.push_back (Builder ("Dub Techno Chord", "Techno", "The Basic Channel chord: dark filtered stab dissolving into a dub delay and a big hall.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.80f)
            .set (osc2Wave, wave::pulse).set (osc2Level, 0.35f).set (osc2Detune, -6.0f).set (pulseWidth, 0.4f)
            .set (filterCutoff, 750.0f).set (filterResonance, 0.30f).set (filterEnvAmount, 0.60f).set (filtEnvDecay, 140.0f).set (filterKeyTrack, 0.2f)
            .set (ampDecay, 260.0f).set (ampRelease, 150.0f)
            .set (chordType, chord::min7).set (chordDensity, 3)
            .set (satDrive, 0.25f)
            .set (delayMode, dly::dub).set (delayDivision, div::n1_8D).set (delayFeedback, 0.68f).set (delayMix, 0.42f)
            .set (delayHighpass, 300.0f).set (delayLowpass, 2800.0f).set (delayModDepth, 0.3f).set (delayDucking, 0.45f)
            .set (reverbMode, rev::hall).set (reverbSize, 0.75f).set (reverbDecay, 0.7f).set (reverbDamping, 0.6f)
            .set (reverbMix, 0.30f).set (reverbLowCut, 300.0f)
            .preset);

        list.push_back (Builder ("Acid Chord", "Techno", "Resonant 24 dB filter slammed by the envelope, with diode grit. A 303 playing chords.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.85f)
            .set (osc2Wave, wave::pulse).set (osc2Level, 0.30f)
            .set (filterCutoff, 420.0f).set (filterResonance, 0.72f).set (filterEnvAmount, 0.85f).set (filtEnvDecay, 180.0f)
            .set (filterKeyTrack, 0.4f).set (filterVelAmount, 0.3f)
            .set (ampDecay, 300.0f).set (ampRelease, 100.0f)
            .set (chordType, chord::minor).set (chordDensity, 3)
            .set (driveAmount, 0.5f)
            .set (satType, sat::diode).set (satDrive, 0.30f).set (satMix, 0.6f)
            .set (delayDivision, div::n1_16D).set (delayFeedback, 0.40f).set (delayMix, 0.20f).set (delayLowpass, 4000.0f)
            .set (reverbMode, rev::room).set (reverbMix, 0.10f)
            .preset);

        list.push_back (Builder ("Industrial Hit", "Techno", "Band-passed, distorted and wide. Warehouse metal.")
            .set (osc1Wave, wave::pulse).set (osc1Level, 0.80f).set (pulseWidth, 0.15f)
            .set (osc2Wave, wave::saw).set (osc2Level, 0.50f).set (osc2Octave, -1).set (osc2Detune, 25.0f)
            .set (unisonCount, 3).set (unisonDetune, 22.0f)
            .set (filterType, filter::bandpass).set (filterSlope, filter::slope12)
            .set (filterCutoff, 1800.0f).set (filterResonance, 0.55f).set (filterEnvAmount, 0.5f).set (filtEnvDecay, 120.0f)
            .set (ampDecay, 200.0f).set (ampRelease, 150.0f)
            .set (chordType, chord::power).set (chordDensity, 2).set (chordOctaveDouble, 1.0f)
            .set (driveAmount, 0.7f)
            .set (satType, sat::diode).set (satDrive, 0.6f).set (satMix, 0.8f)
            .set (delayDivision, div::n1_16).set (delayFeedback, 0.25f).set (delayMix, 0.12f)
            .set (reverbMode, rev::room).set (reverbSize, 0.4f).set (reverbDecay, 0.4f).set (reverbMix, 0.20f).set (reverbLowCut, 200.0f)
            .set (outputWidth, 1.3f)
            .preset);

        list.push_back (Builder ("Hypnotic Sparse", "Techno", "Few hits, long dub echoes. Leave space and let the delay play the rest.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.60f)
            .set (osc2Wave, wave::triangle).set (osc2Level, 0.50f).set (osc2Octave, 1)
            .set (filterCutoff, 1100.0f).set (filterResonance, 0.40f).set (filterEnvAmount, 0.55f).set (filtEnvDecay, 260.0f).set (filterKeyTrack, 0.5f)
            .set (ampDecay, 330.0f)
            .set (chordType, chord::min7).set (chordDensity, 3).set (chordSpread, 1)
            .set (seqEnabled, 1.0f).set (seqPattern, pattern::sparse).set (seqGate, 0.4f)
            .set (delayMode, dly::dub).set (delayDivision, div::n1_4D).set (delayFeedback, 0.60f).set (delayMix, 0.38f)
            .set (delayHighpass, 350.0f).set (delayLowpass, 2400.0f).set (delayModDepth, 0.25f)
            .set (reverbMode, rev::hall).set (reverbSize, 0.8f).set (reverbDecay, 0.75f).set (reverbMix, 0.30f)
            .preset);

        list.push_back (Builder ("Peak Time Stab", "Techno", "Big power chord, pumping and limited. Built to cut through a 130 BPM kick.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.90f)
            .set (osc2Wave, wave::pulse).set (osc2Level, 0.50f).set (osc2Detune, 9.0f)
            .set (unisonCount, 4).set (unisonDetune, 16.0f).set (unisonWidth, 0.85f)
            .set (filterCutoff, 3200.0f).set (filterResonance, 0.25f).set (filterEnvAmount, 0.45f).set (filtEnvDecay, 200.0f)
            .set (ampDecay, 280.0f).set (ampRelease, 120.0f)
            .set (chordType, chord::power).set (chordOctaveDouble, 1.0f)
            .set (driveAmount, 0.3f)
            .set (satType, sat::transistor).set (satDrive, 0.35f)
            .set (delayMode, dly::pingPong).set (delayDivision, div::n1_8D).set (delayFeedback, 0.35f).set (delayMix, 0.15f)
            .set (reverbMode, rev::plate).set (reverbMix, 0.14f)
            .set (pumpEnabled, 1.0f).set (pumpDepth, 0.7f).set (pumpShape, 0.7f).set (pumpAttack, 4.0f)
            .preset);

        list.push_back (Builder ("Minimal Blip", "Techno", "A tiny sine chord bouncing between the speakers. Minimal and clicky.")
            .set (osc1Wave, wave::sine).set (osc1Level, 0.90f)
            .set (osc2Wave, wave::triangle).set (osc2Level, 0.25f).set (osc2Octave, 1)
            .set (filterEnvAmount, 0.0f)
            .set (ampAttack, 0.3f).set (ampDecay, 70.0f).set (ampRelease, 40.0f)
            .set (chordType, chord::sus2).set (chordDensity, 3).set (chordSpread, 1)
            .set (chorusEnabled, 0.0f)
            .set (delayMode, dly::pingPong).set (delayDivision, div::n1_16).set (delayFeedback, 0.45f).set (delayMix, 0.30f)
            .set (delayHighpass, 400.0f).set (delayLowpass, 7000.0f)
            .set (reverbMode, rev::plate).set (reverbSize, 0.3f).set (reverbDecay, 0.25f).set (reverbMix, 0.12f)
            .preset);

        //----------------------------------------------------------------------
        // RAVE
        //----------------------------------------------------------------------
        list.push_back (Builder ("Rave Power Stab", "Rave", "Seven detuned saws on a power chord with an octave below. Hands in the air.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.90f)
            .set (osc2Wave, wave::saw).set (osc2Level, 0.60f).set (osc2Detune, 20.0f)
            .set (unisonCount, 7).set (unisonDetune, 32.0f).set (unisonWidth, 1.0f)
            .set (filterCutoff, 5500.0f).set (filterResonance, 0.15f).set (filterEnvAmount, 0.30f)
            .set (ampDecay, 380.0f).set (ampRelease, 180.0f)
            .set (chordType, chord::power).set (chordOctaveDouble, 1.0f).set (chordSpread, 1)
            .set (driveAmount, 0.45f)
            .set (satType, sat::transistor).set (satDrive, 0.35f)
            .set (delayMode, dly::pingPong).set (delayDivision, div::n1_8D).set (delayFeedback, 0.30f).set (delayMix, 0.15f)
            .set (reverbMode, rev::hall).set (reverbMix, 0.18f)
            .preset);

        list.push_back (Builder ("Hardcore 92", "Rave", "Bright major organ-saw stab with the octave doubled. Breakbeat hardcore.")
            .set (osc1Wave, wave::organ).set (osc1Level, 0.75f)
            .set (osc2Wave, wave::saw).set (osc2Level, 0.45f).set (osc2Detune, 5.0f)
            .set (unisonCount, 3).set (unisonDetune, 9.0f)
            .set (filterCutoff, 7000.0f).set (filterResonance, 0.20f).set (filterEnvAmount, 0.30f).set (filtEnvDecay, 150.0f)
            .set (ampDecay, 240.0f).set (ampRelease, 80.0f)
            .set (chordType, chord::major).set (chordVoicing, voicing::open).set (chordDensity, 4).set (chordOctaveDouble, 1.0f)
            .set (driveAmount, 0.25f)
            .set (satType, sat::transistor).set (satDrive, 0.25f)
            .set (delayDivision, div::n1_16D).set (delayFeedback, 0.20f).set (delayMix, 0.12f)
            .set (reverbMode, rev::room).set (reverbMix, 0.16f)
            .preset);

        list.push_back (Builder ("Trance Gate", "Rave", "Sustained supersaw chopped by a 16th gate. Press play and ride the filter.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.85f)
            .set (osc2Wave, wave::saw).set (osc2Level, 0.70f).set (osc2Detune, 15.0f)
            .set (unisonCount, 7).set (unisonDetune, 26.0f).set (unisonWidth, 1.0f)
            .set (filterCutoff, 5000.0f).set (filterResonance, 0.20f).set (filterEnvAmount, 0.25f).set (filtEnvDecay, 90.0f)
            .set (ampDecay, 120.0f).set (ampSustain, 0.6f).set (ampRelease, 40.0f)
            .set (chordType, chord::sus2).set (chordDensity, 4)
            .set (seqEnabled, 1.0f).set (seqPattern, pattern::custom).set (seqGate, 0.45f)
            .steps ({ 0, 2, 3, 4, 6, 8, 9, 11, 12, 14, 15 })
            .set (delayMode, dly::pingPong).set (delayDivision, div::n1_8).set (delayFeedback, 0.30f).set (delayMix, 0.18f)
            .set (reverbMode, rev::hall).set (reverbMix, 0.22f)
            .preset);

        //----------------------------------------------------------------------
        // OTROS ESTILOS
        //----------------------------------------------------------------------
        list.push_back (Builder ("Afro Pluck", "World", "Soft triangle pluck on a 3+3+2 pattern with a little swing. Afro and organic house.")
            .set (osc1Wave, wave::triangle).set (osc1Level, 0.85f)
            .set (osc2Wave, wave::sine).set (osc2Level, 0.35f).set (osc2Octave, 1)
            .set (filterCutoff, 5000.0f).set (filterResonance, 0.20f).set (filterEnvAmount, 0.5f).set (filtEnvDecay, 80.0f)
            .set (ampAttack, 0.3f).set (ampDecay, 140.0f).set (ampRelease, 70.0f)
            .set (chordType, chord::sus2).set (chordVoicing, voicing::open).set (chordDensity, 3)
            .set (seqEnabled, 1.0f).set (seqPattern, pattern::tresillo).set (seqSwing, 0.56f).set (seqGate, 0.3f)
            .set (delayDivision, div::n1_8D).set (delayFeedback, 0.30f).set (delayMix, 0.18f).set (delayOffset, 0.3f)
            .set (reverbMode, rev::room).set (reverbSize, 0.4f).set (reverbMix, 0.14f)
            .preset);

        list.push_back (Builder ("UK Bass Stab", "Bass", "Square chord with a sub underneath, dry and mono in the lows. Built for a club system.")
            .set (osc1Wave, wave::pulse).set (osc1Level, 0.85f)
            .set (subWave, wave::sine).set (subLevel, 0.6f).set (subOctave, -1)
            .set (filterCutoff, 1600.0f).set (filterResonance, 0.30f).set (filterEnvAmount, 0.5f).set (filtEnvDecay, 130.0f)
            .set (ampDecay, 230.0f).set (ampRelease, 70.0f)
            .set (chordType, chord::min7).set (chordDensity, 3)
            .set (driveAmount, 0.3f)
            .set (delayMix, 0.08f)
            .set (reverbMix, 0.05f)
            .set (outputWidth, 0.6f).set (outputMonoBelow, 180.0f)
            .preset);

        list.push_back (Builder ("Ambient Wash", "Atmos", "A slow, wide major-nine hit that blooms into a huge hall. Still a stab, just a patient one.")
            .set (osc1Wave, wave::saw).set (osc1Level, 0.50f)
            .set (osc2Wave, wave::triangle).set (osc2Level, 0.60f).set (osc2Detune, 7.0f).set (osc2Octave, 1)
            .set (unisonCount, 5).set (unisonDetune, 20.0f).set (unisonWidth, 1.0f)
            .set (filterCutoff, 2200.0f).set (filterResonance, 0.12f).set (filterEnvAmount, 0.15f)
            .set (filtEnvAttack, 40.0f).set (filtEnvDecay, 900.0f)
            .set (ampAttack, 25.0f).set (ampDecay, 1600.0f).set (ampSustain, 0.25f).set (ampRelease, 1200.0f)
            .set (chordType, chord::maj9).set (chordVoicing, voicing::open).set (chordSpread, 2).set (chordDensity, 6)
            .set (chorusEnsemble, 1.0f).set (chorusMix, 0.40f)
            .set (delayMode, dly::pingPong).set (delayDivision, div::n1_4D).set (delayFeedback, 0.50f).set (delayMix, 0.30f).set (delayLowpass, 3000.0f)
            .set (reverbMode, rev::hall).set (reverbSize, 1.0f).set (reverbDecay, 0.9f).set (reverbDamping, 0.55f)
            .set (reverbPreDelayMs, 40.0f).set (reverbMix, 0.45f)
            .preset);

        list.push_back (Builder ("Echo Chamber", "Atmos", "Organ stab into a tape delay on the edge of feedback. Hit Freeze and play over the loop.")
            .set (osc1Wave, wave::organ).set (osc1Level, 0.70f)
            .set (osc2Wave, wave::saw).set (osc2Level, 0.30f).set (osc2Detune, 6.0f)
            .set (filterCutoff, 2400.0f).set (filterResonance, 0.25f).set (filterEnvAmount, 0.40f).set (filtEnvDecay, 150.0f)
            .set (ampDecay, 200.0f).set (ampRelease, 90.0f)
            .set (chordType, chord::min7).set (chordDensity, 4).set (chordInversion, 2)
            .set (satDrive, 0.30f)
            .set (delayMode, dly::dub).set (delayDivision, div::n1_8D).set (delayFeedback, 0.82f).set (delayMix, 0.50f)
            .set (delayHighpass, 400.0f).set (delayLowpass, 2200.0f).set (delayModDepth, 0.45f).set (delayDucking, 0.6f)
            .set (reverbMode, rev::plate).set (reverbMix, 0.20f)
            .preset);

        /*  CALIBRACION DE VOLUMEN (D-046).

            Ganancia de salida medida con SoundAnalysis para que cada preset
            golpee a kTargetHitDb (-18 dB de RMS maximo en 100 ms). No se eligio
            a oido: sin esto, Rave Power Stab sonaba 15 dB mas fuerte que
            Minimal Blip, y pasar de uno a otro en el navegador engana al oido
            (lo mas fuerte siempre parece mejor). El test de presets vuelve a
            medir y falla si alguno se corre mas de 2 dB. */
        const std::pair<const char*, float> calibration[]
        {
            { "Classic Organ Stab", -1.6f }, { "Deep Min9", -1.4f },       { "Garage Swing", 3.6f },
            { "French Filter", 1.4f },       { "Lo-Fi House", 4.7f },     { "Disco Brass Hit", -8.8f },
            { "Detroit Strings", 2.7f },     { "Dub Techno Chord", 0.4f }, { "Acid Chord", -6.9f },
            { "Hypnotic Sparse", 2.5f },     { "Peak Time Stab", -0.2f }, { "Minimal Blip", 7.2f },
            { "Rave Power Stab", -11.6f },   { "Hardcore 92", -4.1f },    { "Trance Gate", -1.7f },
            { "Afro Pluck", 5.2f },          { "UK Bass Stab", -2.6f },   { "Ambient Wash", 1.1f },
        };

        for (auto& preset : list)
            for (const auto& [presetName, gain] : calibration)
                if (preset.name == presetName)
                    preset.values.push_back ({ outputGain, gain });

        return list;
    }
}

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> list = build();
    return list;
}
}
