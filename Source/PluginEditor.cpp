#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace stabr
{
namespace
{
    constexpr int kW = gui::MainView::kBaseWidth;
    constexpr int kH = gui::MainView::kBaseHeight;
}

StabrEditor::StabrEditor (StabrProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      view (p),
      vblank (this, [this] { view.tick (juce::Time::getMillisecondCounterHiRes() * 0.001); })
{
    /*  La escala guardada se lee ANTES que nada. setResizeLimits acota el
        tamano inicial (0x0) al minimo y eso dispara resized(), que escribe la
        escala en el procesador: leyendola despues, todo proyecto abria la
        interfaz al 75 %. Lo encontro el test de persistencia de la escala. */
    const float savedScale = proc.getUiScale();

    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);

    view.onScaleRequested = [this] (float s) { setUiScale (s); };

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kW * kMinScale), juce::roundToInt (kH * kMinScale),
                     juce::roundToInt (kW * kMaxScale), juce::roundToInt (kH * kMaxScale));

    if (auto* bounds = getConstrainer())
        bounds->setFixedAspectRatio ((double) kW / (double) kH);

    setUiScale (savedScale);
}

StabrEditor::~StabrEditor()
{
    setLookAndFeel (nullptr);
}

void StabrEditor::setUiScale (float scale)
{
    const float s = juce::jlimit (kMinScale, kMaxScale, scale);
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
}

void StabrEditor::paint (juce::Graphics& g)
{
    g.fillAll (gui::colours::background);
}

void StabrEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;

    // La vista se diseña a tamaño base y se escala entera. Ver MainView.h.
    view.setBounds (0, 0, kW, kH);
    view.setTransform (juce::AffineTransform::scale (scale));
    view.setScaleIndicator (scale);

    proc.setUiScale (scale);
}
}
