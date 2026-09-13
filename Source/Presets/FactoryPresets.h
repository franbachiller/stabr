#pragma once

#include "Preset.h"

namespace stabr::presets
{
/*  Los 20 presets de fabrica. Cada uno tiene una intencion sonora concreta
    (ver la descripcion) y su volumen calibrado por medicion: todos golpean al
    mismo nivel. Ver DECISIONS.md D-046. */
const std::vector<Preset>& factoryPresets();
}
