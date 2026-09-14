#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace stabr::gui::sp92
{
/*  Recursos compartidos de la interfaz: las tipografias embebidas y las
    imagenes que se pintan una vez (aluminio de las perillas, textura del
    plastico).

    NO son variables estaticas a proposito (D-053). Un estatico con una
    Typeface o una Image de JUCE vive mas que JUCE: el plugin apaga JUCE al
    destruirse su ultima instancia y esos objetos quedaban retenidos, y FL
    Studio se colgaba al cerrar. Como DeletedAtShutdown, JUCE los libera al
    empezar su propio apagado, mientras todo sigue vivo.

    Solo desde el hilo de mensajes. */
class Sp92Assets final : private juce::DeletedAtShutdown
{
public:
    static Sp92Assets& get()
    {
        if (instance == nullptr)
            instance = new Sp92Assets();

        return *instance;
    }

    ~Sp92Assets() override { instance = nullptr; }

    juce::Typeface::Ptr michroma, archivo600, archivo700, reenieBeanie;
    std::map<int, juce::Image> metalCaps;
    juce::Image noiseTile;

private:
    Sp92Assets() = default;

    inline static Sp92Assets* instance = nullptr;

    JUCE_DECLARE_NON_COPYABLE (Sp92Assets)
};
}
