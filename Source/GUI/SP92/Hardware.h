#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>

#include "Anim.h"
#include "HardwareKnob.h"

namespace stabr::gui::sp92
{
/*  Pad STAB: el Create STAB se golpea. Goma: se hunde en 60 ms, rebota apenas
    al soltarlo y el brillo se apaga con cola. */
class StabPad final : public juce::Button
{
public:
    StabPad();

    void flash (double now);
    void tick (double now, float dt);
    float getDepth() const noexcept { return depth.pos; }

protected:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;

private:
    anim::Spring depth;
    float glow = 0.0f, hover = 0.0f;
    double heldUntil = -10.0, lastNow = 0.0;
};

/*  Palanca metalica con piloto: los candados de Create STAB. Gira sobre la
    tuerca en 120 ms. */
class BatSwitch final : public juce::ToggleButton
{
public:
    explicit BatSwitch (const juce::String& title);
    void tick (float dt);

protected:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    float flip = -1.0f, lamp = -1.0f;
};

/*  LED de encendido que abre el menu de configuracion. */
class PowerButton final : public juce::Button
{
public:
    PowerButton();

protected:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/*  Disquetera con el disquete a la vista: nombre escrito a mano, numero de
    disco y color. Al cambiar de sonido lo expulsa y lo carga con la curva de la
    maqueta (.28 s, cubic-bezier(.3,.7,.3,1)). Un clic abre la lista de sonidos. */
class FloppyDrive final : public juce::Button
{
public:
    FloppyDrive();

    void setDisk (const juce::String& name, const juce::String& header, juce::Colour colour);
    void tick (double now, float dt);

    juce::String getShownName() const { return shownName; }
    bool hitTest (int x, int y) override { return x >= (int) kBayX && y >= 0; }

    static constexpr double kHalfSwap = 0.28;
    static constexpr float kBayX = 16.0f;

protected:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    float outAmount (double t) const;

    juce::String shownName, shownHeader, pendingName, pendingHeader;
    juce::Colour shownColour { 0xff2a4ea6 }, pendingColour;
    double swapStart = -10.0, lastNow = 0.0;
    bool hasPending = false, pendingThisSwap = false, firstDisk = true;
    float led = 0.0f, hover = 0.0f;
};

/*  Vumetro de aguja para la salida, con balistica de un VU real: una aguja
    con masa que tarda ~300 ms y pasa apenas de largo. */
class VuMeter final : public juce::Component
{
public:
    VuMeter();

    void push (float peakL, float peakR);
    void tick (float dt);
    float getNeedle() const noexcept { return needle.pos; }

    void paint (juce::Graphics&) override;

private:
    float peak = 0.0f, envelope = 0.0f, drawn = -1.0f;
    anim::Spring needle;
};

/*  Cinta de enmascarar escrita a mano. Doble clic para editarla. */
class TapeLabel final : public juce::Label
{
public:
    TapeLabel();
    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    juce::TextEditor* createEditorComponent() override;
    void editorShown (juce::TextEditor*) override;

private:
    juce::Rectangle<float> tapeArea() const;
};

/*  Selector rotativo de patron. Las posiciones se serigrafian alrededor, fuera
    del componente (pinta sin recorte), y la elegida se enciende en 150 ms. */
class PatternSwitch final : public juce::Component
{
public:
    PatternSwitch (const juce::StringArray& positions, const juce::String& title);

    HardwareKnob& getKnob() noexcept { return knob; }
    void tick (float dt);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    HardwareKnob knob;
    juce::StringArray positions;
    std::vector<float> amounts;
};

/*  Encabezado de seccion: texto y una linea hasta el final. */
class SectionHead final : public juce::Component
{
public:
    SectionHead();

    void setText (const juce::String&);
    void paint (juce::Graphics&) override;

    static void draw (juce::Graphics&, const juce::String&, juce::Rectangle<float> area, juce::Colour);

private:
    juce::String text;
};
}
