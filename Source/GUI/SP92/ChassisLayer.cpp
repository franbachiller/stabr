#include "ChassisLayer.h"
#include "Sp92Fonts.h"
#include "../../Common/Utf8.h"

#include <random>

namespace stabr::gui::sp92
{
namespace
{
    /*  Textura del plastico: ruido blanco y negro de baja opacidad, en un
        mosaico. Semilla fija: el aparato se ve igual cada vez que se abre. */
    const juce::Image& noiseTile()
    {
        static const juce::Image tile = []
        {
            juce::Image img (juce::Image::ARGB, 160, 160, true);
            std::mt19937 rng (0x57AB);
            std::uniform_int_distribution<int> dist (-28, 28);

            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                {
                    const int v = dist (rng);
                    img.setPixelAt (x, y, (v >= 0 ? juce::Colours::white : juce::Colours::black).withAlpha ((juce::uint8) std::abs (v)));
                }

            return img;
        }();

        return tile;
    }

    void fillNoise (juce::Graphics& g, const juce::Path& area, float opacity)
    {
        g.setTiledImageFill (noiseTile(), 0, 0, opacity);
        g.fillPath (area);
    }

    void screw (juce::Graphics& g, float x, float y, float degrees)
    {
        const juce::Rectangle<float> head { x, y, 14.0f, 14.0f };
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (head.translated (0.0f, 1.0f));

        juce::ColourGradient metal (juce::Colour (0xfff1efea), x + 5.0f, y + 4.5f, juce::Colour (0xff8c8983), x + 14.0f, y + 14.0f, true);
        g.setGradientFill (metal);
        g.fillEllipse (head);

        const auto c = head.getCentre();
        const float a = degrees * juce::MathConstants<float>::pi / 180.0f;
        g.setColour (juce::Colour (40, 38, 34).withAlpha (0.6f));
        g.drawLine (c.x - std::cos (a) * 5.0f, c.y - std::sin (a) * 5.0f, c.x + std::cos (a) * 5.0f, c.y + std::sin (a) * 5.0f, 2.0f);
    }
}

ChassisLayer::ChassisLayer() : fin (&finish (0))
{
    setInterceptsMouseClicks (false, false);
    setBufferedToImage (true);
    setOpaque (true);
}

void ChassisLayer::setFinish (const Finish& f)
{
    fin = &f;
    repaint();
}

void ChassisLayer::addSilk (const juce::String& text, float x, float y, bool centred, bool small) { silks.push_back ({ text, x, y, centred, small }); }
void ChassisLayer::addHead (const juce::String& text, juce::Rectangle<float> area)                { heads.push_back ({ text, area }); }
void ChassisLayer::addBezel (juce::Rectangle<float> area, const juce::String& tiny)               { bezels.push_back ({ area, tiny }); }
void ChassisLayer::addWell (juce::Rectangle<float> area)                                          { wells.push_back (area); }

void ChassisLayer::paint (juce::Graphics& g)
{
    const auto& f = *fin;
    const auto b = getLocalBounds().toFloat();
    constexpr float corner = 24.0f;

    // El escritorio detras de las esquinas redondeadas.
    g.fillAll (juce::Colour (0xff0f0d0b));

    //--------------------------------------------------------------------------
    // Carcasa
    //--------------------------------------------------------------------------
    juce::Path body;
    body.addRoundedRectangle (b, corner);

    juce::ColourGradient plastic (f.chassisHi, 0.0f, 0.0f, f.chassisLo, 0.0f, b.getBottom(), false);
    plastic.addColour (0.14, f.chassis);
    plastic.addColour (0.86, f.chassis);
    g.setGradientFill (plastic);
    g.fillPath (body);
    fillNoise (g, body, 0.55f);

    juce::ColourGradient bottomShade (juce::Colours::transparentBlack, 0.0f, b.getBottom() - 12.0f,
                                      juce::Colours::black.withAlpha (0.2f), 0.0f, b.getBottom(), false);
    g.setGradientFill (bottomShade);
    g.fillPath (body);

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.strokePath (body, juce::PathStrokeType (1.0f));

    //--------------------------------------------------------------------------
    // Franja superior
    //--------------------------------------------------------------------------
    juce::Path strip;
    strip.addRoundedRectangle (0.0f, 0.0f, b.getWidth(), 92.0f, corner, corner, true, true, false, false);
    juce::ColourGradient stripGrad (f.strip.interpolatedWith (juce::Colours::white, 0.2f), 0.0f, 0.0f, f.strip, 0.0f, 15.0f, false);
    g.setGradientFill (stripGrad);
    g.fillPath (strip);
    fillNoise (g, strip, 0.4f);

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (corner, 1.5f, b.getWidth() - corner, 1.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (0.0f, 92.0f, b.getWidth(), 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillRect (0.0f, 94.0f, b.getWidth(), 1.0f);

    g.setColour (f.stripInk);
    g.setFont (fonts::display (34.0f).withExtraKerningFactor (0.2f));
    g.drawText ("STABR", juce::Rectangle<float> (40.0f, 18.0f, 230.0f, 48.0f), juce::Justification::centredLeft, false);

    const juce::Rectangle<float> badge { 268.0f, 24.0f, 80.0f, 25.0f };
    g.setColour (f.stab);
    g.fillRoundedRectangle (badge, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.fillRect (badge.withTop (badge.getBottom() - 2.0f).reduced (2.0f, 0.0f));
    g.setColour (juce::Colour (0xff1a0d05));
    g.setFont (fonts::display (13.0f).withExtraKerningFactor (0.12f));
    g.drawText ("SP-92", badge, juce::Justification::centred, false);

    g.setColour (f.stripInk);
    g.setFont (fonts::label (11.0f).withExtraKerningFactor (0.2f));
    g.drawText ("POLYPHONIC CHORD STAB SAMPLER", juce::Rectangle<float> (362.0f, 22.0f, 256.0f, 14.0f), juce::Justification::centredLeft, false);
    g.setColour (f.stripInk.withAlpha (0.6f));
    g.setFont (fonts::label (9.0f).withExtraKerningFactor (0.3f));
    g.drawText (utf8 ("16 STEPS \xc2\xb7 20 DISKS \xc2\xb7 DUB DELAY"), juce::Rectangle<float> (362.0f, 40.0f, 260.0f, 12.0f), juce::Justification::centredLeft, false);

    //--------------------------------------------------------------------------
    // Marcos de pantalla, pozos y encabezados
    //--------------------------------------------------------------------------
    for (const auto& bezel : bezels)
    {
        const auto r = bezel.area;
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 10.0f);
        juce::ColourGradient frame (juce::Colour (0xff25221f), 0.0f, r.getY(), juce::Colour (0xff151311), 0.0f, r.getBottom(), false);
        g.setGradientFill (frame);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawLine (r.getX() + 10.0f, r.getY() + 1.5f, r.getRight() - 10.0f, r.getY() + 1.5f, 1.0f);

        if (bezel.tiny.isNotEmpty())
        {
            g.setColour (juce::Colour (0xff8b8378));
            g.setFont (fonts::label (8.0f).withExtraKerningFactor (0.24f));
            g.drawText (bezel.tiny, juce::Rectangle<float> (r.getX() + 16.0f, r.getBottom() - 14.0f, 300.0f, 10.0f), juce::Justification::centredLeft, false);
        }
    }

    for (const auto& w : wells)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRoundedRectangle (w.translated (0.0f, 2.0f), 12.0f);
        juce::ColourGradient well (f.well.interpolatedWith (juce::Colours::black, 0.15f), 0.0f, w.getY(), f.well, 0.0f, w.getBottom(), false);
        g.setGradientFill (well);
        g.fillRoundedRectangle (w, 12.0f);
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.4f), 0.0f, w.getY(), juce::Colours::transparentBlack, 0.0f, w.getY() + 12.0f, false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (w, 12.0f);
    }

    // Costura entre las dos mitades del panel.
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (20.0f, 470.0f, b.getWidth() - 40.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillRect (20.0f, 472.0f, b.getWidth() - 40.0f, 1.0f);

    for (const auto& h : heads)
    {
        const auto font = fonts::label (11.0f).withExtraKerningFactor (0.24f);
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, h.text, 0.0f, 0.0f);
        const float textW = ga.getBoundingBox (0, -1, true).getWidth();

        g.setColour (f.silk);
        g.setFont (font);
        g.drawText (h.text, h.area.withWidth (textW + 4.0f), juce::Justification::centredLeft, false);
        g.setColour (f.silk.withAlpha (0.35f));
        g.fillRect (h.area.getX() + textW + 12.0f, h.area.getCentreY(), h.area.getWidth() - textW - 12.0f, 1.0f);
    }

    //--------------------------------------------------------------------------
    // Serigrafia de controles
    //--------------------------------------------------------------------------
    for (const auto& s : silks)
    {
        g.setColour (s.small ? f.silkMuted : f.silk);
        g.setFont (s.small ? fonts::label (9.0f).withExtraKerningFactor (0.14f) : fonts::label (11.0f).withExtraKerningFactor (0.16f));
        const juce::Rectangle<float> area = s.centred ? juce::Rectangle<float> (s.x - 60.0f, s.y, 120.0f, 13.0f)
                                                      : juce::Rectangle<float> (s.x, s.y, 120.0f, 13.0f);
        g.drawText (s.text, area, s.centred ? juce::Justification::centredTop : juce::Justification::topLeft, false);
    }

    //--------------------------------------------------------------------------
    // Placa de serie y tornillos
    //--------------------------------------------------------------------------
    const juce::Rectangle<float> plate { 936.0f, 612.0f, 136.0f, 74.0f };
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (plate.translated (0.0f, 2.0f), 4.0f);
    juce::ColourGradient brushed (juce::Colour (0xffe7e5df), plate.getX(), plate.getY(), juce::Colour (0xffd4d1ca), plate.getRight(), plate.getBottom(), false);
    brushed.addColour (0.55, juce::Colour (0xffa9a69f));
    g.setGradientFill (brushed);
    g.fillRoundedRectangle (plate, 4.0f);
    g.setColour (juce::Colours::white);
    g.drawLine (plate.getX() + 4.0f, plate.getY() + 1.0f, plate.getRight() - 4.0f, plate.getY() + 1.0f, 1.0f);

    g.setColour (juce::Colour (0xff3a3832));
    g.setFont (fonts::display (10.0f).withExtraKerningFactor (0.1f));
    g.drawText ("NARFBACH", plate.reduced (12.0f, 9.0f).withHeight (13.0f), juce::Justification::centredLeft, false);
    g.setFont (fonts::label (8.5f).withExtraKerningFactor (0.16f));
    const char* lines[] { "STABR SP-92", "S/N 0092-0417", "BUILT FOR THE CLUB" };
    for (int i = 0; i < 3; ++i)
        g.drawText (lines[i], juce::Rectangle<float> (plate.getX() + 12.0f, plate.getY() + 24.0f + (float) i * 12.5f, 120.0f, 12.0f), juce::Justification::centredLeft, false);

    screw (g, 10.0f, 10.0f, 25.0f);
    screw (g, b.getWidth() - 24.0f, 10.0f, -40.0f);
    screw (g, 10.0f, b.getHeight() - 24.0f, 70.0f);
    screw (g, b.getWidth() - 24.0f, b.getHeight() - 24.0f, 10.0f);
}
}
