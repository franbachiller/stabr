#include "ChordVisualizer.h"
#include "Palette.h"
#include "../Chord/ChordNaming.h"

#include <cmath>

namespace stabr::gui
{
ChordVisualizer::ChordVisualizer()
{
    setTitle ("Chord display");
    setInterceptsMouseClicks (false, false);
}

juce::String ChordVisualizer::noteName (int midiNote)
{
    const int n = juce::jlimit (0, 127, midiNote);
    return juce::String (chord::keyNames[(size_t) (n % 12)]) + juce::String (n / 12 - 1);
}

juce::String ChordVisualizer::nameFor (const chord::ChordResult& notes)
{
    if (notes.count <= 0)
        return {};

    const auto name = chord::nameChord (notes);

    if (name.valid)
    {
        juce::String s = juce::String (chord::keyNames[(size_t) name.root]) + juce::String (name.quality);

        if (name.bass != name.root)
            s << "/" << chord::keyNames[(size_t) name.bass];

        return s;
    }

    // Sin nombre conocido (una nota sola, un cluster): las notas, que es honesto.
    juce::String s;

    for (int i = 0; i < juce::jmin (notes.count, 4); ++i)
        s << (i > 0 ? " " : "") << chord::keyNames[(size_t) (((notes[i] % 12) + 12) % 12)];

    return s;
}

void ChordVisualizer::showNotes (const chord::ChordResult& notes, bool isOnset, float vel, double t)
{
    if (notes.count > 0)
    {
        shown = notes;
        sounding = true;

        if (isOnset)
        {
            onsetTime = t;
            velocity = juce::jlimit (0.0f, 1.0f, vel);
        }
    }
    else if (sounding)
    {
        // Se solto: se sigue mostrando lo ultimo mientras se desvanece.
        sounding = false;
        releaseTime = t;
    }

    setDescription (nameFor (shown));
    dirty = true;
}

void ChordVisualizer::setDetail (const juce::String& d)
{
    if (d != detail)
    {
        detail = d;
        dirty = true;
    }
}

void ChordVisualizer::tick (double t)
{
    now = t;

    const bool animating = (t - onsetTime) < 0.5 || (! sounding && (t - releaseTime) < 1.0);

    if (animating || dirty || wasAnimating)
        repaint();

    wasAnimating = animating;
    dirty = false;
}

void ChordVisualizer::paint (juce::Graphics& g)
{
    using namespace colours;

    auto b = getLocalBounds().toFloat();

    //--------------------------------------------------------------------------
    // Nombre + detalle del voicing
    //--------------------------------------------------------------------------
    auto header = b.removeFromTop (38.0f);
    const float life = sounding ? 1.0f : (float) std::exp (-(now - releaseTime) / 0.35);

    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.setColour (shown.count > 0 ? text.withAlpha (0.45f + 0.55f * life) : textMuted);
    g.drawText (shown.count > 0 ? nameFor (shown) : juce::String ("--"),
                header.removeFromLeft (header.getWidth() * 0.5f), juce::Justification::centredLeft, true);

    g.setFont (fonts::label (12.0f));
    g.setColour (textMuted);
    g.drawText (detail, header, juce::Justification::centredRight, true);

    //--------------------------------------------------------------------------
    // Carril de alturas
    //--------------------------------------------------------------------------
    const auto lane = b.withTrimmedTop (4.0f);
    g.setColour (background);
    g.fillRoundedRectangle (lane, 4.0f);

    int lo = 48, hi = 72;

    if (shown.count > 0)
    {
        int mn = shown[0], mx = shown[0];

        for (int i = 1; i < shown.count; ++i)
        {
            mn = juce::jmin (mn, shown[i]);
            mx = juce::jmax (mx, shown[i]);
        }

        // Rango minimo de dos octavas centrado en el acorde: un power chord no
        // tiene que verse gigante ni un acorde abierto salirse del carril.
        const int span = juce::jmax (24, mx - mn + 10);
        lo = (mn + mx) / 2 - span / 2;
        hi = lo + span;
    }

    const float top = lane.getY() + 10.0f;
    const float usable = lane.getHeight() - 20.0f;

    auto yOf = [&] (int note)
    {
        return top + usable - (float) (note - lo) / (float) (hi - lo) * usable;
    };

    g.setFont (fonts::value (11.0f));

    for (int n = ((lo + 11) / 12) * 12; n <= hi; n += 12)
    {
        const float y = yOf (n);
        g.setColour (line);
        g.drawHorizontalLine (juce::roundToInt (y), lane.getX() + 34.0f, lane.getRight() - 6.0f);
        g.setColour (textMuted);
        g.drawText (noteName (n), juce::Rectangle<float> (lane.getX() + 4.0f, y - 7.0f, 30.0f, 14.0f),
                    juce::Justification::centredLeft, false);
    }

    if (shown.count <= 0 || life < 0.01f)
        return;

    //--------------------------------------------------------------------------
    // Notas
    //--------------------------------------------------------------------------
    const float flash = (float) std::exp (-(now - onsetTime) / 0.1) * velocity;
    const float spacing = juce::jmin (18.0f, lane.getWidth() * 0.35f / (float) juce::jmax (1, shown.count));
    const float lineEnd = lane.getRight() - 44.0f;
    float lastLabelY = -1000.0f;
    bool shiftLabel = false;

    for (int i = 0; i < shown.count; ++i)
    {
        const float y = yOf (shown[i]);
        const float x = lane.getX() + 46.0f + (float) i * spacing;

        g.setColour (accent.withAlpha (0.15f + 0.30f * life));
        g.drawLine (x, y, lineEnd, y, 2.0f);

        const float r = 5.0f + 3.0f * flash;
        g.setColour (accent.withAlpha (0.35f + 0.65f * life));
        g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);

        // Notas a un semitono se pisarian: las etiquetas alternan de columna.
        shiftLabel = std::abs (y - lastLabelY) < 13.0f ? ! shiftLabel : false;
        lastLabelY = y;

        g.setColour (accentSoft.withAlpha (0.4f + 0.6f * life));
        g.drawText (noteName (shown[i]),
                    juce::Rectangle<float> (lane.getRight() - (shiftLabel ? 80.0f : 40.0f), y - 7.0f, 36.0f, 14.0f),
                    juce::Justification::centredRight, false);
    }
}
}
