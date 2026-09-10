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
