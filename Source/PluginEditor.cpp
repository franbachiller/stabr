#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace stabr
{
namespace
{
    constexpr int kW = gui::sp92::DeviceView::kBaseWidth;
    constexpr int kH = gui::sp92::DeviceView::kBaseHeight;
}

StabrEditor::StabrEditor (StabrProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      view (p),
      vblank (this, [this] { view.tick (juce::Time::getMillisecondCounterHiRes() * 0.001); })
{
    /*  Lo guardado se lee ANTES que nada. setResizeLimits acota el tamano
        inicial (0x0) al minimo y eso dispara resized(), que escribe la escala
        en el procesador: leyendola despues, todo proyecto abria al 75 %. */
    const float savedScale = proc.getUiScale();
    const int savedFinish = proc.getUiFinish();

    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);

    view.onScaleRequested = [this] (float s) { setUiScale (s); };
    view.onFinishRequested = [this] (int f) { setFinish (f); };

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kW * kMinScale), juce::roundToInt (kH * kMinScale),
                     juce::roundToInt (kW * kMaxScale), juce::roundToInt (kH * kMaxScale));

    if (auto* bounds = getConstrainer())
        bounds->setFixedAspectRatio ((double) kW / (double) kH);

    setFinish (savedFinish);
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

void StabrEditor::setFinish (int index)
{
    const int f = juce::jlimit (0, gui::sp92::kNumFinishes - 1, index);
    lookAndFeel.setFinish (gui::sp92::finish (f));
    proc.setUiFinish (f);
    view.setFinish (f);
}

int StabrEditor::getFinish() const noexcept
{
    return proc.getUiFinish();
}

void StabrEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0f0d0b));
}

void StabrEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;

    view.setBounds (0, 0, kW, kH);
    view.setTransform (juce::AffineTransform::scale (scale));

    proc.setUiScale (scale);
}
}
