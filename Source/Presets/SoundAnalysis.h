#pragma once

#include "Preset.h"

namespace stabr
{
class StabrProcessor;
}

namespace stabr::presets
{
struct SoundReport
{
    bool  finite = true;
    float peak = 0.0f;            // pico absoluto de la salida
    float hitDb = -120.0f;        // RMS maximo en ventanas de 100 ms del primer medio segundo
    float tailDb = -120.0f;       // RMS entre 2.0 y 2.5 s: cuanta cola deja
    float lowSideDb = -120.0f;    // RMS del LATERAL por debajo de 100 Hz, relativo al golpe

    bool isSilent() const noexcept { return hitDb < -45.0f; }

    /*  Graves en mono: lo que queda de estereo por debajo de 100 Hz tiene que
        estar 24 dB por debajo del golpe. */
    bool hasMonoBass() const noexcept { return lowSideDb < -24.0f; }
};

/*  Volumen al que golpean todos los presets de fabrica y todos los stabs
    generados. Ver DECISIONS.md D-046. */
inline constexpr float kTargetHitDb = -18.0f;

/*  Renderiza un estimulo FIJO y lo mide: acorde sobre La3 (57), 250 ms de
    nota, 2.5 s en total, velocity 0.9, secuenciador apagado.

    Estimulo fijo a proposito: dos sonidos solo se pueden comparar si se los
    toca igual. El secuenciador se apaga porque si no el patron cambiaria
    cuantos golpes caen en la ventana.

    'engine' es un procesador dedicado al analisis, nunca el que suena en el
    DAW. Ver D-047 sobre en que hilo se crea. */
SoundReport analyse (StabrProcessor& engine, const Preset&, double sampleRate = 44100.0);
}
