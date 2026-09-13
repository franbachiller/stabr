#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

#include "HardwareKnob.h"

namespace stabr::gui::sp92
{
/*  Pad STAB: el Create STAB se golpea. */
class StabPad final : public juce::Button
{
public:
    StabPad();
    void flash (double now) { flashAt = now; lastNow = now; repaint(); }
    void tick (double now);

protected:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    double flashAt = -10.0, lastNow = 0.0;
};

/*  Palanca metalica con piloto: los candados de Create STAB. */
class BatSwitch final : public juce::ToggleButton
{
public:
    explicit BatSwitch (const juce::String& title);

protected:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
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
    disco y color. Al cambiar de sonido expulsa y carga. Un clic abre la lista
    de sonidos. */
class FloppyDrive final : public juce::Button
{
public:
    FloppyDrive();

    void setDisk (const juce::String& name, const juce::String& header, juce::Colour colour);
    void tick (double now);

    juce::String getShownName() const { return shownName; }

protected:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::String shownName, shownHeader, pendingName, pendingHeader;
    juce::Colour shownColour { 0xff2a4ea6 }, pendingColour;
    double swapStart = -10.0, lastNow = 0.0;
    bool hasPending = false, firstDisk = true;

    static constexpr double kHalfSwap = 0.14;
};

/*  Vumetro de aguja para la salida. */
class VuMeter final : public juce::Component
{
public:
    VuMeter();

    void push (float peakL, float peakR);
    void tick();
    float getNeedle() const noexcept { return needle; }

    void paint (juce::Graphics&) override;

private:
    float target = 0.0f, needle = 0.0f, drawn = -1.0f;
};

/*  Cinta de enmascarar escrita a mano. Doble clic para editarla. */
class TapeLabel final : public juce::Label
{
public:
    TapeLabel();
    void paint (juce::Graphics&) override;

protected:
    juce::TextEditor* createEditorComponent() override;
};

/*  Selector rotativo de patron con las posiciones serigrafiadas alrededor. */
class PatternSwitch final : public juce::Component
{
public:
    PatternSwitch (const juce::StringArray& positions, const juce::String& title);

    HardwareKnob& getKnob() noexcept { return knob; }

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    HardwareKnob knob;
    juce::StringArray positions;
};
}
