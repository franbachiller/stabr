#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace stabr::presets
{
/*  Un valor de parametro en unidades "planas" (Hz, ms, indice de eleccion),
    no normalizadas: un preset escrito a mano tiene que poder leerse. */
struct ParamValue
{
    juce::String id;
    float value = 0.0f;
};

struct Preset
{
    juce::String name;
    juce::String category;
    juce::String description;
    std::vector<ParamValue> values;   // solo lo que difiere del default

    const ParamValue* find (const juce::String& id) const noexcept
    {
        for (const auto& v : values)
            if (v.id == id)
                return &v;

        return nullptr;
    }
};

/*  A que parte del instrumento pertenece un parametro. Lo usan los presets
    (que no tocan la sesion) y los candados del generador. Ver D-045. */
enum class Group
{
    sound,       // osciladores, unisono, filtro, envolventes, drive
    chord,       // tipo, voicing, inversion, spread, densidad
    fx,          // saturacion, chorus, delay, reverb, pump, salida
    sequencer,   // patron, rate, swing, gate, pasos
    session      // tonalidad, escala, modo de nota, polifonia, bend, HQ, master
};

Group groupOf (const juce::String& paramID) noexcept;

inline bool isSessionParameter (const juce::String& paramID) noexcept
{
    return groupOf (paramID) == Group::session;
}

/*  Carga un preset: todo parametro que no sea de sesion queda en el valor del
    preset o, si el preset no lo menciona, en su DEFAULT. Nunca en lo que
    tuviera antes: si no, un preset sonaria distinto segun desde donde se
    llegue a el. */
void applyPreset (juce::AudioProcessorValueTreeState&, const Preset&);

/*  Estado actual (sin la sesion) como preset completo. */
Preset capturePreset (juce::AudioProcessorValueTreeState&, const juce::String& name);

float getPlain (juce::AudioProcessorValueTreeState&, const juce::String& paramID);
void  setPlain (juce::AudioProcessorValueTreeState&, const juce::String& paramID, float plainValue);

/*  Distancia entre dos sonidos, 0 (identicos) a 1. Promedio de diferencias
    normalizadas sobre los parametros que no son de sesion; las elecciones
    cuentan 0 o 1. Sirve para garantizar que ningun preset ni ningun stab
    generado sea casi igual a otro.

    SoundDistance precalcula la lista de parametros y convierte cada preset a
    un vector una sola vez: Create STAB compara decenas de candidatos contra
    decenas de sonidos, y buscar cada ID en cada preset en cada comparacion
    seria millones de comparaciones de strings. */
class SoundDistance
{
public:
    explicit SoundDistance (juce::AudioProcessorValueTreeState&);

    std::vector<float> vectorOf (const Preset&) const;
    float between (const std::vector<float>& a, const std::vector<float>& b) const noexcept;

private:
    struct Entry
    {
        juce::String id;
        juce::RangedAudioParameter* param = nullptr;
        float defaultPlain = 0.0f;
        bool discrete = false;
    };

    std::vector<Entry> entries;
};

float distance (juce::AudioProcessorValueTreeState&, const Preset& a, const Preset& b);
}
