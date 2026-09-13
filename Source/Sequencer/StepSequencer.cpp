#include "StepSequencer.h"

#include <algorithm>
#include <cmath>

namespace stabr::seq
{
void StepSequencer::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    reset();
}

void StepSequencer::reset() noexcept
{
    running = false;
    gateOpen = false;
    gateOffBeats = 0.0;
    expectedStartBeats = 0.0;
    freeRunBeats = 0.0;
    currentStep.store (-1, std::memory_order_relaxed);
}

//==============================================================================
double StepSequencer::stepOnsetBeats (std::int64_t stepIndex, double stepBeats, float swing) noexcept
{
    const double s = std::clamp ((double) swing, 0.5, 0.75);
    double onset = (double) stepIndex * stepBeats;

    if (stepIndex % 2 != 0)
        onset += (s - 0.5) * 2.0 * stepBeats;

    return onset;
}

bool StepSequencer::passesProbability (std::int64_t stepIndex, float probability) noexcept
{
    if (probability >= 0.9999f) return true;
    if (probability <= 0.0001f) return false;

    // splitmix64: barato, sin estado, y bien distribuido incluso para indices
    // consecutivos (un LCG sembrado con el indice no lo es).
    auto z = (std::uint64_t) stepIndex + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z =  z ^ (z >> 31);

    const double u = (double) (z >> 11) * (1.0 / 9007199254740992.0);   // [0, 1)
    return u < (double) probability;
}

int StepSequencer::patternIndexOf (std::int64_t stepIndex) const noexcept
{
    const auto length = (std::int64_t) std::clamp (params.length, 1, kNumSteps);
    return (int) (((stepIndex % length) + length) % length);   // positivo aun con ppq negativo (pre-roll)
}

const Step& StepSequencer::stepAt (std::int64_t stepIndex) const noexcept
{
    const auto& pattern = params.pattern == PatternId::custom ? params.custom
                                                              : factoryPattern (params.pattern);
    return pattern[(size_t) patternIndexOf (stepIndex)];
}

//==============================================================================
void StepSequencer::process (const Transport& t, int numSamples, int latencySamples,
                             EventList& out) noexcept
{
    using Type = Event::Type;

    out.clear();

    if (numSamples <= 0)
        return;

    // Sin posicion del host (Standalone) no hay transporte que respetar: corre
    // libre. Con posicion, el secuenciador sigue al play/stop del host.
    const bool shouldRun = params.enabled && (t.hasPosition ? t.isPlaying : true);

    if (! shouldRun)
    {
        // Parar nunca deja una nota colgada.
        if (gateOpen)
            out.add ({ Type::gateOff, 0, 0.0f, 0 });

        gateOpen = false;
        running = false;
        freeRunBeats = 0.0;   // al volver a encender en modo libre arranca del paso 0
        currentStep.store (-1, std::memory_order_relaxed);
        return;
    }

    const double bpm = std::clamp (t.bpm, 20.0, 999.0);
    const double beatsPerSample = bpm / 60.0 / sampleRate;
    const double samplesPerBeat = 1.0 / beatsPerSample;

    const double hostBeats = t.hasPosition ? t.ppq : freeRunBeats;
    double start = hostBeats - (double) std::max (0, latencySamples) * beatsPerSample;

    if (running)
    {
        /*  CONTIGUO O SALTO.

            Si el bloque empieza donde termino el anterior (con una tolerancia
            para el redondeo del host), se usa la posicion ESPERADA y no la
            informada: asi los intervalos quedan exactamente pegados y un
            disparo en el borde no se duplica ni se pierde.

            Si no, el transporte salto (loop, click en la regla): se cierra la
            nota abierta y se sigue desde la posicion nueva. */
        const double tolerance = 0.010 * bpm / 60.0;   // 10 ms

        if (std::abs (start - expectedStartBeats) <= tolerance)
        {
            start = expectedStartBeats;
        }
        else if (gateOpen)
        {
            out.add ({ Type::gateOff, 0, 0.0f, 0 });
            gateOpen = false;
        }
    }

    running = true;

    const double end = start + (double) numSamples * beatsPerSample;
    const double stepBeats = stepLengthInBeats (params.rate);
    const float  swing = std::clamp (params.swing, 0.5f, 0.75f);
    const double gate = std::clamp ((double) params.gate, 0.01, 1.0);

    /*  Todas las comparaciones se hacen en MUESTRAS, con la misma tolerancia
        para decidir si un disparo cae en el bloque y para calcular su offset.
        Si se decidieran con criterios distintos, un disparo justo en el borde
        podria "caer" en el bloque con un offset igual a numSamples. */
    constexpr double eps = 1.0e-4;
    const double lastPos = (double) numSamples - eps;

    auto positionOf = [start, samplesPerBeat] (double beats) noexcept
    {
        return (beats - start) * samplesPerBeat;
    };

    auto offsetOf = [&] (double beats) noexcept
    {
        return std::clamp ((int) std::floor (positionOf (beats) + eps), 0, numSamples - 1);
    };

    const auto firstIndex = (std::int64_t) std::floor (start / stepBeats) - 2;
    const auto lastIndex  = (std::int64_t) std::floor (end / stepBeats) + 1;

    for (auto idx = firstIndex; idx <= lastIndex; ++idx)
    {
        const double stepStart = stepOnsetBeats (idx, stepBeats, swing);

        if (positionOf (stepStart) >= lastPos)
            break;   // los inicios son monotonos: todo lo que sigue es del bloque siguiente

        const double stepEnd = stepOnsetBeats (idx + 1, stepBeats, swing);

        if (positionOf (stepEnd) < -eps)
            continue;   // el paso entero (con sus ratchets) quedo antes de este bloque

        const auto& step = stepAt (idx);

        if (! step.on || ! passesProbability (idx, step.probability))
            continue;

        // Los ratchets reparten el paso REAL, ya con swing: un paso atrasado es
        // mas corto y sus repeticiones se aprietan con el.
        const int ratchet = std::clamp (step.ratchet, 1, kMaxRatchet);
        const double sub = (stepEnd - stepStart) / (double) ratchet;

        for (int k = 0; k < ratchet; ++k)
        {
            const double on = stepStart + (double) k * sub;
            const double pos = positionOf (on);

            if (pos < -eps || pos >= lastPos)
                continue;

            // Lugar para off + on + el off final: una lista llena nunca puede
            // dejar una nota abierta sin su cierre.
            if (out.count + 3 > kMaxEvents)
                break;

            if (gateOpen)
            {
                // La nota anterior se cierra en su muestra, o en este disparo si
                // todavia seguia abierta (gate largo o ratchet).
                out.add ({ Type::gateOff, offsetOf (std::min (gateOffBeats, on)), 0.0f, 0 });
                gateOpen = false;
            }

            out.add ({ Type::gateOn, offsetOf (on),
                       std::clamp (step.velocity, 0.0f, 1.0f), patternIndexOf (idx) });

            gateOpen = true;
            gateOffBeats = on + gate * sub;
        }
    }

    if (gateOpen && positionOf (gateOffBeats) < lastPos)
    {
        out.add ({ Type::gateOff, offsetOf (std::max (gateOffBeats, start)), 0.0f, 0 });
        gateOpen = false;
    }

    expectedStartBeats = end;

    if (! t.hasPosition)
        freeRunBeats += (double) numSamples * beatsPerSample;

    currentStep.store (patternIndexOf ((std::int64_t) std::floor (start / stepBeats)),
                       std::memory_order_relaxed);
}
}
