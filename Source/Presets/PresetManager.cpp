#include "PresetManager.h"
#include "FactoryPresets.h"
#include "../PluginProcessor.h"
#include "../Common/StateVersion.h"

namespace stabr::presets
{
namespace
{
    constexpr size_t kMaxHistory = 64;

    void pushLimited (std::vector<Preset>& stack, Preset p)
    {
        stack.push_back (std::move (p));

        if (stack.size() > kMaxHistory)
            stack.erase (stack.begin());
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state), userDirectory (getDefaultUserDirectory())
{
}

PresetManager::~PresetManager()
{
    // El trabajo en curso usa 'engine': se espera a que termine antes de que
    // se destruya cualquier otro miembro.
    pool.removeAllJobs (true, 10000);
}

//==============================================================================
const std::vector<Preset>& PresetManager::getFactoryPresets() const
{
    return factoryPresets();
}

void PresetManager::loadFactoryPreset (int index)
{
    const auto& list = factoryPresets();

    if (index >= 0 && index < (int) list.size())
        applyWithHistory (list[(size_t) index]);
}

juce::File PresetManager::getDefaultUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Narfbach").getChildFile ("STABR").getChildFile ("Presets");
}

void PresetManager::setUserDirectory (const juce::File& dir)
{
    userDirectory = dir;
}

juce::Array<juce::File> PresetManager::getUserPresetFiles() const
{
    auto files = userDirectory.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
    files.sort();
    return files;
}

juce::Result PresetManager::saveUserPreset (const juce::String& name)
{
    const auto cleanName = name.trim();

    if (cleanName.isEmpty())
        return juce::Result::fail ("The preset needs a name.");

    if (! userDirectory.createDirectory())
        return juce::Result::fail ("Couldn't create the preset folder: " + userDirectory.getFullPathName());

    auto preset = capturePreset (apvts, cleanName);

    juce::XmlElement xml ("STABRPreset");
    xml.setAttribute ("name", cleanName);
    xml.setAttribute ("category", "User");
    xml.setAttribute ("stateVersion", state::kCurrentVersion);
    xml.setAttribute ("pluginVersion", JucePlugin_VersionString);

    for (const auto& v : preset.values)
    {
        auto* child = xml.createNewChildElement ("PARAM");
        child->setAttribute ("id", v.id);
        child->setAttribute ("value", (double) v.value);
    }

    const auto file = userDirectory.getChildFile (juce::File::createLegalFileName (cleanName) + fileExtension);

    if (! xml.writeTo (file))
        return juce::Result::fail ("Couldn't write " + file.getFullPathName());

    currentName = cleanName;

    if (onChange)
        onChange();

    return juce::Result::ok();
}

bool PresetManager::loadUserPreset (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName ("STABRPreset"))
        return false;

    Preset preset;
    preset.name = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    preset.category = "User";

    for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = child->getStringAttribute ("id");

        // Un preset de una version futura puede traer IDs que esta no conoce,
        // y uno manipulado a mano puede traer la sesion: se ignoran los dos.
        if (apvts.getParameter (id) != nullptr && ! isSessionParameter (id))
            preset.values.push_back ({ id, (float) child->getDoubleAttribute ("value") });
    }

    applyWithHistory (preset);
    return true;
}

//==============================================================================
void PresetManager::applyWithHistory (const Preset& preset)
{
    pushLimited (undoStack, capturePreset (apvts, currentName));
    redoStack.clear();

    applyPreset (apvts, preset);
    currentName = preset.name;

    if (onChange)
        onChange();
}

void PresetManager::undo()
{
    if (undoStack.empty())
        return;

    pushLimited (redoStack, capturePreset (apvts, currentName));
    const auto previous = undoStack.back();
    undoStack.pop_back();

    applyPreset (apvts, previous);
    currentName = previous.name;

    if (onChange)
        onChange();
}

void PresetManager::redo()
{
    if (redoStack.empty())
        return;

    pushLimited (undoStack, capturePreset (apvts, currentName));
    const auto next = redoStack.back();
    redoStack.pop_back();

    applyPreset (apvts, next);
    currentName = next.name;

    if (onChange)
        onChange();
}

//==============================================================================
StabrProcessor& PresetManager::analysisEngine()
{
    // Hilo de mensajes: el APVTS arranca un Timer, y crearlo o destruirlo en un
    // hilo de fondo compite con su callback (D-047).
    jassert (juce::MessageManager::getInstanceWithoutCreating() == nullptr
             || juce::MessageManager::getInstanceWithoutCreating()->isThisTheMessageThread());

    if (engine == nullptr)
        engine = std::make_unique<StabrProcessor>();

    return *engine;
}

std::vector<Preset> PresetManager::avoidList() const
{
    auto list = factoryPresets();
    list.insert (list.end(), generated.begin(), generated.end());
    return list;
}

void PresetManager::applyGenerated (const GeneratedStab& result)
{
    applyWithHistory (result.preset);
    pushLimited (generated, result.preset);
}

GeneratedStab PresetManager::createStabNow (juce::int64 seed, GeneratorLocks locks)
{
    StabGenerator generator (apvts, analysisEngine());
    auto result = generator.generate (seed, capturePreset (apvts, currentName), locks, avoidList());
    applyGenerated (result);
    return result;
}

void PresetManager::createStab (GeneratorLocks locks, std::function<void (bool)> onDone)
{
    if (generating.exchange (true))
        return;

    auto* eng = &analysisEngine();
    auto current = capturePreset (apvts, currentName);
    auto avoid = avoidList();

    // Semilla nueva cada vez: reloj de alta resolucion mezclado con un contador,
    // asi dos clics en el mismo milisegundo tampoco repiten.
    const auto seed = juce::Time::getHighResolutionTicks()
                    ^ (++seedCounter * (juce::int64) 0x9E3779B97F4A7C15ull);

    juce::WeakReference<PresetManager> weak (this);

    pool.addJob ([this, weak, eng, current, avoid, locks, seed, onDone]
    {
        StabGenerator generator (apvts, *eng);
        auto result = generator.generate (seed, current, locks, avoid);

        juce::MessageManager::callAsync ([weak, result, onDone]
        {
            auto* self = weak.get();

            if (self == nullptr)
                return;

            self->applyGenerated (result);
            self->generating = false;

            if (onDone)
                onDone (result.accepted);
        });
    });
}
}
