#include "Preset.h"

#include <cmath>
#include <map>

namespace stabr::presets
{
Group groupOf (const juce::String& id) noexcept
{
    static const juce::StringArray session
    {
        "scale_key", "scale_mode", "scale_lock_mode", "note_mode",
        "polyphony", "pitch_bend_range", "hq_mode", "master_gain"
    };

    if (session.contains (id))                       return Group::session;
    if (id.startsWith ("chord_"))                    return Group::chord;
    if (id.startsWith ("seq_"))                      return Group::sequencer;

    if (id.startsWith ("sat_") || id.startsWith ("chorus_") || id.startsWith ("delay_")
        || id.startsWith ("rev_") || id.startsWith ("pump_") || id.startsWith ("out_") || id == "fx_order")
        return Group::fx;

    return Group::sound;
}

namespace
{
    juce::RangedAudioParameter* rangedAt (juce::AudioProcessorParameter* p)
    {
        return dynamic_cast<juce::RangedAudioParameter*> (p);
    }
}

float getPlain (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* p = apvts.getParameter (id))
        return p->convertFrom0to1 (p->getValue());

    jassertfalse;
    return 0.0f;
}

void setPlain (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float normalised = p->convertTo0to1 (plain);

        if (std::abs (normalised - p->getValue()) > 1.0e-6f)
            p->setValueNotifyingHost (normalised);
    }
    else
    {
        jassertfalse;   // ID desconocido en un preset
    }
}

void applyPreset (juce::AudioProcessorValueTreeState& apvts, const Preset& preset)
{
    std::map<juce::String, float> values;

    for (const auto& v : preset.values)
        values[v.id] = v.value;

    for (auto* param : apvts.processor.getParameters())
    {
        auto* ranged = rangedAt (param);

        if (ranged == nullptr || isSessionParameter (ranged->getParameterID()))
            continue;

        const auto it = values.find (ranged->getParameterID());
        const float plain = it != values.end() ? it->second
                                               : ranged->convertFrom0to1 (ranged->getDefaultValue());

        setPlain (apvts, ranged->getParameterID(), plain);
    }
}

Preset capturePreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
{
    Preset p;
    p.name = name;

    for (auto* param : apvts.processor.getParameters())
        if (auto* ranged = rangedAt (param))
            if (! isSessionParameter (ranged->getParameterID()))
                p.values.push_back ({ ranged->getParameterID(), ranged->convertFrom0to1 (ranged->getValue()) });

    return p;
}

SoundDistance::SoundDistance (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* param : apvts.processor.getParameters())
    {
        auto* ranged = rangedAt (param);

        if (ranged == nullptr)
            continue;

        const auto& id = ranged->getParameterID();

        // Los pasos del secuenciador son 64 parametros: pesarian mas que todo
        // el sonido junto. Cuentan solo los globales del secuenciador.
        if (isSessionParameter (id) || id.startsWith ("seq_step_"))
            continue;

        Entry e;
        e.id = id;
        e.param = ranged;
        e.defaultPlain = ranged->convertFrom0to1 (ranged->getDefaultValue());
        e.discrete = dynamic_cast<juce::AudioParameterChoice*> (ranged) != nullptr
                  || dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr;
        entries.push_back (e);
    }
}

std::vector<float> SoundDistance::vectorOf (const Preset& preset) const
{
    std::map<juce::String, float> values;

    for (const auto& v : preset.values)
        values[v.id] = v.value;

    std::vector<float> result;
    result.reserve (entries.size());

    for (const auto& e : entries)
    {
        const auto it = values.find (e.id);
        result.push_back (e.param->convertTo0to1 (it != values.end() ? it->second : e.defaultPlain));
    }

    return result;
}

float SoundDistance::between (const std::vector<float>& a, const std::vector<float>& b) const noexcept
{
    const size_t n = juce::jmin (a.size(), b.size(), entries.size());

    if (n == 0)
        return 0.0f;

    double sum = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        const float diff = std::abs (a[i] - b[i]);
        sum += entries[i].discrete ? (diff > 1.0e-4f ? 1.0 : 0.0) : diff;
    }

    return (float) (sum / (double) n);
}

float distance (juce::AudioProcessorValueTreeState& apvts, const Preset& a, const Preset& b)
{
    const SoundDistance metric (apvts);
    return metric.between (metric.vectorOf (a), metric.vectorOf (b));
}
}
