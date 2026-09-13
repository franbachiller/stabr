#include "StabGenerator.h"
#include "../PluginProcessor.h"
#include "../Params/ParameterIDs.h"

#include <cmath>
#include <map>

namespace stabr::presets
{
namespace
{
    namespace wave  { constexpr int saw = 0, pulse = 1, triangle = 2, organ = 3, sine = 4; }
    namespace chord { constexpr int power = 0, major = 1, minor = 2, sus2 = 3, sus4 = 4, min7 = 5, maj7 = 6,
                                    min9 = 7, maj9 = 8, min11 = 9, sixNine = 10, add9 = 11; }
    namespace dly   { constexpr int stereo = 0, pingPong = 1, dub = 2; }
    namespace rev   { constexpr int plate = 0, room = 1, hall = 2; }
    namespace sat   { constexpr int tape = 0, transistor = 1, diode = 2, bitcrush = 3; }
    namespace div   { constexpr int n1_16 = 4, n1_16D = 6, n1_8 = 7, n1_8D = 9, n1_4 = 10, n1_4D = 12; }

    //==========================================================================
    struct Dice
    {
        juce::Random& r;

        float uni (float lo, float hi)    { return lo + (hi - lo) * r.nextFloat(); }
        float logUni (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); }
        int   integer (int lo, int hi)    { return lo + r.nextInt (hi - lo + 1); }
        bool  chance (float p)            { return r.nextFloat() < p; }

        int pick (std::initializer_list<int> options)
        {
            return *(options.begin() + r.nextInt ((int) options.size()));
        }
    };

    /*  Valores del candidato. Un mapa y no un vector: las reglas de correlacion
        leen y corrigen valores ya sorteados. */
    struct Values
    {
        std::map<juce::String, float> v;

        void  set (const char* id, float value) { v[id] = value; }
        float get (const char* id, float fallback) const
        {
            const auto it = v.find (id);
            return it != v.end() ? it->second : fallback;
        }
    };

    struct Archetype
    {
        const char* name;
        float weight;
        void (*sound) (Dice&, Values&);
        void (*chordPart) (Dice&, Values&);
        void (*fx) (Dice&, Values&);
    };

    //==========================================================================
    const std::vector<Archetype>& archetypes()
    {
        using namespace pid;

        static const std::vector<Archetype> list
        {
            { "Organ", 1.2f,
              [] (Dice& d, Values& v)
              {
                  v.set (osc1Wave, (float) wave::organ); v.set (osc1Level, d.uni (0.75f, 0.9f));
                  if (d.chance (0.5f)) { v.set (osc2Wave, (float) d.pick ({ wave::organ, wave::sine })); v.set (osc2Level, d.uni (0.15f, 0.35f)); v.set (osc2Octave, 1.0f); }
                  v.set (filterCutoff, d.logUni (5000.0f, 12000.0f)); v.set (filterResonance, d.uni (0.05f, 0.2f));
                  v.set (filterEnvAmount, d.uni (0.15f, 0.4f)); v.set (filtEnvDecay, d.uni (100.0f, 250.0f));
                  v.set (ampDecay, d.uni (180.0f, 400.0f)); v.set (ampRelease, d.uni (60.0f, 140.0f));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chordType, (float) d.pick ({ chord::min7, chord::min9, chord::maj7, chord::minor, chord::major }));
                  v.set (chordDensity, (float) d.integer (3, 5));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chorusMix, d.uni (0.1f, 0.3f));
                  v.set (delayMode, (float) d.pick ({ dly::stereo, dly::pingPong })); v.set (delayDivision, (float) d.pick ({ div::n1_8D, div::n1_8, div::n1_16D }));
                  v.set (delayFeedback, d.uni (0.15f, 0.35f)); v.set (delayMix, d.uni (0.08f, 0.2f));
                  v.set (reverbMode, (float) d.pick ({ rev::room, rev::plate })); v.set (reverbMix, d.uni (0.08f, 0.18f));
                  v.set (satDrive, d.uni (0.05f, 0.25f));
              } },

            { "Deep", 1.2f,
              [] (Dice& d, Values& v)
              {
                  v.set (osc1Wave, (float) wave::saw); v.set (osc1Level, d.uni (0.6f, 0.8f));
                  v.set (osc2Wave, (float) wave::saw); v.set (osc2Level, d.uni (0.35f, 0.6f)); v.set (osc2Detune, d.uni (6.0f, 12.0f));
                  v.set (unisonCount, (float) d.integer (2, 4)); v.set (unisonDetune, d.uni (8.0f, 16.0f));
                  v.set (filterCutoff, d.logUni (900.0f, 2200.0f)); v.set (filterResonance, d.uni (0.1f, 0.3f));
                  v.set (filterEnvAmount, d.uni (0.35f, 0.6f)); v.set (filtEnvDecay, d.uni (200.0f, 500.0f));
                  v.set (ampAttack, d.uni (1.0f, 6.0f)); v.set (ampDecay, d.uni (400.0f, 900.0f)); v.set (ampRelease, d.uni (200.0f, 400.0f));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chordType, (float) d.pick ({ chord::min9, chord::maj9, chord::min11, chord::min7 }));
                  v.set (chordVoicing, (float) d.pick ({ 1, 2 })); v.set (chordDensity, (float) d.integer (4, 6));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chorusMix, d.uni (0.2f, 0.4f));
                  v.set (delayDivision, (float) div::n1_8D); v.set (delayFeedback, d.uni (0.3f, 0.45f)); v.set (delayMix, d.uni (0.15f, 0.25f));
                  v.set (delayLowpass, d.logUni (2500.0f, 4500.0f));
                  v.set (reverbMode, (float) rev::hall); v.set (reverbMix, d.uni (0.18f, 0.28f));
              } },

            { "Dub", 1.3f,
              [] (Dice& d, Values& v)
              {
                  v.set (osc1Wave, (float) d.pick ({ wave::saw, wave::pulse })); v.set (osc1Level, d.uni (0.7f, 0.85f));
                  if (d.chance (0.6f)) { v.set (osc2Wave, (float) wave::pulse); v.set (osc2Level, d.uni (0.2f, 0.4f)); v.set (osc2Detune, d.uni (-8.0f, 8.0f)); }
                  v.set (filterCutoff, d.logUni (500.0f, 1200.0f)); v.set (filterResonance, d.uni (0.2f, 0.45f));
                  v.set (filterEnvAmount, d.uni (0.5f, 0.75f)); v.set (filtEnvDecay, d.uni (100.0f, 220.0f));
                  v.set (ampDecay, d.uni (180.0f, 320.0f));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chordType, (float) d.pick ({ chord::min7, chord::minor, chord::min9 }));
                  v.set (chordDensity, (float) d.integer (2, 3));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (delayMode, (float) dly::dub); v.set (delayDivision, (float) d.pick ({ div::n1_8D, div::n1_4D, div::n1_4 }));
                  v.set (delayFeedback, d.uni (0.55f, 0.75f)); v.set (delayMix, d.uni (0.3f, 0.45f));
                  v.set (delayHighpass, d.uni (250.0f, 400.0f)); v.set (delayLowpass, d.logUni (1800.0f, 3200.0f));
                  v.set (delayModDepth, d.uni (0.15f, 0.4f)); v.set (delayDucking, d.uni (0.35f, 0.55f));
                  v.set (reverbMode, (float) rev::hall); v.set (reverbSize, d.uni (0.6f, 0.85f)); v.set (reverbMix, d.uni (0.2f, 0.35f));
                  v.set (reverbLowCut, d.uni (250.0f, 350.0f));
                  v.set (satDrive, d.uni (0.1f, 0.3f));
              } },

            { "Rave", 1.0f,
              [] (Dice& d, Values& v)
              {
                  v.set (osc1Wave, (float) wave::saw); v.set (osc1Level, d.uni (0.8f, 0.9f));
                  v.set (osc2Wave, (float) wave::saw); v.set (osc2Level, d.uni (0.4f, 0.65f)); v.set (osc2Detune, d.uni (10.0f, 25.0f));
                  v.set (unisonCount, (float) d.integer (5, 7)); v.set (unisonDetune, d.uni (20.0f, 35.0f)); v.set (unisonWidth, d.uni (0.85f, 1.0f));
                  v.set (filterCutoff, d.logUni (3000.0f, 8000.0f)); v.set (filterEnvAmount, d.uni (0.2f, 0.4f));
                  v.set (ampDecay, d.uni (250.0f, 450.0f)); v.set (ampRelease, d.uni (100.0f, 200.0f));
                  v.set (driveAmount, d.uni (0.25f, 0.55f));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chordType, (float) d.pick ({ chord::power, chord::major, chord::minor }));
                  v.set (chordOctaveDouble, d.chance (0.7f) ? 1.0f : 0.0f);
              },
              [] (Dice& d, Values& v)
              {
                  v.set (satType, (float) sat::transistor); v.set (satDrive, d.uni (0.2f, 0.4f));
                  v.set (delayMode, (float) dly::pingPong); v.set (delayDivision, (float) div::n1_8D);
                  v.set (delayFeedback, d.uni (0.2f, 0.35f)); v.set (delayMix, d.uni (0.1f, 0.18f));
                  v.set (reverbMode, (float) rev::hall); v.set (reverbMix, d.uni (0.12f, 0.22f));
              } },

            { "Pluck", 1.0f,
              [] (Dice& d, Values& v)
              {
                  v.set (osc1Wave, (float) d.pick ({ wave::triangle, wave::sine, wave::pulse })); v.set (osc1Level, d.uni (0.8f, 0.9f));
                  if (d.chance (0.5f)) { v.set (osc2Wave, (float) wave::sine); v.set (osc2Level, d.uni (0.2f, 0.35f)); v.set (osc2Octave, 1.0f); }
                  v.set (filterCutoff, d.logUni (3000.0f, 9000.0f)); v.set (filterEnvAmount, d.uni (0.3f, 0.6f)); v.set (filtEnvDecay, d.uni (50.0f, 120.0f));
                  v.set (ampAttack, d.uni (0.2f, 1.0f)); v.set (ampDecay, d.uni (60.0f, 160.0f)); v.set (ampRelease, d.uni (30.0f, 80.0f));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chordType, (float) d.pick ({ chord::sus2, chord::sus4, chord::add9, chord::min7, chord::sixNine }));
                  v.set (chordDensity, (float) d.integer (3, 4)); v.set (chordSpread, (float) d.integer (0, 1));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (delayMode, (float) dly::pingPong); v.set (delayDivision, (float) d.pick ({ div::n1_16, div::n1_8, div::n1_16D }));
                  v.set (delayFeedback, d.uni (0.3f, 0.5f)); v.set (delayMix, d.uni (0.18f, 0.32f)); v.set (delayHighpass, d.uni (300.0f, 500.0f));
                  v.set (reverbMode, (float) rev::plate); v.set (reverbMix, d.uni (0.08f, 0.16f));
                  v.set (chorusMix, 0.0f);
              } },

            { "Acid", 0.8f,
              [] (Dice& d, Values& v)
              {
                  v.set (osc1Wave, (float) wave::saw); v.set (osc1Level, d.uni (0.8f, 0.9f));
                  v.set (osc2Wave, (float) wave::pulse); v.set (osc2Level, d.uni (0.2f, 0.35f));
                  v.set (filterCutoff, d.logUni (300.0f, 700.0f)); v.set (filterResonance, d.uni (0.6f, 0.8f)); v.set (filterSlope, 1.0f);
                  v.set (filterEnvAmount, d.uni (0.7f, 0.9f)); v.set (filtEnvDecay, d.uni (120.0f, 250.0f)); v.set (filterKeyTrack, d.uni (0.3f, 0.5f));
                  v.set (ampDecay, d.uni (220.0f, 340.0f));
                  v.set (driveAmount, d.uni (0.3f, 0.55f));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chordType, (float) d.pick ({ chord::minor, chord::power, chord::min7 }));
                  v.set (chordDensity, (float) d.integer (2, 3));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (satType, (float) sat::diode); v.set (satDrive, d.uni (0.2f, 0.4f)); v.set (satMix, d.uni (0.5f, 0.7f));
                  v.set (delayDivision, (float) div::n1_16D); v.set (delayFeedback, d.uni (0.3f, 0.45f)); v.set (delayMix, d.uni (0.12f, 0.22f));
                  v.set (reverbMode, (float) rev::room); v.set (reverbMix, d.uni (0.06f, 0.12f));
              } },

            { "Lo-Fi", 0.8f,
              [] (Dice& d, Values& v)
              {
                  v.set (osc1Wave, (float) d.pick ({ wave::triangle, wave::organ, wave::sine })); v.set (osc1Level, d.uni (0.75f, 0.9f));
                  v.set (osc2Wave, (float) wave::organ); v.set (osc2Level, d.uni (0.2f, 0.45f));
                  v.set (filterCutoff, d.logUni (2500.0f, 5000.0f)); v.set (filterEnvAmount, d.uni (0.1f, 0.3f));
                  v.set (ampDecay, d.uni (200.0f, 400.0f));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chordType, (float) d.pick ({ chord::min9, chord::maj7, chord::min7, chord::sixNine }));
                  v.set (chordDensity, (float) d.integer (4, 5));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (satType, (float) sat::bitcrush); v.set (satBits, (float) d.integer (7, 11));
                  v.set (satRateReduce, (float) d.integer (2, 5)); v.set (satMix, d.uni (0.35f, 0.6f));
                  v.set (delayModDepth, d.uni (0.5f, 0.8f)); v.set (delayFeedback, d.uni (0.25f, 0.4f)); v.set (delayMix, d.uni (0.15f, 0.25f));
                  v.set (delayLowpass, d.logUni (2500.0f, 3500.0f));
                  v.set (chorusEnsemble, 1.0f); v.set (chorusMix, d.uni (0.2f, 0.35f));
                  v.set (reverbMode, (float) rev::room); v.set (reverbDamping, d.uni (0.6f, 0.8f)); v.set (reverbMix, d.uni (0.1f, 0.18f));
              } },

            { "Atmos", 0.7f,
              [] (Dice& d, Values& v)
              {
                  v.set (osc1Wave, (float) wave::saw); v.set (osc1Level, d.uni (0.45f, 0.6f));
                  v.set (osc2Wave, (float) wave::triangle); v.set (osc2Level, d.uni (0.45f, 0.65f)); v.set (osc2Octave, 1.0f); v.set (osc2Detune, d.uni (4.0f, 10.0f));
                  v.set (unisonCount, (float) d.integer (4, 6)); v.set (unisonDetune, d.uni (14.0f, 24.0f)); v.set (unisonWidth, 1.0f);
                  v.set (filterCutoff, d.logUni (1500.0f, 3000.0f)); v.set (filterEnvAmount, d.uni (0.1f, 0.25f));
                  v.set (filtEnvAttack, d.uni (20.0f, 50.0f)); v.set (filtEnvDecay, d.uni (600.0f, 1000.0f));
                  v.set (ampAttack, d.uni (10.0f, 30.0f)); v.set (ampDecay, d.uni (900.0f, 1600.0f));
                  v.set (ampSustain, d.uni (0.15f, 0.3f)); v.set (ampRelease, d.uni (700.0f, 1200.0f));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chordType, (float) d.pick ({ chord::maj9, chord::min9, chord::add9, chord::sixNine }));
                  v.set (chordVoicing, 1.0f); v.set (chordSpread, (float) d.integer (1, 2)); v.set (chordDensity, (float) d.integer (5, 6));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chorusEnsemble, 1.0f); v.set (chorusMix, d.uni (0.3f, 0.45f));
                  v.set (delayMode, (float) dly::pingPong); v.set (delayDivision, (float) div::n1_4D);
                  v.set (delayFeedback, d.uni (0.4f, 0.55f)); v.set (delayMix, d.uni (0.2f, 0.3f)); v.set (delayLowpass, d.logUni (2500.0f, 3500.0f));
                  v.set (reverbMode, (float) rev::hall); v.set (reverbSize, d.uni (0.8f, 1.0f)); v.set (reverbDecay, d.uni (0.75f, 0.9f));
                  v.set (reverbMix, d.uni (0.35f, 0.45f));
              } },

            { "Brass", 0.8f,
              [] (Dice& d, Values& v)
              {
                  v.set (osc1Wave, (float) wave::pulse); v.set (osc1Level, d.uni (0.75f, 0.85f)); v.set (pulseWidth, d.uni (0.3f, 0.45f));
                  v.set (osc2Wave, (float) wave::saw); v.set (osc2Level, d.uni (0.35f, 0.5f)); v.set (osc2Detune, d.uni (5.0f, 10.0f));
                  v.set (filterCutoff, d.logUni (700.0f, 1300.0f)); v.set (filterEnvAmount, d.uni (0.6f, 0.85f));
                  v.set (filtEnvAttack, d.uni (5.0f, 15.0f)); v.set (filtEnvDecay, d.uni (150.0f, 280.0f));
                  v.set (ampAttack, d.uni (3.0f, 8.0f)); v.set (ampDecay, d.uni (220.0f, 340.0f)); v.set (ampSustain, d.uni (0.1f, 0.2f));
                  v.set (driveAmount, d.uni (0.2f, 0.4f));
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chordType, (float) d.pick ({ chord::major, chord::maj7, chord::sus4, chord::min7 }));
                  v.set (chordDensity, 3.0f); v.set (chordOctaveDouble, d.chance (0.5f) ? 1.0f : 0.0f);
              },
              [] (Dice& d, Values& v)
              {
                  v.set (chorusEnsemble, 1.0f); v.set (chorusMix, d.uni (0.15f, 0.3f));
                  v.set (reverbMode, (float) rev::plate); v.set (reverbMix, d.uni (0.12f, 0.2f));
                  v.set (delayMix, d.uni (0.05f, 0.12f));
              } },
        };

        return list;
    }

    //==========================================================================
    /*  Reglas que valen para todos los arquetipos: detalles que nadie elegiria
        al azar y correcciones entre parametros. */
    void applySharedRules (Dice& d, Values& v, bool soundOnly)
    {
        using namespace pid;

        if (! v.v.count (filterSlope))
            v.set (filterSlope, d.chance (0.7f) ? 1.0f : 0.0f);

        v.set (filterKeyTrack, v.get (filterKeyTrack, d.uni (0.2f, 0.5f)));
        v.set (filterVelAmount, d.uni (0.0f, 0.3f));

    // Detalles que los arquetipos no fijan: sin esto quedaban siempre en su
    // default y dos stabs del mismo arquetipo se parecian demasiado.
    if (! v.v.count (pulseWidth))    v.set (pulseWidth, d.uni (0.3f, 0.6f));
    if (! v.v.count (ampAttack))     v.set (ampAttack, d.uni (0.2f, 3.0f));
    if (! v.v.count (filtEnvAttack)) v.set (filtEnvAttack, d.uni (0.2f, 4.0f));

        if (d.chance (0.25f))
        {
            v.set (subWave, (float) wave::sine);
            v.set (subLevel, d.uni (0.2f, 0.45f));
        }

        // Resonancia alta con mucho drive se vuelve un silbido: se modera.
        if (v.get (filterResonance, 0.15f) > 0.5f)
        {
            v.set (driveAmount, juce::jmin (v.get (driveAmount, 0.0f), 0.4f));
            v.set (filterEnvAmount, juce::jmax (v.get (filterEnvAmount, 0.55f), 0.4f));
        }

        // Un filtro muy cerrado sin envolvente no se oye.
        if (v.get (filterCutoff, 12000.0f) < 900.0f)
            v.set (filterEnvAmount, juce::jmax (v.get (filterEnvAmount, 0.55f), 0.45f));

        if (soundOnly)
            return;

        // Muchas voces de unisono ya son un chorus: sumar otro embarra.
        if (v.get (unisonCount, 1.0f) >= 5.0f)
            v.set (chorusMix, juce::jmin (v.get (chorusMix, 0.0f), 0.15f));

        // Feedback alto: el lazo tiene que oscurecerse y hacerle lugar al golpe.
        if (v.get (delayFeedback, 0.45f) > 0.6f)
        {
            v.set (delayLowpass, juce::jmin (v.get (delayLowpass, 6000.0f), 3500.0f));
            v.set (delayDucking, juce::jmax (v.get (delayDucking, 0.4f), 0.4f));
        }

        // Un sonido largo ya ocupa espacio: menos reverb.
        if (v.get (ampDecay, 220.0f) > 600.0f && v.get (reverbMix, 0.2f) > 0.3f)
            v.set (reverbMix, v.get (reverbMix, 0.2f) * 0.8f);

        if (! v.v.count (reverbDecay))   v.set (reverbDecay, d.uni (0.3f, 0.75f));
        if (! v.v.count (reverbDamping)) v.set (reverbDamping, d.uni (0.3f, 0.75f));
        if (! v.v.count (delayModDepth)) v.set (delayModDepth, d.uni (0.05f, 0.3f));
        if (! v.v.count (chorusRate))    v.set (chorusRate, d.logUni (0.2f, 1.5f));
        v.set (reverbPreDelayMs, d.uni (0.0f, 60.0f));
        v.set (delayOffset, d.chance (0.4f) ? d.uni (-0.3f, 0.3f) : 0.0f);

        v.set (fxOrder, d.chance (0.7f) ? 0.0f : (float) d.integer (1, 5));
        v.set (outputLimiter, 1.0f);
        v.set (outputWidth, d.uni (0.8f, 1.2f));
        v.set (outputMonoBelow, d.uni (100.0f, 160.0f));

        if (d.chance (0.2f))
        {
            v.set (pumpEnabled, 1.0f);
            v.set (pumpDepth, d.uni (0.4f, 0.7f));
            v.set (pumpShape, d.uni (0.4f, 0.8f));
            v.set (pumpAttack, d.uni (3.0f, 10.0f));
        }
    }

    void sampleSequencer (Dice& d, Values& v)
    {
        using namespace pid;

        v.set (seqPattern, (float) d.integer (1, 4));   // solo patrones de fabrica: los pasos editables no se pisan
        v.set (seqRate, (float) (d.chance (0.7f) ? 2 : d.pick ({ 1, 4 })));
        v.set (seqSteps, d.chance (0.8f) ? 16.0f : (float) d.pick ({ 12, 14, 15 }));
        v.set (seqSwing, d.chance (0.5f) ? 0.5f : d.uni (0.52f, 0.62f));
        v.set (seqGate, d.uni (0.25f, 0.6f));
    }
}

//==============================================================================
StabGenerator::StabGenerator (juce::AudioProcessorValueTreeState& ref, StabrProcessor& eng)
    : reference (ref), engine (eng)
{
}

Preset StabGenerator::sample (juce::Random& random, const Preset& current, GeneratorLocks locks,
                              juce::String& archetypeName) const
{
    Dice dice { random };

    // Arquetipo por peso.
    const auto& list = archetypes();
    float totalWeight = 0.0f;

    for (const auto& a : list)
        totalWeight += a.weight;

    float roll = dice.uni (0.0f, totalWeight);
    const Archetype* chosen = &list.back();

    for (const auto& a : list)
    {
        if (roll < a.weight)
        {
            chosen = &a;
            break;
        }

        roll -= a.weight;
    }

    archetypeName = chosen->name;

    Values values;
    chosen->sound (dice, values);

    if (! locks.chord)
    {
        chosen->chordPart (dice, values);

        // Variacion dentro del arquetipo: el mismo tipo de acorde con otro
        // bajo u otra disposicion ya es otro sonido.
        if (dice.chance (0.35f))
            values.set (pid::chordInversion, (float) dice.integer (1, 2));

        if (dice.chance (0.25f))
            values.set (pid::chordVoicing, (float) dice.integer (0, 3));
    }

    if (! locks.fx)
        chosen->fx (dice, values);

    applySharedRules (dice, values, locks.fx);

    if (! locks.sequencer)
        sampleSequencer (dice, values);

    Preset result;
    result.category = "Generated";

    // Lo bloqueado se copia tal cual del sonido actual. El encendido del
    // secuenciador tambien: generar un sonido no tiene que arrancar ni parar
    // el patron del usuario.
    for (const auto& pv : current.values)
    {
        const auto group = groupOf (pv.id);
        const bool keep = (group == Group::chord && locks.chord)
                       || (group == Group::fx && locks.fx)
                       || (group == Group::sequencer && (locks.sequencer || pv.id.startsWith ("seq_step_")))
                       || pv.id == pid::seqEnabled;

        if (keep)
            values.v[pv.id] = pv.value;
    }

    for (const auto& [id, value] : values.v)
        result.values.push_back ({ id, value });

    return result;
}

GeneratedStab StabGenerator::generate (juce::int64 seed, const Preset& current, GeneratorLocks locks,
                                       const std::vector<Preset>& avoid)
{
    GeneratedStab best;
    int bestScore = -1;

    /*  La semilla se mezcla antes de usarla. juce::Random es un congruencial
        lineal: con semillas consecutivas la PRIMERA tirada sale casi igual, y
        como la primera tirada elige el arquetipo, el test con semillas
        1000..1039 dio 20 de 40 "Organ" y ni un "Atmos". splitmix64 rompe esa
        correlacion. */
    auto scramble = [] (juce::int64 value)
    {
        auto z = (std::uint64_t) value + 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return (juce::int64) (z ^ (z >> 31));
    };

    const SoundDistance metric (reference);
    std::vector<std::vector<float>> avoidVectors;
    avoidVectors.reserve (avoid.size());

    for (const auto& other : avoid)
        avoidVectors.push_back (metric.vectorOf (other));

    auto nearestTo = [&] (const std::vector<float>& vector)
    {
        float nearest = 1.0f;

        for (const auto& other : avoidVectors)
            nearest = juce::jmin (nearest, metric.between (vector, other));

        return nearest;
    };

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
    {
        /*  UNICIDAD ANTES DEL RENDER.

            El diagnostico de rechazos mostro que los 87 intentos descartados
            en 40 generaciones lo fueron por PARECIDO, ninguno por calidad:
            renderizar 100+ ms para despues descartar por parecido era tiempo
            tirado. Sortear es casi gratis, asi que se sortean hasta kMaxDraws
            candidatos y se renderiza el primero suficientemente distinto, o el
            mas distinto de todos. */
        juce::String archetype;
        Preset candidate;
        float candidateDistance = -1.0f;

        for (int draw = 0; draw < kMaxDraws; ++draw)
        {
            juce::Random random (scramble (seed + (juce::int64) attempt * 7919 + (juce::int64) draw * 104729));
            juce::String drawnArchetype;
            auto drawn = sample (random, current, locks, drawnArchetype);
            const float nearest = nearestTo (metric.vectorOf (drawn));

            if (nearest > candidateDistance)
            {
                candidateDistance = nearest;
                candidate = std::move (drawn);
                archetype = drawnArchetype;
            }

            if (candidateDistance >= kMinDistance)
                break;
        }

        auto report = analyse (engine, candidate);

        if (! report.finite || report.isSilent())
        {
            ++best.rejectedSilent;
            best.attempts = attempt + 1;
            continue;
        }

        /*  Normalizacion con la ganancia de SALIDA, despues de los efectos:
            con el master cambiaria cuanto empujan la saturacion y el delay, y
            el sonido medido dejaria de ser el mismo. Dos pasadas porque el
            limitador no es lineal. */
        auto setGain = [&candidate] (float gain)
        {
            for (auto& pv : candidate.values)
                if (pv.id == pid::outputGain) { pv.value = gain; return; }

            candidate.values.push_back ({ pid::outputGain, gain });
        };

        float gain = 0.0f;

        for (int pass = 0; pass < 2 && std::abs (report.hitDb - kTargetHitDb) > kLoudnessToleranceDb * 0.5f; ++pass)
        {
            gain = juce::jlimit (-24.0f, 12.0f, gain + (kTargetHitDb - report.hitDb));
            setGain (gain);
            report = analyse (engine, candidate);
        }

        const bool loud   = report.finite && std::abs (report.hitDb - kTargetHitDb) <= kLoudnessToleranceDb;
        const bool clean  = report.peak <= 1.0f && report.tailDb < report.hitDb - 6.0f && report.hasMonoBass();

        // Se vuelve a medir: la normalizacion cambio la ganancia de salida.
        const bool unique = nearestTo (metric.vectorOf (candidate)) >= kMinDistance;

        best.rejectedLoudness += loud ? 0 : 1;
        best.rejectedPeak     += report.peak <= 1.0f ? 0 : 1;
        best.rejectedTail     += report.tailDb < report.hitDb - 6.0f ? 0 : 1;
        best.rejectedBass     += report.hasMonoBass() ? 0 : 1;
        best.rejectedSimilar  += unique ? 0 : 1;

        const int score = (loud ? 1 : 0) + (clean ? 1 : 0) + (unique ? 1 : 0);

        // Se conservan los contadores de rechazo al reemplazar el mejor.
        if (score > bestScore)
        {
            bestScore = score;
            best.preset = candidate;
            best.report = report;
            best.archetype = archetype;
        }

        best.attempts = attempt + 1;

        if (loud && clean && unique)
        {
            best.accepted = true;
            break;
        }
    }

    // Nombre legible y unico: arquetipo + los ultimos 4 digitos hex de la semilla.
    best.preset.name = best.archetype + " " + juce::String::toHexString ((int) (seed & 0xFFFF)).paddedLeft ('0', 4).toUpperCase();
    return best;
}
}
