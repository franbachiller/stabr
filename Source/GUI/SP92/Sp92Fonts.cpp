#include "Sp92Fonts.h"
#include "Sp92Assets.h"

#if STABR_HAS_EMBEDDED_FONTS
 #include <BinaryData.h>
#endif

namespace stabr::gui::sp92::fonts
{
namespace
{
    juce::Font withTracking (juce::Font f, float em)
    {
        // El kerning de JUCE es proporcion de la altura; el del CSS, del em.
        return em == 0.0f ? f : f.withExtraKerningFactor (em * f.getHeightToPointsFactor());
    }

   #if STABR_HAS_EMBEDDED_FONTS
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    // En Sp92Assets y no en estaticos: ver D-053.
    juce::Typeface::Ptr cached (juce::Typeface::Ptr& slot, const char* data, int size)
    {
        if (slot == nullptr)
            slot = load (data, size);

        return slot;
    }

    juce::Typeface::Ptr michroma()      { return cached (Sp92Assets::get().michroma, BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize); }
    juce::Typeface::Ptr archivo600()    { return cached (Sp92Assets::get().archivo600, BinaryData::ArchivoNarrowSemiBold_ttf, BinaryData::ArchivoNarrowSemiBold_ttfSize); }
    juce::Typeface::Ptr archivo700()    { return cached (Sp92Assets::get().archivo700, BinaryData::ArchivoNarrowBold_ttf, BinaryData::ArchivoNarrowBold_ttfSize); }
    juce::Typeface::Ptr reenieBeanie()  { return cached (Sp92Assets::get().reenieBeanie, BinaryData::ReenieBeanieRegular_ttf, BinaryData::ReenieBeanieRegular_ttfSize); }

    juce::Font make (juce::Typeface::Ptr tf, float px, float em)
    {
        return withTracking (juce::Font (juce::FontOptions (tf).withPointHeight (px)), em);
    }
   #else
    juce::Font make (const char* name, int style, float px, float em)
    {
        return withTracking (juce::Font (juce::FontOptions (name, px, style).withPointHeight (px)), em);
    }
   #endif
}

#if STABR_HAS_EMBEDDED_FONTS
bool usingEmbeddedFonts() noexcept { return true; }

juce::Font display   (float px, float em) { return make (michroma(), px, em); }
juce::Font label     (float px, float em) { return make (archivo600(), px, em); }
juce::Font labelBold (float px, float em) { return make (archivo700(), px, em); }
juce::Font hand      (float px)           { return make (reenieBeanie(), px, 0.0f); }
#else
bool usingEmbeddedFonts() noexcept { return false; }

juce::Font display   (float px, float em) { return make ("Bahnschrift", juce::Font::bold, px * 0.9f, em); }
juce::Font label     (float px, float em) { return make ("Bahnschrift SemiCondensed", juce::Font::plain, px, em); }
juce::Font labelBold (float px, float em) { return make ("Bahnschrift SemiCondensed", juce::Font::bold, px, em); }
juce::Font hand      (float px)           { return make ("Segoe Print", juce::Font::plain, px * 0.62f, 0.0f); }
#endif

float textWidth (const juce::Font& f, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (f, s);
}

void drawText (juce::Graphics& g, const juce::String& s, const juce::Font& f,
               juce::Rectangle<float> box, juce::Justification j)
{
    const float w = textWidth (f, s);
    float x = box.getX();

    if (j.testFlags (juce::Justification::horizontallyCentred))
        x = box.getCentreX() - w * 0.5f;
    else if (j.testFlags (juce::Justification::right))
        x = box.getRight() - w;

    g.setFont (f);
    g.drawText (s, juce::Rectangle<float> (x, box.getY(), w + 40.0f, box.getHeight()), juce::Justification::centredLeft, false);
}
}
