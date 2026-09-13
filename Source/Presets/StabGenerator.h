#pragma once

#include "Preset.h"
#include "SoundAnalysis.h"

namespace stabr
{
class StabrProcessor;
}

namespace stabr::presets
{
/*  Partes que Create STAB deja como estan. La tonalidad, la escala y el resto
    de la sesion nunca se tocan: son del tema, no del sonido. */
struct GeneratorLocks
{
    bool chord = false;
    bool fx = false;
    bool sequencer = false;
};

struct GeneratedStab
{
    Preset preset;
    SoundReport report;
    juce::String archetype;
    int attempts = 0;
    bool accepted = false;   // false = ningun intento paso todo; se devuelve el mejor

    // Por que se rechazo cada intento. Un intento puede fallar por varios
    // motivos a la vez. Sirve para ajustar el generador con datos.
    int rejectedSilent = 0, rejectedLoudness = 0, rejectedPeak = 0,
        rejectedTail = 0, rejectedBass = 0, rejectedSimilar = 0;
};

/*  "Create STAB". Ver DECISIONS.md D-047.

    No sortea parametros al azar: eso da ruido casi siempre. Sortea primero un
    ARQUETIPO (organo, deep, dub, rave, pluck, acid, lo-fi, atmos, brass) y
    despues cada parametro dentro de rangos que tienen sentido para ese tipo,
    con reglas de correlacion como las usaria alguien que sabe: filtro cerrado
    pide mas envolvente, resonancia alta pide menos drive, feedback alto pide
    un lazo mas oscuro.

    Cada candidato se RENDERIZA y se mide antes de aceptarse: tiene que sonar,
    no saturar, no dejar una cola que embarre, tener los graves en mono, quedar
    al volumen objetivo y no parecerse a ningun preset ni a ningun stab ya
    generado. Si falla, se sortea otro.

    Deterministico por semilla: la misma semilla da el mismo sonido.
*/
class StabGenerator
{
public:
    static constexpr int   kMaxAttempts = 8;    // candidatos RENDERIZADOS
    static constexpr int   kMaxDraws = 32;      // sorteos por intento, antes de renderizar
    static constexpr float kMinDistance = 0.06f;
    static constexpr float kLoudnessToleranceDb = 1.5f;

    /*  'reference' solo se usa para leer rangos de parametros. 'engine' es el
        procesador de analisis, de uso exclusivo durante la generacion. */
    StabGenerator (juce::AudioProcessorValueTreeState& reference, StabrProcessor& engine);

    GeneratedStab generate (juce::int64 seed, const Preset& current, GeneratorLocks,
                            const std::vector<Preset>& avoid);

    /*  Solo el sorteo, sin render ni verificacion. */
    Preset sample (juce::Random&, const Preset& current, GeneratorLocks, juce::String& archetypeName) const;

private:
    juce::AudioProcessorValueTreeState& reference;
    StabrProcessor& engine;
};
}
