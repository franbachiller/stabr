#include "DotMatrix.h"

#include <algorithm>
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

    /*  Pixeles fisicos que cubre cada punto de un eje, con la fraccion exacta
        de cada pixel que queda dentro del punto. */
    template <typename SpanT>
    void makeSpans (std::vector<SpanT>& spans, int count, float origin, float pitch, float size, float scale, int limit)
    {
        spans.assign ((size_t) count, {});

        for (int i = 0; i < count; ++i)
        {
            const float a = (origin + (float) i * pitch + pitch * (1.0f - size) * 0.5f) * scale;
            const float b = a + pitch * size * scale;
            auto& s = spans[(size_t) i];

            s.first = juce::jlimit (0, limit, (int) std::floor (a));
            const int end = juce::jlimit (s.first, limit, (int) std::ceil (b));

            for (int k = s.first; k < end; ++k)
                s.weights.push_back (juce::jmax (0.0f, juce::jmin (b, (float) k + 1.0f) - juce::jmax (a, (float) k)));
        }
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
DotMatrix::DotMatrix (int dotsWide, int dotsHigh, juce::BorderSize<int> glassInsets)
    : W (dotsWide), H (dotsHigh), insets (glassInsets),
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
    geomScale = 0.0f;   // el brillo depende del acabado: se rehace todo
    repaint();
}

void DotMatrix::clear() noexcept                { std::fill (fb.begin(), fb.end(), (std::uint8_t) 0); }

void DotMatrix::set (int x, int y) noexcept
{
    if (x >= 0 && y >= 0 && x < W && y < H)
        fb[(size_t) (y * W + x)] = 1;
}

void DotMatrix::unset (int x, int y) noexcept
{
    if (x >= 0 && y >= 0 && x < W && y < H)
        fb[(size_t) (y * W + x)] = 0;
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

juce::Rectangle<float> DotMatrix::getGridArea() const
{
    // Puntos cuadrados, grilla centrada en el vidrio.
    const auto inner = insets.subtractedFrom (getLocalBounds()).toFloat();
    const float pitch = juce::jmax (0.0f, juce::jmin (inner.getWidth() / (float) W, inner.getHeight() / (float) H));
    return juce::Rectangle<float> (pitch * (float) W, pitch * (float) H).withCentre (inner.getCentre());
}

juce::Rectangle<float> DotMatrix::getDotRect (int x, int y) const
{
    const auto grid = getGridArea();
    const float p = grid.getWidth() / (float) W;
    const float inset = p * (1.0f - kLitSize) * 0.5f;
    return { grid.getX() + (float) x * p + inset, grid.getY() + (float) y * p + inset, p * kLitSize, p * kLitSize };
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
    int x0 = W, y0 = H, x1 = -1, y1 = -1;

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const auto i = (size_t) (y * W + x);
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
            x0 = juce::jmin (x0, x); y0 = juce::jmin (y0, y);
            x1 = juce::jmax (x1, x); y1 = juce::jmax (y1, y);
        }

    settling = moving;

    if (x1 >= 0)
    {
        dirtyDots = dirtyDots.getUnion ({ x0, y0, x1 - x0 + 1, y1 - y0 + 1 });
        repaint();
    }
}

//==============================================================================
void DotMatrix::rebuildGeometry (float scale)
{
    geomScale = scale;
    geomW = getWidth();
    geomH = getHeight();
    PW = juce::jmax (1, (int) std::ceil ((float) getWidth() * scale));
    PH = juce::jmax (1, (int) std::ceil ((float) getHeight() * scale));

    const auto grid = getGridArea();
    const float p = grid.getWidth() / (float) W;
    const float pitchPx = p * scale;

    // El hueco entre puntos se muestra solo si hay pixeles para dibujarlo. A
    // 2,5 px por punto (1x) un hueco de 0,3 px no se ve como hueco: se ve como
    // una trama clara que cruza las letras. Ahi los puntos encendidos se tocan
    // y los trazos quedan solidos; a 2x o en pantallas retina aparece el hueco.
    const float litGap = juce::jlimit (0.0f, pitchPx * (1.0f - kLitSize), (pitchPx - 3.0f) * 0.15f);
    const float litSize = 1.0f - litGap / pitchPx;
    const float unlitGap = juce::jlimit (0.3f, pitchPx * (1.0f - kUnlitSize), 0.45f + (pitchPx - 2.5f) * 0.12f);
    const float unlitSize = juce::jlimit (0.5f, 1.0f, 1.0f - unlitGap / pitchPx);

    makeSpans (litCols, W, grid.getX(), p, litSize, scale, PW);
    makeSpans (litRows, H, grid.getY(), p, litSize, scale, PH);
    makeSpans (offCols, W, grid.getX(), p, unlitSize, scale, PW);
    makeSpans (offRows, H, grid.getY(), p, unlitSize, scale, PH);

    const auto n = (size_t) (PW * PH);
    lit.assign (n, 0.0f);
    glow.assign (n, 0.0f);
    tmp.assign (n, 0.0f);

    // Fuera de la grilla, el mismo tono medio que la trama de puntos apagados:
    // si no, el area activa se lee como un rectangulo mas oscuro.
    unlit.assign (n, unlitSize * unlitSize);

    const int ax0 = juce::jlimit (0, PW, (int) std::floor (grid.getX() * scale)), ax1 = juce::jlimit (0, PW, (int) std::ceil (grid.getRight() * scale));
    const int ay0 = juce::jlimit (0, PH, (int) std::floor (grid.getY() * scale)), ay1 = juce::jlimit (0, PH, (int) std::ceil (grid.getBottom() * scale));

    for (int py = ay0; py < ay1; ++py)
        std::fill (unlit.begin() + py * PW + ax0, unlit.begin() + py * PW + ax1, 0.0f);

    for (int y = 0; y < H; ++y)
    {
        const auto& rs = offRows[(size_t) y];

        for (int x = 0; x < W; ++x)
        {
            const auto& cs = offCols[(size_t) x];

            for (size_t j = 0; j < rs.weights.size(); ++j)
                for (size_t k = 0; k < cs.weights.size(); ++k)
                    unlit[(size_t) ((rs.first + (int) j) * PW + cs.first + (int) k)] += rs.weights[j] * cs.weights[k];
        }
    }

    // Resplandor: gaussiano de ~0.6 punto. Sombra del LCD verde: mas cerrada y corrida.
    const float sigma = juce::jmax (0.5f, (fin->lcdGlows ? 0.62f : 0.42f) * p * scale);
    blurRadius = juce::jmax (1, (int) std::ceil (sigma * 2.6f));
    kernel.assign ((size_t) (blurRadius * 2 + 1), 0.0f);

    float sum = 0.0f;
    for (int k = -blurRadius; k <= blurRadius; ++k)
        sum += (kernel[(size_t) (k + blurRadius)] = std::exp (-(float) (k * k) / (2.0f * sigma * sigma)));
    for (auto& v : kernel)
        v /= sum;

    shadowDx = fin->lcdGlows ? 0 : juce::roundToInt (0.30f * p * scale);
    shadowDy = fin->lcdGlows ? 0 : juce::roundToInt (0.45f * p * scale);

    layer = juce::Image (juce::Image::ARGB, PW, PH, true, juce::SoftwareImageType());
    fullDirty = true;
}

void DotMatrix::renderRegion (juce::Rectangle<int> dots)
{
    dots = dots.getIntersection ({ 0, 0, W, H });

    if (dots.isEmpty() || litCols.empty())
        return;

    // 1) Cobertura de los puntos encendidos en los pixeles de la region.
    const auto& c0 = litCols[(size_t) dots.getX()];
    const auto& c1 = litCols[(size_t) (dots.getRight() - 1)];
    const auto& r0 = litRows[(size_t) dots.getY()];
    const auto& r1 = litRows[(size_t) (dots.getBottom() - 1)];

    const int rx0 = c0.first, rx1 = juce::jmin (PW, c1.first + (int) c1.weights.size());
    const int ry0 = r0.first, ry1 = juce::jmin (PH, r1.first + (int) r1.weights.size());

    for (int py = ry0; py < ry1; ++py)
        std::fill (lit.begin() + py * PW + rx0, lit.begin() + py * PW + rx1, 0.0f);

    // Un vecino puede compartir el pixel del borde: se suma lo que cae dentro.
    for (int y = juce::jmax (0, dots.getY() - 1); y < juce::jmin (H, dots.getBottom() + 1); ++y)
    {
        const auto& rs = litRows[(size_t) y];

        for (int x = juce::jmax (0, dots.getX() - 1); x < juce::jmin (W, dots.getRight() + 1); ++x)
        {
            const float l = level[(size_t) (y * W + x)];

            if (l <= 0.0f)
                continue;

            const auto& cs = litCols[(size_t) x];

            for (size_t j = 0; j < rs.weights.size(); ++j)
            {
                const int py = rs.first + (int) j;

                if (py < ry0 || py >= ry1)
                    continue;

                const float wy = rs.weights[j] * l;

                for (size_t k = 0; k < cs.weights.size(); ++k)
                {
                    const int px = cs.first + (int) k;

                    if (px >= rx0 && px < rx1)
                        lit[(size_t) (py * PW + px)] += wy * cs.weights[k];
                }
            }
        }
    }

    // 2) Brillo: desenfoque separable sobre la region mas el radio.
    const int r = blurRadius;
    const bool whole = dots == juce::Rectangle<int> (0, 0, W, H);   // incluye los margenes fuera de la grilla
    const int gx0 = whole ? 0  : juce::jmax (0, rx0 - r - std::abs (shadowDx));
    const int gx1 = whole ? PW : juce::jmin (PW, rx1 + r + std::abs (shadowDx));
    const int gy0 = whole ? 0  : juce::jmax (0, ry0 - r - std::abs (shadowDy));
    const int gy1 = whole ? PH : juce::jmin (PH, ry1 + r + std::abs (shadowDy));
    const int hy0 = juce::jmax (0, gy0 - r), hy1 = juce::jmin (PH, gy1 + r);

    for (int py = hy0; py < hy1; ++py)
    {
        const float* src = lit.data() + py * PW;
        float* dst = tmp.data() + py * PW;

        for (int px = gx0; px < gx1; ++px)
        {
            float s = 0.0f;
            const int k0 = juce::jmax (-r, -px), k1 = juce::jmin (r, PW - 1 - px);

            for (int k = k0; k <= k1; ++k)
                s += kernel[(size_t) (k + r)] * src[px + k];

            dst[px] = s;
        }
    }

    for (int py = gy0; py < gy1; ++py)
    {
        const int k0 = juce::jmax (-r, hy0 - py), k1 = juce::jmin (r, hy1 - 1 - py);

        for (int px = gx0; px < gx1; ++px)
        {
            float s = 0.0f;

            for (int k = k0; k <= k1; ++k)
                s += kernel[(size_t) (k + r)] * tmp[(size_t) ((py + k) * PW + px)];

            glow[(size_t) (py * PW + px)] = s;
        }
    }

    // 3) Composicion: puntos apagados, brillo o sombra, y puntos encendidos.
    struct RGBA { float r, g, b, a; };
    auto rgba = [] (juce::Colour c) { return RGBA { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), c.getFloatAlpha() }; };

    const auto off = rgba (fin->lcdOff), on = rgba (fin->lcdOn), halo = rgba (fin->lcdGlow);
    const bool emissive = fin->lcdGlows;

    juce::Image::BitmapData bd (layer, juce::Image::BitmapData::readWrite);

    for (int py = gy0; py < gy1; ++py)
    {
        for (int px = gx0; px < gx1; ++px)
        {
            const auto i = (size_t) (py * PW + px);
            float cr = 0.0f, cg = 0.0f, cb = 0.0f, ca = 0.0f;   // premultiplicado

            auto over = [&] (const RGBA& c, float alpha)
            {
                alpha = juce::jlimit (0.0f, 1.0f, alpha);
                cr = c.r * alpha + cr * (1.0f - alpha);
                cg = c.g * alpha + cg * (1.0f - alpha);
                cb = c.b * alpha + cb * (1.0f - alpha);
                ca = alpha + ca * (1.0f - alpha);
            };

            over (off, off.a * unlit[i]);

            if (emissive)
            {
                over (halo, halo.a * 0.42f * juce::jmin (1.0f, glow[i] * 1.5f));
            }
            else
            {
                const int sx = px - shadowDx, sy = py - shadowDy;
                const float s = (sx >= 0 && sy >= 0) ? glow[(size_t) (sy * PW + sx)] : 0.0f;
                over (on, 0.16f * juce::jmin (1.0f, s * 1.4f));
            }

            over (on, on.a * lit[i]);

            auto* pixel = (juce::PixelARGB*) bd.getPixelPointer (px, py);
            pixel->setARGB ((juce::uint8) juce::roundToInt (ca * 255.0f), (juce::uint8) juce::roundToInt (cr * 255.0f),
                            (juce::uint8) juce::roundToInt (cg * 255.0f), (juce::uint8) juce::roundToInt (cb * 255.0f));
        }
    }
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

    const float scale = juce::jlimit (0.5f, 8.0f, g.getInternalContext().getPhysicalPixelScaleFactor());

    if (std::abs (scale - geomScale) > 0.001f || getWidth() != geomW || getHeight() != geomH)
        rebuildGeometry (scale);

    if (fullDirty)
    {
        renderRegion ({ 0, 0, W, H });
        fullDirty = false;
        dirtyDots = {};
    }
    else if (! dirtyDots.isEmpty())
    {
        renderRegion (dirtyDots);
        dirtyDots = {};
    }

    // La capa de puntos esta hecha pixel a pixel: se dibuja 1:1, dentro del vidrio.
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path glassShape;
        glassShape.addRoundedRectangle (b, 4.0f);
        g.reduceClipRegion (glassShape);
        g.setOpacity (1.0f);
        g.drawImageTransformed (layer, juce::AffineTransform::scale (1.0f / scale));
    }

    // Reflejo del vidrio, suave: un corte neto se leia como una raya sobre las letras.
    {
        const float ang = 165.0f * juce::MathConstants<float>::pi / 180.0f;
        const juce::Point<float> dir { std::sin (ang), -std::cos (ang) };
        const float len = std::abs (b.getWidth() * dir.x) + std::abs (b.getHeight() * dir.y);
        const auto c = b.getCentre();
        juce::ColourGradient reflection (juce::Colours::white.withAlpha (0.13f), c - dir * (len * 0.5f),
                                         juce::Colours::white.withAlpha (0.0f), c + dir * (len * 0.5f), false);
        reflection.addColour (0.30, juce::Colours::white.withAlpha (0.05f));
        reflection.addColour (0.55, juce::Colours::white.withAlpha (0.0f));
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
