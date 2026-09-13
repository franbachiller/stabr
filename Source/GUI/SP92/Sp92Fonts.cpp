#include "Sp92Fonts.h"

#if STABR_HAS_EMBEDDED_FONTS
 #include <BinaryData.h>
#endif

namespace stabr::gui::sp92::fonts
{
#if STABR_HAS_EMBEDDED_FONTS
namespace
{
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    juce::Typeface::Ptr michroma()
    {
        static auto t = load (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
        return t;
    }

    juce::Typeface::Ptr archivoNarrow()
    {
        static auto t = load (BinaryData::ArchivoNarrowSemiBold_ttf, BinaryData::ArchivoNarrowSemiBold_ttfSize);
        return t;
    }

    juce::Typeface::Ptr reenieBeanie()
    {
        static auto t = load (BinaryData::ReenieBeanieRegular_ttf, BinaryData::ReenieBeanieRegular_ttfSize);
        return t;
    }
}

bool usingEmbeddedFonts() noexcept { return true; }

juce::Font display (float h) { return juce::FontOptions (michroma()).withHeight (h); }
juce::Font label (float h, bool) { return juce::FontOptions (archivoNarrow()).withHeight (h); }
juce::Font hand (float h) { return juce::FontOptions (reenieBeanie()).withHeight (h); }

#else

bool usingEmbeddedFonts() noexcept { return false; }

juce::Font display (float h)
{
    return juce::FontOptions ("Bahnschrift", h, juce::Font::bold);
}

juce::Font label (float h, bool bold)
{
    return juce::FontOptions ("Bahnschrift SemiCondensed", h, bold ? juce::Font::bold : juce::Font::plain);
}

juce::Font hand (float h)
{
    // Segoe Print es mas ancha que Reenie Beanie: se compensa con la altura.
    return juce::FontOptions ("Segoe Print", h * 0.62f, juce::Font::plain);
}
#endif
}
