#pragma once

#include "ChordTypes.h"

namespace stabr::chord
{

/*  Configuracion completa del generador. Se arma una vez por bloque desde los
    parametros y se pasa por referencia. */
struct ChordParams
{
    ChordType   type            = ChordType::min7;
    ScaleLock   lock            = ScaleLock::diatonic;
    int         keyRoot         = 0;                       // 0..11, C..B
    ScaleMode   mode            = ScaleMode::naturalMinor;
    VoicingType voicing         = VoicingType::closed;
    int         inversion       = 0;                       // 0..3
    int         spread          = 0;                       // 0..3
    bool        doubleRootBelow = false;
    int         density         = 4;                       // 3..7
};

/*  Posicion de una nota dentro de la escala. */
struct ScalePosition
{
    int midiNote = 60;   // la nota cuantizada
    int degree   = 0;    // grado absoluto: puede ser negativo o mayor que 6
};

//==============================================================================
/*  Cuantiza una nota MIDI al grado mas cercano de la escala.

    Empate hacia abajo: entre dos grados equidistantes elige el mas grave, que
    es mas predecible al tocar que un redondeo que cambia de direccion.
*/
ScalePosition quantizeToScale (int midiNote, int keyRoot, ScaleMode mode) noexcept;

/*  Semitonos desde la tonica para un grado absoluto (soporta grados fuera de
    0..6, sumando o restando octavas). */
int degreeToSemitones (int degree, ScaleMode mode) noexcept;

//==============================================================================
/*  EL corazon del plugin: una nota entra, un acorde sale.

    Funcion pura y sin asignaciones: se llama desde el hilo de audio al
    procesar cada note-on. Todo el estado esta en los argumentos, asi que se
    testea sin instanciar nada.

    Orden de las transformaciones, que importa:
      1. cuantizar la fundamental a la escala (si el lock no esta en Off)
      2. armar las alturas (por grados si es diatonico, por semitonos si no)
      3. densidad: recortar o extender hasta la cantidad de voces pedida
      4. inversion
      5. voicing (closed / open / drop2 / drop3)
      6. spread
      7. duplicar la fundamental una octava abajo
      8. ordenar, sacar duplicados y descartar lo que se sale del rango MIDI
*/
ChordResult generateChord (int rootMidiNote, const ChordParams& params) noexcept;

}
