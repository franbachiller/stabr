#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace stabr
{
/*  Versionado del estado guardado.

    El ValueTree serializado lleva una propiedad stateVersion. migrateState()
    corre siempre al cargar y transforma arboles viejos al formato actual.
    Hoy es identidad, pero el gancho tiene que existir desde la v1: agregarlo
    despues significa que los proyectos guardados con la v1 no tienen numero de
    version y no se pueden distinguir.
*/
namespace state
{
    inline constexpr int kCurrentVersion = 1;

    inline const juce::Identifier versionProperty { "stateVersion" };
    inline const juce::Identifier rootType        { "STABR" };

    /*  Escala de la interfaz (0.75..2). No es un parametro: no se automatiza
        ni le importa al host, pero se guarda con el proyecto. */
    inline const juce::Identifier uiScaleProperty { "ui_scale" };

    /*  Nombre del sonido cargado, para mostrarlo al reabrir el proyecto. */
    inline const juce::Identifier presetNameProperty { "preset_name" };

    /*  Interfaz del SP-92: acabado, candados de Create STAB y texto de la cinta. */
    inline const juce::Identifier uiFinishProperty { "ui_finish" };
    inline const juce::Identifier uiLocksProperty  { "ui_locks" };
    inline const juce::Identifier uiTapeProperty   { "ui_tape" };

    /*  Se llama en el hilo de mensajes, nunca en audio. Puede asignar memoria. */
    inline void migrate (juce::ValueTree& tree)
    {
        if (! tree.isValid())
            return;

        const int version = tree.hasProperty (versionProperty)
                          ? (int) tree.getProperty (versionProperty)
                          : 0;   // 0 = estado pre-versionado

        // switch con fallthrough deliberado: cada caso migra un escalon y cae
        // al siguiente, de forma que un estado v0 pasa por todas las etapas.
        switch (version)
        {
            case 0:
                // Sin migracion: la v1 es la primera version publicada.
                [[fallthrough]];
            case kCurrentVersion:
            default:
                break;
        }

        tree.setProperty (versionProperty, kCurrentVersion, nullptr);
    }
}
}
