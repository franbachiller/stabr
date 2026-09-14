#include "LcdDrawing.h"
#include "../../Chord/ChordNaming.h"

#include <algorithm>
#include <cmath>

namespace stabr::gui::sp92::lcdraw
{
namespace
{
    constexpr int whiteIndex[12] { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };

    int pitchClass (int note) noexcept { return ((note % 12) + 12) % 12; }

    void fill (DotMatrix& lcd, int x0, int y0, int x1, int y1)   // inclusivo
    {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                lcd.set (x, y);
    }
}

//==============================================================================
std::array<int, 6> layoutCells (const std::array<int, 6>& widths, int screenWidth)
{
    std::array<int, 6> left {};

    // De 5 puntos de aire para abajo: el primero que entra. Seis textos de 5
    // caracteres (29 puntos) con 3 de aire suman 189: siempre entra con 3.
    for (int gap = 5; gap >= 1; --gap)
    {
        for (int i = 0; i < 6; ++i)
            left[(size_t) i] = juce::roundToInt (((float) i + 0.5f) * (float) screenWidth / 6.0f - (float) widths[(size_t) i] * 0.5f);

        left[0] = juce::jmax (0, left[0]);

        for (int i = 1; i < 6; ++i)
            left[(size_t) i] = juce::jmax (left[(size_t) i], left[(size_t) i - 1] + widths[(size_t) i - 1] + gap);

        if (left[5] + widths[5] > screenWidth)
        {
            left[5] = screenWidth - widths[5];

            for (int i = 4; i >= 0; --i)
                left[(size_t) i] = juce::jmin (left[(size_t) i], left[(size_t) i + 1] - gap - widths[(size_t) i]);
        }

        if (left[0] >= 0)
            return left;
    }

    left[0] = juce::jmax (0, left[0]);
    return left;
}

//==============================================================================
bool isBlackKey (int midiNote) noexcept
{
    return whiteIndex[pitchClass (midiNote)] < 0;
}

int KeyboardLayout::blackKeyCentre (int midiNote) const
{
    const int rel = midiNote - lowC;
    const int below = whiteIndex[pitchClass (midiNote) - 1];   // la blanca de la izquierda
    return x + ((rel / 12) * 7 + below + 1) * keyWidth;
}

juce::Rectangle<int> KeyboardLayout::keyInterior (int midiNote) const
{
    if (! contains (midiNote))
        return {};

    const int rel = midiNote - lowC;

    if (! isBlackKey (midiNote))
    {
        const int wi = (rel / 12) * 7 + whiteIndex[pitchClass (midiNote)];
        return { x + wi * keyWidth + 1, y + 1, keyWidth - 1, whiteHeight - 1 };
    }

    const int half = juce::jmax (1, blackWidth / 2);
    return { blackKeyCentre (midiNote) - half, y + 1, half * 2 + 1, blackHeight };
}

KeyboardLayout keyboardFor (const chord::ChordResult& notes, int x0, int x1, int y0)
{
    int lo = 60, hi = 60;

    if (notes.count > 0)
    {
        lo = hi = notes[0];
        for (int i = 1; i < notes.count; ++i)
        {
            lo = juce::jmin (lo, notes[i]);
            hi = juce::jmax (hi, notes[i]);
        }
    }

    KeyboardLayout k;
    k.lowC = lo - pitchClass (lo);
    k.octaves = juce::jlimit (2, 4, (hi - k.lowC) / 12 + 1);

    const int area = x1 - x0 + 1;
    k.keyWidth = juce::jlimit (3, 7, (area - 1) / (k.octaves * 7));
    k.blackWidth = k.keyWidth >= 5 ? 3 : 1;
    k.x = x0 + (area - k.width()) / 2;
    k.y = y0;
    return k;
}

void drawKeyboard (DotMatrix& lcd, const chord::ChordResult& notes, int x0, int x1, int y0, bool hit)
{
    const auto k = keyboardFor (notes, x0, x1, y0);
    const int right = k.x + k.width() - 1;
    const int bottom = k.y + KeyboardLayout::whiteHeight;
    const int whites = k.octaves * 7;

    std::array<bool, 128> pressed {};
    for (int i = 0; i < notes.count; ++i)
        if (notes[i] >= 0 && notes[i] < 128 && k.contains (notes[i]))
            pressed[(size_t) notes[i]] = true;

    // Marco y division de las teclas blancas.
    lcd.hline (k.x, right, k.y);
    lcd.hline (k.x, right, bottom);
    for (int i = 0; i <= whites; ++i)
        lcd.vline (k.x + i * k.keyWidth, k.y, bottom);

    // Blancas que suenan: llenas.
    for (int rel = 0; rel < k.octaves * 12; ++rel)
    {
        const int note = k.lowC + rel;

        if (isBlackKey (note) || ! pressed[(size_t) note])
            continue;

        const auto r = k.keyInterior (note);
        fill (lcd, r.getX(), r.getY(), r.getRight() - 1, r.getBottom() - 1);
    }

    // Negras: se recortan sobre las blancas con un punto de aire; llenas si
    // suenan, contorno si no.
    const int half = k.blackWidth / 2;

    for (int rel = 0; rel < k.octaves * 12; ++rel)
    {
        const int note = k.lowC + rel;

        if (! isBlackKey (note))
            continue;

        const int bx = k.blackKeyCentre (note);
        const int keyBottom = k.y + KeyboardLayout::blackHeight;

        for (int yy = k.y + 1; yy <= keyBottom + 1; ++yy)
            for (int xx = bx - half - 1; xx <= bx + half + 1; ++xx)
                lcd.unset (xx, yy);

        if (pressed[(size_t) note])
        {
            const int w = juce::jmax (1, half);
            fill (lcd, bx - w, k.y + 1, bx + w, keyBottom);
        }
        else if (half >= 1)
        {
            lcd.vline (bx - half, k.y, keyBottom);
            lcd.vline (bx + half, k.y, keyBottom);
            lcd.hline (bx - half, bx + half, keyBottom);
        }
        else
        {
            lcd.vline (bx, k.y, keyBottom);
        }
    }

    // El golpe: una raya bajo cada tecla que suena.
    if (hit)
        for (int rel = 0; rel < k.octaves * 12; ++rel)
            if (pressed[(size_t) (k.lowC + rel)])
            {
                const auto r = k.keyInterior (k.lowC + rel);
                lcd.hline (r.getX(), r.getRight() - 1, bottom + 2);
            }

    // Las notas, del bajo al agudo.
    std::array<int, 8> sorted {};
    const int count = juce::jmin (notes.count, (int) sorted.size());
    for (int i = 0; i < count; ++i)
        sorted[(size_t) i] = notes[i];
    std::sort (sorted.begin(), sorted.begin() + count);

    juce::StringArray names;
    for (int i = 0; i < count; ++i)
        names.add (chord::keyNames[(size_t) pitchClass (sorted[(size_t) i])]);

    auto text = names.joinIntoString (" ");
    if (lcd.textWidth (text) > x1 - x0 + 1)
        text = names.joinIntoString ("");

    lcd.textRight (text, x1, bottom + 4);
}

//==============================================================================
int EchoScreen::xOf (float seconds, float window) noexcept
{
    const int span = width - 1 - laneStart - 2;
    return laneStart + 1 + juce::roundToInt (juce::jlimit (0.0f, 1.0f, seconds / juce::jmax (0.001f, window)) * (float) span);
}

void drawEcho (DotMatrix& lcd, const DelayViewInfo& info, float elapsed, float bpm)
{
    constexpr int lanes[2] { EchoScreen::laneL, EchoScreen::laneR };
    constexpr int bar = EchoScreen::maxBar;

    if (! info.enabled)
    {
        lcd.textCenter ("DELAY OFF", EchoScreen::width / 2, 8);
        return;
    }

    std::array<Echo, kMaxEchoes> echoes {};
    const int count = computeEchoes (info, echoes);
    const float window = echoWindowSeconds (echoes, count);

    // Carriles.
    lcd.text ("L", 0, lanes[0] - 3);
    lcd.text ("R", 0, lanes[1] - 3);

    for (int y : lanes)
        for (int x = EchoScreen::laneStart; x < EchoScreen::width; x += 3)
            lcd.set (x, y);

    // Regla: un punto por tiempo, tres por compas.
    const float beat = 60.0f / juce::jlimit (20.0f, 400.0f, bpm);
    const int beatsInWindow = (int) (window / beat);
    const int step = beatsInWindow > 24 ? 4 : 1;

    for (int k = 0; (float) k * beat <= window + 1.0e-4f; k += step)
    {
        const int x = EchoScreen::xOf ((float) k * beat, window);
        lcd.set (x, EchoScreen::rulerY);

        if (k % 4 == 0)
        {
            lcd.set (x - 1, EchoScreen::rulerY);
            lcd.set (x + 1, EchoScreen::rulerY);
        }
    }

    // El golpe original, en los dos canales.
    const int x0 = EchoScreen::xOf (0.0f, window);
    const bool hitNow = elapsed >= 0.0f && elapsed < 0.15f;

    for (int y : lanes)
    {
        lcd.vline (x0, y - bar, y + bar);
        if (hitNow)
            lcd.vline (x0 + 1, y - bar, y + bar);
    }

    // Repeticiones: altura = volumen; las que suenan ahora, dobles.
    for (int i = 0; i < count; ++i)
    {
        const auto& ec = echoes[(size_t) i];
        const int x = EchoScreen::xOf (ec.time, window);
        const int y = lanes[juce::jlimit (0, 1, ec.channel)];
        const int h = juce::jlimit (1, bar, juce::roundToInt (ec.gain * (float) bar));
        const bool lit = info.freeze || (elapsed >= ec.time && elapsed < ec.time + 0.25f);

        lcd.vline (x, y - h, y + h);
        if (lit)
            lcd.vline (x + 1, y - h, y + h);
    }

    // Cursor de reproduccion.
    if (elapsed >= 0.0f && elapsed < window)
    {
        const int x = EchoScreen::xOf (elapsed, window);
        for (int y = 1; y < 22; y += 2)
            lcd.set (x, y);
    }
}
}
