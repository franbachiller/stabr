#include <juce_core/juce_core.h>
#include "Chord/ChordGenerator.h"
#include <vector>
#include <set>

namespace
{
using namespace stabr::chord;

juce::String toStr (const ChordResult& c)
{
    juce::String s;

    for (int i = 0; i < c.count; ++i)
        s << c[i] << " ";

    return s.trim();
}

/*  Intervalos desde la nota mas grave, para comparar la forma de un acorde sin
    depender de en que octava cayo. */
std::vector<int> intervalsFromBass (const ChordResult& c)
{
    std::vector<int> v;

    for (int i = 0; i < c.count; ++i)
        v.push_back (c[i] - c[0]);

    return v;
}

/*  Clases de altura presentes, que es lo que define la identidad del acorde. */
std::set<int> pitchClasses (const ChordResult& c)
{
    std::set<int> s;

    for (int i = 0; i < c.count; ++i)
        s.insert (((c[i] % 12) + 12) % 12);

    return s;
}

class ChordGeneratorTests : public juce::UnitTest
{
public:
    ChordGeneratorTests() : juce::UnitTest ("ChordGenerator", "Chord") {}

    void runTest() override
    {
        beginTest ("recetas literales: intervalos exactos con lock Off");
        {
            struct Expected { ChordType type; std::vector<int> semis; const char* name; };

            const std::vector<Expected> cases
            {
                { ChordType::power,    { 0, 7 },            "power" },
                { ChordType::majTriad, { 0, 4, 7 },         "mayor" },
                { ChordType::minTriad, { 0, 3, 7 },         "menor" },
                { ChordType::sus2,     { 0, 2, 7 },         "sus2" },
                { ChordType::sus4,     { 0, 5, 7 },         "sus4" },
                { ChordType::min7,     { 0, 3, 7, 10 },     "min7" },
                { ChordType::maj7,     { 0, 4, 7, 11 },     "maj7" },
                { ChordType::min9,     { 0, 3, 7, 10, 14 }, "min9" },
                { ChordType::maj9,     { 0, 4, 7, 11, 14 }, "maj9" },
                { ChordType::min11,    { 0, 3, 7, 10, 14, 17 }, "min11" },
                { ChordType::sixNine,  { 0, 4, 7, 9, 14 },  "6/9" },
                { ChordType::add9,     { 0, 4, 7, 14 },     "add9" },
                { ChordType::dim7,     { 0, 3, 6, 9 },      "dim7" },
            };

            for (const auto& c : cases)
            {
                ChordParams p;
                p.lock = ScaleLock::off;
                p.type = c.type;
                p.density = (int) c.semis.size();   // sin recortar ni extender

                const auto r = generateChord (60, p);
                const auto got = intervalsFromBass (r);

                expect (got == c.semis,
                        juce::String (c.name) + ": esperaba intervalos correctos, obtuve " + toStr (r));
            }
        }

        beginTest ("DIATONICO: los 7 grados de La menor natural dan los acordes correctos");
        {
            /*  ESTE ES EL CRITERIO DE ACEPTACION DE LA FASE 2: una sola tecla
                por grado tiene que dar el acorde diatonico correcto. */
            ChordParams p;
            p.lock = ScaleLock::diatonic;
            p.keyRoot = 9;                      // La
            p.mode = ScaleMode::naturalMinor;
            p.type = ChordType::min7;
            p.density = 4;

            struct Deg { int note; std::vector<int> semis; const char* name; };

            const std::vector<Deg> degrees
            {
                { 57, { 0, 3, 7, 10 }, "I   Am7"    },
                { 59, { 0, 3, 6, 10 }, "II  Bm7b5"  },
                { 60, { 0, 4, 7, 11 }, "III Cmaj7"  },
                { 62, { 0, 3, 7, 10 }, "IV  Dm7"    },
                { 64, { 0, 3, 7, 10 }, "V   Em7"    },
                { 65, { 0, 4, 7, 11 }, "VI  Fmaj7"  },
                { 67, { 0, 4, 7, 10 }, "VII G7"     },
            };

            for (const auto& d : degrees)
            {
                const auto r = generateChord (d.note, p);
                const auto got = intervalsFromBass (r);

                expect (got == d.semis,
                        juce::String (d.name) + ": obtuve " + toStr (r));
            }
        }

        beginTest ("DIATONICO: todo acorde generado cae dentro de la escala");
        {
            // La promesa del scale lock: toques la tecla que toques, y elijas
            // el tipo que elijas, no puede sonar una nota fuera de tonalidad.
            for (int modeIndex = 0; modeIndex < (int) ScaleMode::numModes; ++modeIndex)
            {
                const auto mode = (ScaleMode) modeIndex;

                for (int key = 0; key < 12; ++key)
                {
                    std::set<int> allowed;

                    for (int step : getScaleSteps (mode))
                        allowed.insert (((key + step) % 12 + 12) % 12);

                    for (int typeIndex = 0; typeIndex < (int) ChordType::numTypes; ++typeIndex)
                    {
                        const auto type = (ChordType) typeIndex;

                        // power y dim7 son excepciones declaradas: se mantienen
                        // literales porque su identidad son sus intervalos.
                        if (getDegreeRecipe (type).alwaysLiteral)
                            continue;

                        for (int note = 48; note <= 72; ++note)
                        {
                            ChordParams p;
                            p.lock = ScaleLock::diatonic;
                            p.keyRoot = key;
                            p.mode = mode;
                            p.type = type;
                            p.density = 5;

                            const auto r = generateChord (note, p);

                            for (int pc : pitchClasses (r))
                            {
                                if (allowed.count (pc) == 0)
                                {
                                    expect (false,
                                            juce::String ("nota fuera de escala: pc ") + juce::String (pc)
                                                + " en key " + juce::String (key)
                                                + " modo " + juce::String (modeIndex)
                                                + " tipo " + juce::String (typeIndex)
                                                + " nota " + juce::String (note)
                                                + " -> " + toStr (r));
                                    return;
                                }
                            }
                        }
                    }
                }
            }

            expect (true);
        }

        beginTest ("DIATONICO: la fundamental siempre es un grado de la escala");
        {
            for (int modeIndex = 0; modeIndex < (int) ScaleMode::numModes; ++modeIndex)
            {
                for (int key = 0; key < 12; ++key)
                {
                    std::set<int> allowed;

                    for (int step : getScaleSteps ((ScaleMode) modeIndex))
                        allowed.insert (((key + step) % 12 + 12) % 12);

                    for (int note = 24; note <= 96; ++note)
                    {
                        const auto q = quantizeToScale (note, key, (ScaleMode) modeIndex);
                        const int pc = ((q.midiNote % 12) + 12) % 12;

                        expect (allowed.count (pc) > 0,
                                "cuantizacion fuera de escala: nota " + juce::String (note)
                                    + " -> " + juce::String (q.midiNote));

                        // Nunca se aleja mas de un semitono y medio.
                        expect (std::abs (q.midiNote - note) <= 2,
                                "cuantizacion demasiado lejos: " + juce::String (note)
                                    + " -> " + juce::String (q.midiNote));
                    }
                }
            }
        }

        beginTest ("LITERAL: la fundamental se cuantiza pero el acorde no cambia");
        {
            ChordParams p;
            p.lock = ScaleLock::literal;
            p.keyRoot = 9;
            p.mode = ScaleMode::naturalMinor;
            p.type = ChordType::maj7;
            p.density = 4;

            // Do# no esta en La menor: la fundamental baja a Do, pero el acorde
            // sigue siendo maj7 aunque el grado III ya lo fuera.
            const auto r = generateChord (61, p);
            expectEquals (r[0], 60, "la fundamental no se cuantizo a Do");
            expect (intervalsFromBass (r) == std::vector<int> ({ 0, 4, 7, 11 }),
                    "el acorde dejo de ser maj7 literal: " + toStr (r));

            // Y sobre el grado I, donde lo diatonico seria min7, sigue dando maj7.
            const auto onRoot = generateChord (57, p);
            expect (intervalsFromBass (onRoot) == std::vector<int> ({ 0, 4, 7, 11 }),
                    "Literal deberia ignorar la calidad de la escala: " + toStr (onRoot));
        }

        beginTest ("EXCEPCIONES: power mantiene la quinta justa en todo grado");
        {
            // Apilar por grados daria un tritono en los grados de quinta
            // disminuida (VII en mayor, II en frigio). Eso no es un power chord.
            for (int modeIndex = 0; modeIndex < (int) ScaleMode::numModes; ++modeIndex)
            {
                for (int note = 48; note <= 72; ++note)
                {
                    ChordParams p;
                    p.lock = ScaleLock::diatonic;
                    p.keyRoot = 0;
                    p.mode = (ScaleMode) modeIndex;
                    p.type = ChordType::power;
                    p.density = 3;

                    const auto r = generateChord (note, p);
                    expect (r.count >= 2);
                    expectEquals (r[1] - r[0], 7,
                                  "power sin quinta justa en modo " + juce::String (modeIndex)
                                      + ": " + toStr (r));
                }
            }
        }

        beginTest ("EXCEPCIONES: dim7 mantiene su simetria");
        {
            ChordParams p;
            p.lock = ScaleLock::diatonic;
            p.keyRoot = 9;
            p.mode = ScaleMode::naturalMinor;
            p.type = ChordType::dim7;
            p.density = 4;

            for (int note = 48; note <= 72; ++note)
            {
                const auto r = generateChord (note, p);
                expect (intervalsFromBass (r) == std::vector<int> ({ 0, 3, 6, 9 }),
                        "dim7 perdio su simetria: " + toStr (r));
            }
        }

        beginTest ("DENSIDAD: al recortar se saca la quinta, no la septima");
        {
            /*  La septima define la calidad del acorde; la quinta no aporta
                nada. Recortar desde arriba convertiria un min7 en triada
                menor, o sea en OTRO acorde. */
            ChordParams p;
            p.lock = ScaleLock::off;
            p.type = ChordType::min7;
            p.density = 3;

            const auto r = generateChord (60, p);
            expectEquals (r.count, 3);

            const auto pcs = pitchClasses (r);
            expect (pcs.count (10) > 0, "se perdio la septima menor: " + toStr (r));
            expect (pcs.count (3) > 0,  "se perdio la tercera menor: " + toStr (r));
            expect (pcs.count (7) == 0, "deberia haberse sacado la quinta: " + toStr (r));
        }

        beginTest ("DENSIDAD: la cantidad de voces pedida es la que sale");
        {
            for (int typeIndex = 0; typeIndex < (int) ChordType::numTypes; ++typeIndex)
            {
                for (int d = kMinDensity; d <= kMaxDensity; ++d)
                {
                    ChordParams p;
                    p.lock = ScaleLock::off;
                    p.type = (ChordType) typeIndex;
                    p.density = d;

                    const auto r = generateChord (60, p);

                    expectEquals (r.count, d,
                                  "tipo " + juce::String (typeIndex) + " density " + juce::String (d)
                                      + " dio " + juce::String (r.count) + " voces: " + toStr (r));
                }
            }
        }

        beginTest ("INVERSIONES: mismas clases de altura, distinto bajo");
        {
            ChordParams p;
            p.lock = ScaleLock::off;
            p.type = ChordType::min7;
            p.density = 4;

            const auto root = generateChord (60, p);
            const auto basePcs = pitchClasses (root);

            for (int inv = 1; inv <= 3; ++inv)
            {
                p.inversion = inv;
                const auto r = generateChord (60, p);

                expect (pitchClasses (r) == basePcs,
                        "la inversion " + juce::String (inv) + " cambio las notas: " + toStr (r));
                expect (r[0] > root[0],
                        "la inversion " + juce::String (inv) + " no subio el bajo: " + toStr (r));
            }
        }

        beginTest ("VOICINGS: no cambian la identidad del acorde");
        {
            ChordParams p;
            p.lock = ScaleLock::off;
            p.type = ChordType::min7;
            p.density = 4;

            const auto closedPcs = pitchClasses (generateChord (60, p));

            for (int v = 1; v < (int) VoicingType::numTypes; ++v)
            {
                p.voicing = (VoicingType) v;
                const auto r = generateChord (60, p);

                expect (pitchClasses (r) == closedPcs,
                        juce::String (voicingNames[(size_t) v])
                            + " cambio las notas del acorde: " + toStr (r));
            }

            // Drop2 y Drop3 tienen que BAJAR alguna voz.
            p.voicing = VoicingType::drop2;
            expect (generateChord (60, p)[0] < 60, "drop2 no bajo ninguna voz");

            p.voicing = VoicingType::drop3;
            expect (generateChord (60, p)[0] < 60, "drop3 no bajo ninguna voz");
        }

        beginTest ("SPREAD: separa las voces sin cambiar las notas");
        {
            ChordParams p;
            p.lock = ScaleLock::off;
            p.type = ChordType::min7;
            p.density = 4;

            const auto basePcs = pitchClasses (generateChord (60, p));
            int previousRange = 0;

            for (int s = 0; s <= 3; ++s)
            {
                p.spread = s;
                const auto r = generateChord (60, p);

                expect (pitchClasses (r) == basePcs,
                        "spread " + juce::String (s) + " cambio las notas: " + toStr (r));

                const int range = r[r.count - 1] - r[0];
                expect (range >= previousRange,
                        "spread " + juce::String (s) + " no separo mas que el anterior");
                previousRange = range;
            }
        }

        beginTest ("OCTAVE DOWN agrega la fundamental una octava abajo");
        {
            ChordParams p;
            p.lock = ScaleLock::off;
            p.type = ChordType::min7;
            p.density = 4;

            const auto plain = generateChord (60, p);

            p.doubleRootBelow = true;
            const auto doubled = generateChord (60, p);

            expectEquals (doubled.count, plain.count + 1);
            expectEquals (doubled[0], 48, "no agrego la fundamental una octava abajo");
        }

        beginTest ("ROBUSTEZ: nunca sale del rango MIDI ni repite notas");
        {
            juce::Random rng (0xC40D);

            for (int iteration = 0; iteration < 40000; ++iteration)
            {
                ChordParams p;
                p.type            = (ChordType)   rng.nextInt ((int) ChordType::numTypes);
                p.lock            = (ScaleLock)   rng.nextInt ((int) ScaleLock::numModes);
                p.keyRoot         = rng.nextInt (12);
                p.mode            = (ScaleMode)   rng.nextInt ((int) ScaleMode::numModes);
                p.voicing         = (VoicingType) rng.nextInt ((int) VoicingType::numTypes);
                p.inversion       = rng.nextInt (4);
                p.spread          = rng.nextInt (4);
                p.density         = rng.nextInt ({ kMinDensity, kMaxDensity + 1 });
                p.doubleRootBelow = rng.nextBool();

                const int note = rng.nextInt (128);
                const auto r = generateChord (note, p);

                expect (r.count >= 0 && r.count <= kMaxChordNotes,
                        "cantidad de voces invalida: " + juce::String (r.count));

                for (int i = 0; i < r.count; ++i)
                {
                    if (r[i] < 0 || r[i] > 127)
                    {
                        expect (false, "nota fuera de rango MIDI: " + toStr (r));
                        return;
                    }

                    if (i > 0 && r[i] <= r[i - 1])
                    {
                        expect (false, "notas repetidas o desordenadas: " + toStr (r));
                        return;
                    }
                }
            }

            expect (true);
            logMessage ("  40000 combinaciones aleatorias, todas validas");
        }

        beginTest ("las notas extremas del teclado no rompen nada");
        {
            for (int note : { 0, 1, 2, 125, 126, 127 })
            {
                for (int typeIndex = 0; typeIndex < (int) ChordType::numTypes; ++typeIndex)
                {
                    ChordParams p;
                    p.lock = ScaleLock::diatonic;
                    p.type = (ChordType) typeIndex;
                    p.density = 7;
                    p.spread = 3;
                    p.doubleRootBelow = true;

                    const auto r = generateChord (note, p);

                    for (int i = 0; i < r.count; ++i)
                        expect (r[i] >= 0 && r[i] <= 127,
                                "nota " + juce::String (note) + " tipo " + juce::String (typeIndex)
                                    + " -> fuera de rango: " + toStr (r));
                }
            }
        }
    }
};

static ChordGeneratorTests chordGeneratorTests;
}
