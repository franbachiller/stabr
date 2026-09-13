#include "DotMatrix.h"

#include <array>
#include <cmath>
#include <cstring>

namespace stabr::gui::sp92
{
namespace
{
    struct GlyphTable
    {
        std::array<std::array<std::uint8_t, 7>, 128> rows {};
        std::array<bool, 128> present {};

        void add (char c, std::array<std::uint8_t, 7> r)
        {
            rows[(size_t) (unsigned char) c] = r;
            present[(size_t) (unsigned char) c] = true;
        }

        GlyphTable()
        {
            add ('A', {14,17,17,31,17,17,17}); add ('B', {30,17,17,30,17,17,30}); add ('C', {14,17,16,16,16,17,14});
            add ('D', {30,17,17,17,17,17,30}); add ('E', {31,16,16,30,16,16,31}); add ('F', {31,16,16,30,16,16,16});
            add ('G', {14,17,16,23,17,17,15}); add ('H', {17,17,17,31,17,17,17}); add ('I', {14,4,4,4,4,4,14});
            add ('J', {7,2,2,2,2,18,12});      add ('K', {17,18,20,24,20,18,17}); add ('L', {16,16,16,16,16,16,31});
            add ('M', {17,27,21,21,17,17,17}); add ('N', {17,17,25,21,19,17,17}); add ('O', {14,17,17,17,17,17,14});
            add ('P', {30,17,17,30,16,16,16}); add ('Q', {14,17,17,17,21,18,13}); add ('R', {30,17,17,30,20,18,17});
            add ('S', {15,16,16,14,1,1,30});   add ('T', {31,4,4,4,4,4,4});       add ('U', {17,17,17,17,17,17,14});
            add ('V', {17,17,17,17,17,10,4});  add ('W', {17,17,17,21,21,21,10}); add ('X', {17,17,10,4,10,17,17});
            add ('Y', {17,17,17,10,4,4,4});    add ('Z', {31,1,2,4,8,16,31});

            add ('0', {14,17,19,21,25,17,14}); add ('1', {4,12,4,4,4,4,14});      add ('2', {14,17,1,2,4,8,31});
            add ('3', {31,2,4,2,1,17,14});     add ('4', {2,6,10,18,31,2,2});     add ('5', {31,16,30,1,1,17,14});
            add ('6', {6,8,16,30,17,17,14});   add ('7', {31,1,2,4,8,8,8});       add ('8', {14,17,17,14,17,17,14});
            add ('9', {14,17,17,15,1,2,12});

            // Minusculas que usan los nombres de acordes (Am7, Cmaj7, Dsus4, Fadd9, Bdim7, Em(no5)).
            add ('a', {0,0,14,1,15,17,15});    add ('b', {16,16,22,25,17,17,30}); add ('d', {1,1,13,19,17,17,15});
            add ('e', {0,0,14,17,31,16,14});   add ('i', {4,0,12,4,4,4,14});      add ('j', {2,0,6,2,2,18,12});
            add ('m', {0,0,26,21,21,17,17});   add ('n', {0,0,22,25,17,17,17});   add ('o', {0,0,14,17,17,17,14});
            add ('s', {0,0,14,16,14,1,30});    add ('u', {0,0,17,17,17,19,13});

            add (' ', {0,0,0,0,0,0,0});        add ('.', {0,0,0,0,0,12,12});      add ('-', {0,0,0,31,0,0,0});
            add ('/', {0,1,2,4,8,16,0});       add ('#', {10,10,31,10,31,10,10}); add (':', {0,12,12,0,12,12,0});
            add ('%', {24,25,2,4,8,19,3});     add ('>', {8,4,2,1,2,4,8});        add ('<', {2,4,8,16,8,4,2});
            add ('+', {0,4,4,31,4,4,0});       add ('(', {2,4,8,8,8,4,2});        add (')', {8,4,2,2,2,4,8});
            add ('\'', {4,4,8,0,0,0,0});       add ('*', {0,21,14,31,14,21,0});   add ('=', {0,0,31,0,31,0,0});
        }
    };

    const GlyphTable& glyphs()
    {
        static const GlyphTable table;
        return table;
    }
}

const std::uint8_t* dotGlyph (juce::juce_wchar c) noexcept
{
    const auto& t = glyphs();

    if (c < 0 || c >= 128)
        return nullptr;

    if (t.present[(size_t) c])
        return t.rows[(size_t) c].data();

    // Una minuscula que no tiene glifo propio se muestra en mayuscula.
    if (c >= 'a' && c <= 'z' && t.present[(size_t) (c - 32)])
        return t.rows[(size_t) (c - 32)].data();

    return nullptr;
}

//==============================================================================
DotMatrix::DotMatrix (int dotsWide, int dotsHigh)
    : W (dotsWide), H (dotsHigh),
      fb ((size_t) (dotsWide * dotsHigh), 0), last ((size_t) (dotsWide * dotsHigh), 2),
      level ((size_t) (dotsWide * dotsHigh), 0.0f)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
    fin = &finish (0);
}

void DotMatrix::setFinish (const Finish& f)
{
    fin = &f;
    unlitCache = {};
    repaint();
}

void DotMatrix::clear() noexcept                { std::fill (fb.begin(), fb.end(), (std::uint8_t) 0); }

void DotMatrix::set (int x, int y) noexcept
{
    if (x >= 0 && y >= 0 && x < W && y < H)
        fb[(size_t) (y * W + x)] = 1;
}

bool DotMatrix::isLit (int x, int y) const noexcept
{
    return x >= 0 && y >= 0 && x < W && y < H && fb[(size_t) (y * W + x)] != 0;
}

float DotMatrix::getLevel (int x, int y) const noexcept
{
    return x >= 0 && y >= 0 && x < W && y < H ? level[(size_t) (y * W + x)] : 0.0f;
}

void DotMatrix::block (int x, int y, int size) noexcept
{
    for (int i = 0; i < size; ++i)
        for (int j = 0; j < size; ++j)
            set (x + i, y + j);
}

void DotMatrix::hline (int x0, int x1, int y, bool dotted) noexcept
{
    for (int x = x0; x <= x1; ++x)
        if (! dotted || x % 2 == 0)
            set (x, y);
}

void DotMatrix::vline (int x, int y0, int y1, bool dotted) noexcept
{
    for (int y = y0; y <= y1; ++y)
        if (! dotted || y % 2 == 0)
            set (x, y);
}

int DotMatrix::text (const juce::String& s, int x, int y, int scale) noexcept
{
    for (auto p = s.getCharPointer(); ! p.isEmpty(); ++p)
    {
        if (const auto* g = dotGlyph (*p))
            for (int r = 0; r < 7; ++r)
                for (int k = 0; k < 5; ++k)
                    if (g[r] & (16 >> k))
                        block (x + k * scale, y + r * scale, scale);

        x += 6 * scale;
    }

    return x;
}

int DotMatrix::textWidth (const juce::String& s, int scale) const noexcept
{
    return juce::jmax (0, s.length() * 6 * scale - scale);
}

void DotMatrix::textRight (const juce::String& s, int xRight, int y, int scale) noexcept
{
    text (s, xRight - textWidth (s, scale), y, scale);
}

void DotMatrix::textCenter (const juce::String& s, int xCentre, int y, int scale) noexcept
{
    text (s, xCentre - textWidth (s, scale) / 2, y, scale);
}

void DotMatrix::commit (float dt)
{
    const bool changed = std::memcmp (fb.data(), last.data(), fb.size()) != 0;

    if (! changed && ! settling)
        return;

    if (changed)
        last = fb;

    const float kOn  = 1.0f - std::exp (-juce::jmax (0.0f, dt) / kOnTau);
    const float kOff = 1.0f - std::exp (-juce::jmax (0.0f, dt) / kOffTau);
    bool moving = false;

    for (size_t i = 0; i < fb.size(); ++i)
    {
        const float target = fb[i] != 0 ? 1.0f : 0.0f;
        float l = level[i];

        if (l == target)
            continue;

        l += (target - l) * (target > l ? kOn : kOff);

        if (std::abs (l - target) < 0.04f)
            l = target;
        else
            moving = true;

        level[i] = l;
    }

    settling = moving;
    repaint();
}

//==============================================================================
void DotMatrix::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // Vidrio retroiluminado: radial-gradient(120% 140% at 50% 30%, bg1, bg2).
    {
        const juce::Point<float> c { b.getCentreX(), b.getY() + b.getHeight() * 0.3f };
        juce::ColourGradient glass (fin->lcdBg1, c.x, c.y, fin->lcdBg2, c.x + b.getWidth() * 1.2f, c.y, true);
        juce::FillType fill (glass);
        fill.transform = juce::AffineTransform::scale (1.0f, (1.4f * b.getHeight()) / (1.2f * b.getWidth()), c.x, c.y);
        g.setFillType (fill);
        g.fillRoundedRectangle (b, 4.0f);
    }

    const auto area = b.reduced (10.0f, 8.0f);
    const float px = area.getWidth() / (float) W;
    const float py = area.getHeight() / (float) H;

    // Puntos apagados: una imagen cacheada a la resolucion fisica real.
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();

    if (! unlitCache.isValid() || unlitColour != fin->lcdOff || std::abs (unlitScale - scale) > 0.01f)
    {
        unlitColour = fin->lcdOff;
        unlitScale = scale;
        unlitCache = juce::Image (juce::Image::ARGB,
                                  juce::jmax (1, juce::roundToInt (area.getWidth() * scale)),
                                  juce::jmax (1, juce::roundToInt (area.getHeight() * scale)), true);

        juce::Graphics ig (unlitCache);
        juce::RectangleList<float> dots;
        const float sx = px * scale, sy = py * scale;

        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                dots.addWithoutMerging ({ x * sx + sx * 0.09f, y * sy + sy * 0.09f, sx * 0.82f, sy * 0.82f });

        ig.setColour (unlitColour);
        ig.fillRectList (dots);
    }

    g.drawImage (unlitCache, area);

    // Puntos encendidos en seis niveles de brillo (la inercia del cristal).
    constexpr int kLevels = 6;
    std::array<juce::RectangleList<float>, kLevels + 1> lit, halo, haze;

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const int q = juce::roundToInt (level[(size_t) (y * W + x)] * (float) kLevels);

            if (q <= 0)
                continue;

            const juce::Rectangle<float> dot { area.getX() + x * px + px * 0.07f, area.getY() + y * py + py * 0.07f, px * 0.86f, py * 0.86f };
            lit[(size_t) q].addWithoutMerging (dot);

            if (fin->lcdGlows)
            {
                halo[(size_t) q].addWithoutMerging (dot.expanded (px * 0.40f, py * 0.40f));
                haze[(size_t) q].addWithoutMerging (dot.expanded (px * 0.95f, py * 0.95f));
            }
        }

    for (int q = 1; q <= kLevels; ++q)
    {
        const float a = (float) q / (float) kLevels;

        if (fin->lcdGlows)
        {
            g.setColour (fin->lcdGlow.withMultipliedAlpha (0.08f * a));
            g.fillRectList (haze[(size_t) q]);
            g.setColour (fin->lcdGlow.withMultipliedAlpha (0.20f * a));
            g.fillRectList (halo[(size_t) q]);
        }

        g.setColour (fin->lcdOn.withMultipliedAlpha (a));
        g.fillRectList (lit[(size_t) q]);
    }

    // Reflejo del vidrio: linear-gradient(165deg, .18 0%, .04 34%, 0 35%), con
    // el corte neto de un vidrio real.
    {
        const float ang = 165.0f * juce::MathConstants<float>::pi / 180.0f;
        const juce::Point<float> dir { std::sin (ang), -std::cos (ang) };
        const float len = std::abs (b.getWidth() * dir.x) + std::abs (b.getHeight() * dir.y);
        const auto c = b.getCentre();
        juce::ColourGradient reflection (juce::Colours::white.withAlpha (0.18f), c - dir * (len * 0.5f),
                                         juce::Colours::white.withAlpha (0.0f), c + dir * (len * 0.5f), false);
        reflection.addColour (0.34, juce::Colours::white.withAlpha (0.04f));
        reflection.addColour (0.35, juce::Colours::white.withAlpha (0.0f));
        g.setGradientFill (reflection);
        g.fillRoundedRectangle (b, 4.0f);
    }

    // inset 0 3px 10px .55 e inset 0 0 0 1px .5.
    juce::ColourGradient inner (juce::Colours::black.withAlpha (0.55f), 0.0f, b.getY(),
                                juce::Colours::black.withAlpha (0.0f), 0.0f, b.getY() + 11.0f, false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (b, 4.0f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
}
}
