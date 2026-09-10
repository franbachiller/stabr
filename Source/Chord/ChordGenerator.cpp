#include "ChordGenerator.h"

namespace stabr::chord
{
namespace
{
    constexpr int kScratchSize = 16;   // holgura sobre kMaxChordNotes para pasos intermedios
    constexpr int kMinMidi = 0;
    constexpr int kMaxMidi = 127;

    /*  Division entera hacia abajo, tambien para negativos.
        En C++ -1/7 == 0, y aca necesitamos -1. */
    constexpr int floorDiv (int a, int b) noexcept
    {
        const int q = a / b;
        return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
    }

    /*  Insertion sort: n <= 16 y realtime-safe de forma obvia. */
    void sortAscending (int* v, int n) noexcept
    {
        for (int i = 1; i < n; ++i)
        {
            const int key = v[i];
            int j = i - 1;

            while (j >= 0 && v[j] > key)
            {
                v[j + 1] = v[j];
                --j;
            }

            v[j + 1] = key;
        }
    }

    void removeAt (int* v, int& n, int index) noexcept
    {
        for (int i = index; i < n - 1; ++i)
            v[i] = v[i + 1];

        --n;
    }
}

//==============================================================================
int degreeToSemitones (int degree, ScaleMode mode) noexcept
{
    const int octave = floorDiv (degree, kScaleSize);
    const int index  = degree - octave * kScaleSize;

    return getScaleSteps (mode)[(size_t) index] + 12 * octave;
}

//==============================================================================
ScalePosition quantizeToScale (int midiNote, int keyRoot, ScaleMode mode) noexcept
{
    const auto& steps = getScaleSteps (mode);

    const int rel    = midiNote - keyRoot;
    const int octave = floorDiv (rel, 12);
    const int pc     = rel - octave * 12;   // 0..11

    int bestDegree = 0;
    int bestStep   = steps[0];
    int bestDist   = 128;

    // Candidatos: los 7 grados de esta octava, mas el grado 0 de la siguiente,
    // para que una nota cerca del techo (ej. si con tonica do) suba en vez de
    // caer siete grados hacia abajo.
    for (int i = 0; i < kScaleSize; ++i)
    {
        const int dist = pc - steps[(size_t) i];
        const int abs  = dist < 0 ? -dist : dist;

        if (abs < bestDist)
        {
            bestDist   = abs;
            bestDegree = i;
            bestStep   = steps[(size_t) i];
        }
    }

    {
        const int wrapStep = steps[0] + 12;
        const int dist     = pc - wrapStep;
        const int abs      = dist < 0 ? -dist : dist;

        // Estrictamente menor: ante un empate gana el candidato mas grave.
        if (abs < bestDist)
        {
            bestDegree = kScaleSize;
            bestStep   = wrapStep;
        }
    }

    ScalePosition r;
    r.midiNote = keyRoot + octave * 12 + bestStep;
    r.degree   = octave * kScaleSize + bestDegree;

    return r;
}

//==============================================================================
ChordResult generateChord (int rootMidiNote, const ChordParams& params) noexcept
{
    ChordResult result;

    int v[kScratchSize] {};
    int n = 0;

    //--------------------------------------------------------------------
    // 1. Fundamental: cuantizada a la escala salvo que el lock este en Off.
    //--------------------------------------------------------------------
    int root       = rootMidiNote;
    int rootDegree = 0;
    bool diatonic  = false;

    if (params.lock != ScaleLock::off)
    {
        const auto q = quantizeToScale (rootMidiNote, params.keyRoot, params.mode);
        root       = q.midiNote;
        rootDegree = q.degree;
        diatonic   = (params.lock == ScaleLock::diatonic);
    }

    //--------------------------------------------------------------------
    // 2. Alturas.
    //--------------------------------------------------------------------
    const auto& degRecipe = getDegreeRecipe (params.type);

    if (diatonic && ! degRecipe.alwaysLiteral)
    {
        // Apilar GRADOS de la escala: de aca sale sola la calidad correcta en
        // cada grado, sin tabla de casos especiales. Ver DECISIONS.md D-011.
        const int rootSemis = degreeToSemitones (rootDegree, params.mode);

        for (int i = 0; i < degRecipe.count && n < kScratchSize; ++i)
        {
            const int deg = rootDegree + degRecipe.degrees[(size_t) i];
            v[n++] = root + degreeToSemitones (deg, params.mode) - rootSemis;
        }
    }
    else
    {
        const auto& intRecipe = getIntervalRecipe (params.type);

        for (int i = 0; i < intRecipe.count && n < kScratchSize; ++i)
            v[n++] = root + intRecipe.semitones[(size_t) i];
    }

    const int baseCount = n;

    //--------------------------------------------------------------------
    // 3. Densidad.
    //--------------------------------------------------------------------
    const int target = params.density < kMinDensity ? kMinDensity
                     : (params.density > kMaxDensity ? kMaxDensity : params.density);

    /*  Al RECORTAR se saca primero la quinta, no la nota mas aguda.

        La quinta es la nota mas prescindible de un acorde: no define ni la
        calidad ni la extension. Sacar desde arriba le arrancaria la septima a
        un min7 y lo dejaria como triada menor, o sea otro acorde. En todas las
        recetas el indice 2 es la quinta (en power es el indice 1, pero power
        tiene 2 notas y nunca entra por esta rama).
    */
    if (n > target && n >= 4)
        removeAt (v, n, 2);

    while (n > target && n > 2)
        --n;   // ya ordenado por construccion: recortar por arriba

    // Al EXTENDER se agregan copias una octava arriba de las voces graves.
    while (n < target && n < kScratchSize && baseCount > 0)
    {
        v[n] = v[n - baseCount >= 0 ? n - baseCount : 0] + 12;
        ++n;
    }

    sortAscending (v, n);

    //--------------------------------------------------------------------
    // 4. Inversion: la voz mas grave sube una octava, tantas veces como se pida.
    //--------------------------------------------------------------------
    {
        const int inversions = params.inversion < 0 ? 0
                             : (params.inversion > n - 1 ? n - 1 : params.inversion);

        for (int k = 0; k < inversions; ++k)
        {
            const int lowest = v[0];
            removeAt (v, n, 0);
            v[n++] = lowest + 12;
            sortAscending (v, n);
        }
    }

    //--------------------------------------------------------------------
    // 5. Voicing.
    //--------------------------------------------------------------------
    switch (params.voicing)
    {
        case VoicingType::closed:
            break;

        case VoicingType::open:
            // Voces alternadas una octava arriba: la posicion abierta clasica.
            for (int i = 1; i < n; i += 2)
                v[i] += 12;
            break;

        case VoicingType::drop2:
            if (n >= 2)
                v[n - 2] -= 12;
            break;

        case VoicingType::drop3:
            if (n >= 3)
                v[n - 3] -= 12;
            break;

        case VoicingType::numTypes:
        default:
            break;
    }

    sortAscending (v, n);

    //--------------------------------------------------------------------
    // 6. Spread: reparte 'spread' octavas a lo largo del acorde, de forma que
    //    la voz mas aguda se lleva el maximo y la mas grave no se mueve.
    //--------------------------------------------------------------------
    if (params.spread > 0 && n > 1)
    {
        const int denom = n - 1;

        for (int i = 0; i < n; ++i)
            v[i] += 12 * ((params.spread * i) / denom);
    }

    //--------------------------------------------------------------------
    // 7. Fundamental duplicada una octava abajo.
    //--------------------------------------------------------------------
    if (params.doubleRootBelow && n < kScratchSize && root - 12 >= kMinMidi)
        v[n++] = root - 12;

    //--------------------------------------------------------------------
    // 8. Ordenar, descartar fuera de rango y duplicados, truncar.
    //--------------------------------------------------------------------
    sortAscending (v, n);

    for (int i = 0; i < n; ++i)
    {
        const int note = v[i];

        if (note < kMinMidi || note > kMaxMidi)
            continue;

        if (result.count > 0 && result.notes[(size_t) (result.count - 1)] == note)
            continue;   // duplicado: el array ya viene ordenado

        if (result.count >= kMaxChordNotes)
            break;

        result.notes[(size_t) result.count++] = note;
    }

    return result;
}

}
