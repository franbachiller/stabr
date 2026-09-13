#include "ChassisLayer.h"
#include "Anim.h"
#include "Hardware.h"
#include "Sp92Fonts.h"
#include "../../Common/Utf8.h"

#include <random>

namespace stabr::gui::sp92
{
namespace
{
    /*  Textura del plastico: el ruido de la maqueta (gris 128 +/- 32 en
        soft-light), como luces y sombras de baja opacidad. Semilla fija: el
        aparato se ve igual cada vez que se abre. */
    const juce::Image& noiseTile()
    {
        static const juce::Image tile = []
        {
            juce::Image img (juce::Image::ARGB, 180, 180, true);
            std::mt19937 rng (0x57AB);
            std::uniform_int_distribution<int> dist (-32, 32);

            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                {
                    const int v = dist (rng);
                    img.setPixelAt (x, y, (v >= 0 ? juce::Colours::white : juce::Colours::black).withAlpha ((juce::uint8) (std::abs (v) * 0.8f)));
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
        anim::blurredEllipse (g, head.translated (0.0f, 1.0f), juce::Colours::black.withAlpha (0.45f), 1.0f);

        juce::ColourGradient metal (juce::Colour (0xfff1efea), x + 14.0f * 0.38f, y + 14.0f * 0.32f,
                                    juce::Colour (0xff8c8983), x + 14.0f * 0.38f + 11.3f * 0.7f / 0.7f, y + 14.0f * 0.32f, true);
        g.setGradientFill (metal);
        g.fillEllipse (head);

        {
            juce::Path clip, shade;
            clip.addEllipse (head);
            shade.addEllipse (head);
            shade.addEllipse (head.translated (0.0f, -1.0f));
            shade.setUsingNonZeroWinding (false);
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (clip);
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillPath (shade);
        }

        juce::Path slot;
        slot.addRectangle (x + 2.0f, y + 6.0f, 10.0f, 2.0f);
        slot.applyTransform (juce::AffineTransform::rotation (degrees * juce::MathConstants<float>::pi / 180.0f, x + 7.0f, y + 7.0f));
        g.setColour (juce::Colour (40, 38, 34).withAlpha (0.55f));
        g.fillPath (slot);
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
    g.fillAll (juce::Colour (0xff17130f));

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
    fillNoise (g, body, 0.9f);

    {
        // inset 0 -5px 0 .18, inset 0 0 0 1px .25
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (body);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRect (b.withTop (b.getBottom() - 5.0f));
    }

    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.strokePath (body, juce::PathStrokeType (1.0f), juce::AffineTransform());

    //--------------------------------------------------------------------------
    // Franja superior
    //--------------------------------------------------------------------------
    juce::Path strip;
    strip.addRoundedRectangle (0.0f, 0.0f, b.getWidth(), 92.0f, corner, corner, true, true, false, false);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillRect (0.0f, 92.0f, b.getWidth(), 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (0.0f, 92.0f, b.getWidth(), 2.0f);

    juce::ColourGradient stripGrad (f.strip.interpolatedWith (juce::Colours::white, 0.2f), 0.0f, 0.0f, f.strip, 0.0f, 92.0f * 0.16f, false);
    g.setGradientFill (stripGrad);
    g.fillPath (strip);
    fillNoise (g, strip, 0.9f);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (strip);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRect (0.0f, 0.0f, b.getWidth(), 2.0f);
    }

    g.setColour (f.stripInk);
    fonts::drawText (g, "STABR", fonts::display (34.0f, 0.2f), { 40.0f, 24.0f, 240.0f, 34.0f }, juce::Justification::centredLeft);

    // Placa del modelo: padding 5 9 4, letra de 13 px.
    {
        const auto font = fonts::display (13.0f, 0.12f);
        const float w = fonts::textWidth (font, "SP-92");
        const juce::Rectangle<float> badge { 268.0f, 25.0f, w + 18.0f, 22.0f };

        g.setColour (f.stab);
        g.fillRoundedRectangle (badge, 3.0f);
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (badge, 3.0f);
            g.reduceClipRegion (clip);
            g.setColour (juce::Colours::black.withAlpha (0.2f));
            g.fillRect (badge.withTop (badge.getBottom() - 2.0f));
        }

        g.setColour (juce::Colour (0xff1a0d05));
        fonts::drawText (g, "SP-92", font, { badge.getX() + 9.0f, badge.getY() + 5.0f, w, 13.0f }, juce::Justification::centredLeft);
    }

    g.setColour (f.stripInk);
    fonts::drawText (g, "POLYPHONIC CHORD STAB SAMPLER", fonts::label (11.0f, 0.26f), { 360.0f, 24.0f, 300.0f, 16.5f }, juce::Justification::centredLeft);
    g.setColour (f.stripInk.withMultipliedAlpha (0.6f));
    fonts::drawText (g, utf8 ("16 STEPS \xc2\xb7 20 DISKS \xc2\xb7 DUB DELAY"), fonts::label (9.0f, 0.3f), { 360.0f, 40.5f, 300.0f, 13.5f },
                     juce::Justification::centredLeft);

    //--------------------------------------------------------------------------
    // Marcos de pantalla, pozos y encabezados
    //--------------------------------------------------------------------------
    for (const auto& bezel : bezels)
    {
        const auto r = bezel.area;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, -1.0f), 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 10.0f);

        juce::ColourGradient frame (juce::Colour (0xff25221f), 0.0f, r.getY(), juce::Colour (0xff151311), 0.0f, r.getBottom(), false);
        g.setGradientFill (frame);
        g.fillRoundedRectangle (r, 10.0f);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip, crescent;
            clip.addRoundedRectangle (r, 10.0f);
            crescent.addRoundedRectangle (r, 10.0f);
            crescent.addRoundedRectangle (r.translated (0.0f, 2.0f), 10.0f);
            crescent.setUsingNonZeroWinding (false);
            g.reduceClipRegion (clip);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillPath (crescent);
        }

        if (bezel.tiny.isNotEmpty())
        {
            g.setColour (juce::Colour (0xff8b8378));
            fonts::drawText (g, bezel.tiny, fonts::label (8.0f, 0.24f), { r.getX() + 16.0f, r.getBottom() - 14.0f, 400.0f, 8.0f },
                             juce::Justification::centredLeft);
        }
    }

    for (const auto& w : wells)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRoundedRectangle (w.translated (0.0f, 2.0f), 12.0f);
        juce::ColourGradient well (f.well.interpolatedWith (juce::Colours::black, 0.15f), 0.0f, w.getY(), f.well, 0.0f, w.getBottom(), false);
        g.setGradientFill (well);
        g.fillRoundedRectangle (w, 12.0f);

        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (w, 12.0f);
        g.reduceClipRegion (clip);
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.45f), 0.0f, w.getY(), juce::Colours::transparentBlack, 0.0f, w.getY() + 14.0f, false);
        g.setGradientFill (inner);
        g.fillRect (w);
    }

    // Costura entre las dos mitades del panel.
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillRect (20.0f, 471.0f, b.getWidth() - 40.0f, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (20.0f, 470.0f, b.getWidth() - 40.0f, 2.0f);

    for (const auto& h : heads)
        SectionHead::draw (g, h.text, h.area, f.silk);

    //--------------------------------------------------------------------------
    // Serigrafia de controles
    //--------------------------------------------------------------------------
    for (const auto& s : silks)
    {
        const auto font = s.small ? fonts::label (9.0f, 0.14f) : fonts::label (11.0f, 0.16f);
        const float h = s.small ? 9.0f : 11.0f;

        g.setColour (s.small ? f.silkMuted : f.silk);
        fonts::drawText (g, s.text, font, s.centred ? juce::Rectangle<float> (s.x - 100.0f, s.y, 200.0f, h) : juce::Rectangle<float> (s.x, s.y, 200.0f, h),
                         s.centred ? juce::Justification::centred : juce::Justification::centredLeft);
    }

    //--------------------------------------------------------------------------
    // Placa de serie y tornillos
    //--------------------------------------------------------------------------
    const juce::Rectangle<float> plate { 936.0f, 612.0f, 136.0f, 74.0f };
    anim::blurredRoundedRect (g, plate.translated (0.0f, 2.0f), 4.0f, juce::Colours::black.withAlpha (0.35f), 3.0f);

    {
        const float ang = 135.0f * juce::MathConstants<float>::pi / 180.0f;
        const juce::Point<float> dir { std::sin (ang), -std::cos (ang) };
        const float len = std::abs (plate.getWidth() * dir.x) + std::abs (plate.getHeight() * dir.y);
        const auto c = plate.getCentre();
        juce::ColourGradient brushed (juce::Colour (0xffe7e5df), c - dir * (len * 0.5f), juce::Colour (0xffd4d1ca), c + dir * (len * 0.5f), false);
        brushed.addColour (0.55, juce::Colour (0xffa9a69f));
        g.setGradientFill (brushed);
        g.fillRoundedRectangle (plate, 4.0f);
    }

    g.setColour (juce::Colours::white);
    g.fillRect (plate.getX() + 3.0f, plate.getY(), plate.getWidth() - 6.0f, 1.0f);

    g.setColour (juce::Colour (0xff3a3832));
    fonts::drawText (g, "NARFBACH", fonts::display (10.0f, 0.1f), { plate.getX() + 12.0f, plate.getY() + 9.0f, 120.0f, 13.0f }, juce::Justification::centredLeft);

    const char* lines[] { "STABR SP-92", "S/N 0092-0417", "BUILT FOR THE CLUB" };
    for (int i = 0; i < 3; ++i)
        fonts::drawText (g, lines[i], fonts::label (8.5f, 0.16f),
                         { plate.getX() + 12.0f, plate.getY() + 24.0f + (float) i * 12.325f, 120.0f, 12.325f }, juce::Justification::centredLeft);

    screw (g, 10.0f, 10.0f, 25.0f);
    screw (g, 1076.0f, 10.0f, -40.0f);
    screw (g, 10.0f, 756.0f, 70.0f);
    screw (g, 1076.0f, 756.0f, 10.0f);
}
}
