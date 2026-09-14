#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>

/*  Host minimo para el cuelgue de FL Studio al cerrar (D-053).

    Hace lo mismo que un DAW al cerrar un proyecto: carga el VST3, abre y cierra
    el editor dos veces, libera la instancia y apaga JUCE, que descarga la DLL
    del plugin. Si algo del plugin se bloquea al descargarse, este programa no
    termina: Scripts o el test lo corren con un tiempo limite.

    Uso: STABR_UnloadCheck <ruta a STABR.vst3>
*/
namespace
{
    void say (const juce::String& message)
    {
        std::cout << "[" << juce::Time::getMillisecondCounter() << " ms] " << message << std::endl;
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "uso: STABR_UnloadCheck <ruta a STABR.vst3>" << std::endl;
        return 2;
    }

    const auto path = juce::String::fromUTF8 (argv[1]);

    {
        juce::ScopedJuceInitialiser_GUI juceInit;

        {
            juce::VST3PluginFormat format;
            juce::OwnedArray<juce::PluginDescription> types;
            format.findAllTypesForFile (types, path);

            if (types.isEmpty())
            {
                say ("no se encontro el plugin en " + path);
                return 3;
            }

            juce::String error;
            auto instance = format.createInstanceFromDescription (*types[0], 48000.0, 512, error);

            if (instance == nullptr)
            {
                say ("no se pudo crear la instancia: " + error);
                return 4;
            }

            instance->prepareToPlay (48000.0, 512);
            say ("instancia creada: " + instance->getName());

            for (int round = 1; round <= 2; ++round)
            {
                std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorIfNeeded());

                {
                    juce::DocumentWindow window ("STABR unload check", juce::Colours::black, 0);
                    window.setUsingNativeTitleBar (true);
                    window.setContentNonOwned (editor.get(), true);
                    window.setVisible (true);

                    juce::MessageManager::getInstance()->runDispatchLoopUntil (2500);
                    window.clearContentComponent();
                }

                editor.reset();
                say ("editor cerrado, vuelta " + juce::String (round));
            }

            say ("releaseResources...");
            instance->releaseResources();
            say ("destruyendo la instancia (el plugin apaga su copia de JUCE)...");
            instance.reset();
            say ("instancia destruida");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            say ("instancia liberada");
        }

        say ("apagando JUCE: se descarga la DLL del plugin");
    }

    say ("fin limpio");
    return 0;
}
