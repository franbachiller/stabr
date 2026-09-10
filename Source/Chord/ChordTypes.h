#pragma once

#include <array>
#include <cstddef>
#include <initializer_list>

/*  Tipos y tablas del motor de acordes.

    Deliberadamente sin JUCE: esto es C++ puro y se testea sin levantar nada.
    Los nombres legibles viven aca tambien, para que la GUI y el layout de
    parametros lean de una sola fuente y no se desincronicen.
*/
namespace stabr::chord
{

//==============================================================================
enum class ChordType
{
    power = 0,      // 0 5
    majTriad,       // mayor
    minTriad,       // menor
    sus2,
    sus4,
    min7,
    maj7,
    min9,
    maj9,
    min11,
    sixNine,        // 6/9
    add9,
    dim7,
    numTypes
};

enum class ScaleMode
{
    major = 0,
    naturalMinor,
    dorian,
    phrygian,
    harmonicMinor,
    numModes
};

/*  Como interactua el acorde elegido con la tonalidad. Ver DECISIONS.md D-011. */
enum class ScaleLock
{
    off = 0,        // sin tonalidad: el acorde es literal sobre la nota tocada
    diatonic,       // la fundamental se cuantiza Y la calidad la dicta la escala
    literal,        // la fundamental se cuantiza pero el acorde es exactamente el elegido
    numModes
};

enum class VoicingType
{
    closed = 0,
    open,
    drop2,
    drop3,
    numTypes
};

/*  Que hace el plugin cuando llegan notas. Ver DECISIONS.md D-012. */
enum class NoteMode
{
    chord = 0,      // cada nota genera un acorde
    passthrough,    // las notas suenan tal cual
    autoDetect,     // ventana de recoleccion: 1 nota -> acorde, 2+ -> passthrough
    numModes
};

//==============================================================================
inline constexpr int kMaxChordNotes = 7;
inline constexpr int kMinDensity    = 2;   // diadas y power chords de 2 notas son sonidos validos
inline constexpr int kMaxDensity    = kMaxChordNotes;
inline constexpr int kScaleSize     = 7;   // las cinco escalas son heptatonicas

//==============================================================================
/*  Resultado del generador. Tamano fijo: nada de esto asigna memoria, porque
    se construye desde el hilo de audio al procesar un note-on. */
struct ChordResult
{
    std::array<int, kMaxChordNotes> notes {};
    int count = 0;

    int  operator[] (int i) const noexcept { return notes[(size_t) i]; }
    bool isEmpty() const noexcept          { return count <= 0; }
};

//==============================================================================
/*  Receta literal: semitonos desde la fundamental. */
struct IntervalRecipe
{
    std::array<int, kMaxChordNotes> semitones {};
    int count = 0;
};

/*  Receta diatonica: grados de la escala desde el grado de la fundamental.
    Apilar grados en vez de semitonos es lo que hace que la calidad salga sola
    de la tonalidad: apilar terceras sobre el grado I de menor natural da min7,
    y sobre el grado III da maj7, sin ninguna tabla de casos especiales. */
struct DegreeRecipe
{
    std::array<int, kMaxChordNotes> degrees {};
    int count = 0;
    bool alwaysLiteral = false;   // ver nota sobre power y dim7 mas abajo
};

//==============================================================================
inline const IntervalRecipe& getIntervalRecipe (ChordType t) noexcept
{
    static const std::array<IntervalRecipe, (size_t) ChordType::numTypes> table = []
    {
        std::array<IntervalRecipe, (size_t) ChordType::numTypes> a {};

        auto set = [&a] (ChordType t2, std::initializer_list<int> semis)
        {
            auto& r = a[(size_t) t2];
            r.count = 0;

            for (int s : semis)
                if (r.count < kMaxChordNotes)
                    r.semitones[(size_t) r.count++] = s;
        };

        set (ChordType::power,    { 0, 7 });
        set (ChordType::majTriad, { 0, 4, 7 });
        set (ChordType::minTriad, { 0, 3, 7 });
        set (ChordType::sus2,     { 0, 2, 7 });
        set (ChordType::sus4,     { 0, 5, 7 });
        set (ChordType::min7,     { 0, 3, 7, 10 });
        set (ChordType::maj7,     { 0, 4, 7, 11 });
        set (ChordType::min9,     { 0, 3, 7, 10, 14 });
        set (ChordType::maj9,     { 0, 4, 7, 11, 14 });
        set (ChordType::min11,    { 0, 3, 7, 10, 14, 17 });
        set (ChordType::sixNine,  { 0, 4, 7, 9, 14 });
        set (ChordType::add9,     { 0, 4, 7, 14 });
        set (ChordType::dim7,     { 0, 3, 6, 9 });

        return a;
    }();

    return table[(size_t) t];
}

//==============================================================================
inline const DegreeRecipe& getDegreeRecipe (ChordType t) noexcept
{
    static const std::array<DegreeRecipe, (size_t) ChordType::numTypes> table = []
    {
        std::array<DegreeRecipe, (size_t) ChordType::numTypes> a {};

        auto set = [&a] (ChordType t2, std::initializer_list<int> degs, bool literal = false)
        {
            auto& r = a[(size_t) t2];
            r.count = 0;
            r.alwaysLiteral = literal;

            for (int d : degs)
                if (r.count < kMaxChordNotes)
                    r.degrees[(size_t) r.count++] = d;
        };

        /*  DOS EXCEPCIONES DELIBERADAS, marcadas alwaysLiteral:

            - power: una quinta justa por definicion. Apilarlo por grados da un
              tritono en los grados donde la quinta diatonica es disminuida
              (VII en mayor, II en frigio), y eso no es un power chord: es un
              acorde roto.
            - dim7: es simetrico (0 3 6 9) y no existe como acorde diatonico
              salvo en un grado puntual del menor armonico. Forzarlo a la
              escala lo destruye.

            En ambos casos, con scale lock en Diatonic se cuantiza la
            fundamental pero el acorde se arma con la receta literal.
        */
        set (ChordType::power,    { 0, 4 }, true);
        set (ChordType::majTriad, { 0, 2, 4 });
        set (ChordType::minTriad, { 0, 2, 4 });
        set (ChordType::sus2,     { 0, 1, 4 });
        set (ChordType::sus4,     { 0, 3, 4 });
        set (ChordType::min7,     { 0, 2, 4, 6 });
        set (ChordType::maj7,     { 0, 2, 4, 6 });
        set (ChordType::min9,     { 0, 2, 4, 6, 8 });
        set (ChordType::maj9,     { 0, 2, 4, 6, 8 });
        set (ChordType::min11,    { 0, 2, 4, 6, 8, 10 });
        set (ChordType::sixNine,  { 0, 2, 4, 5, 8 });
        set (ChordType::add9,     { 0, 2, 4, 8 });
        set (ChordType::dim7,     { 0, 2, 4, 6 }, true);

        return a;
    }();

    return table[(size_t) t];
}

//==============================================================================
/*  Semitonos de cada escala desde la tonica. */
inline const std::array<int, kScaleSize>& getScaleSteps (ScaleMode m) noexcept
{
    static const std::array<std::array<int, kScaleSize>, (size_t) ScaleMode::numModes> table
    {{
        {{ 0, 2, 4, 5, 7, 9, 11 }},   // mayor (jonico)
        {{ 0, 2, 3, 5, 7, 8, 10 }},   // menor natural (eolio)
        {{ 0, 2, 3, 5, 7, 9, 10 }},   // dorico
        {{ 0, 1, 3, 5, 7, 8, 10 }},   // frigio
        {{ 0, 2, 3, 5, 7, 8, 11 }},   // menor armonico
    }};

    return table[(size_t) m];
}

//==============================================================================
// Nombres legibles. Fuente unica para el layout de parametros y la GUI.
inline constexpr std::array<const char*, (size_t) ChordType::numTypes> chordTypeNames
{
    "Power", "Major", "Minor", "Sus2", "Sus4",
    "Min7", "Maj7", "Min9", "Maj9", "Min11", "6/9", "Add9", "Dim7"
};

inline constexpr std::array<const char*, (size_t) ScaleMode::numModes> scaleModeNames
{
    "Major", "Natural Minor", "Dorian", "Phrygian", "Harmonic Minor"
};

inline constexpr std::array<const char*, (size_t) ScaleLock::numModes> scaleLockNames
{
    "Off", "Diatonic", "Literal"
};

inline constexpr std::array<const char*, (size_t) VoicingType::numTypes> voicingNames
{
    "Closed", "Open", "Drop 2", "Drop 3"
};

inline constexpr std::array<const char*, (size_t) NoteMode::numModes> noteModeNames
{
    "Chord", "Passthrough", "Auto"
};

inline constexpr std::array<const char*, 12> keyNames
{
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

}
