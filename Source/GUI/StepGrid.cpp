#include "StepGrid.h"
#include "SP92/Sp92LookAndFeel.h"
#include "SP92/Sp92Fonts.h"
#include "SP92/ButtonMotion.h"
#include "../Params/ParameterIDs.h"

namespace stabr::gui
{
namespace
{
    bool sameStep (const seq::Step& a, const seq::Step& b) noexcept
    {
        return a.on == b.on && a.ratchet == b.ratchet
            && std::abs (a.velocity - b.velocity) < 1.0e-4f
            && std::abs (a.probability - b.probability) < 1.0e-4f;
    }

    float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

    void insetRounded (juce::Graphics& g, juce::Rectangle<float> r, float radius, float dy, juce::Colour c)
    {
        juce::Path clip, crescent;
        clip.addRoundedRectangle (r, radius);
        crescent.addRoundedRectangle (r, radius);
        crescent.addRoundedRectangle (r.translated (0.0f, dy), radius);
        crescent.setUsingNonZeroWinding (false);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (clip);
        g.setColour (c);
        g.fillPath (crescent);
    }
}

//==============================================================================
class StepGrid::Cell final : public juce::Component
{
public:
    Cell (StepGrid& o, int i) : owner (o), index (i)
    {
        setWantsKeyboardFocus (true);
        setPaintingIsUnclipped (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Step " + juce::String (i + 1));

        const auto n = (size_t) i;
        getProperties().set (stepParamIdsProperty,
                             juce::String (pid::seqStepOn[n]) + "," + pid::seqStepVelocity[n] + ","
                                 + pid::seqStepProbability[n] + "," + pid::seqStepRatchet[n]);
    }

    void refresh (const seq::Step& s, bool isInLength, bool isPlaying)
    {
        if (sameStep (s, step) && isInLength == inLength && isPlaying == playing)
            return;

        if (! sameStep (s, step))
            setDescription (! s.on ? juce::String ("Off")
                                   : "On, velocity " + juce::String (juce::roundToInt (s.velocity * 100.0f))
                                         + "%, probability " + juce::String (juce::roundToInt (s.probability * 100.0f))
                                         + "%, ratchet " + (s.ratchet > 1 ? "x" + juce::String (s.ratchet) : juce::String ("off")));

        step = s;
        inLength = isInLength;
        playing = isPlaying;
        repaint();
    }

    void animate (float dt)
    {
        const float onTarget   = step.on ? 1.0f : 0.0f;
        const float headTarget = playing ? 1.0f : 0.0f;
        const float ledTarget  = step.on ? 0.55f + 0.45f * juce::jlimit (0.0f, 1.0f, step.velocity) : 0.0f;
        const float pressTarget = isMouseButtonDown() ? 1.0f : 0.0f;
        const float fadeTarget = inLength ? 1.0f : 0.4f;

        if (dt < 0.0f)
        {
            onAmt = onTarget; head = headTarget; led = ledTarget; press = pressTarget; fade = fadeTarget;
            repaint();
            return;
        }

        const float o = sp92::anim::approach (onAmt, onTarget, dt, 0.04f);
        const float h = sp92::anim::approach (head, headTarget, dt, 0.017f, 0.075f);
        const float l = sp92::anim::approach (led, ledTarget, dt, 0.03f, 0.07f);
        const float p = sp92::anim::approach (press, pressTarget, dt, 0.017f, 0.04f);
        const float f = sp92::anim::approach (fade, fadeTarget, dt, 0.06f);

        if (o != onAmt || h != head || l != led || p != press || f != fade)
        {
            onAmt = o; head = h; led = l; press = p; fade = f;
            sp92::repaintWithMargin (*this, 16);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto& fin = sp92::currentFinish (*this);
        const float size = (float) StepGrid::kPadSize;
        const float sink = 3.0f * head + 1.5f * press * (1.0f - head);
        const auto face = juce::Rectangle<float> (0.0f, sink, size, size);
        constexpr float radius = 9.0f;

        if (fade < 0.999f)
            g.beginTransparencyLayer (fade);

        // box-shadow: 0 6px 10px .25, 0 4px 0 .35; encendido: aro de 2 px y brillo
        // de 16 px; sonando: aro pleno y brillo de 24 px.
        sp92::anim::blurredRoundedRect (g, face, radius, fin.led.withAlpha (0.60f * head), 24.0f);
        sp92::anim::blurredRoundedRect (g, face, radius, fin.led.withAlpha (0.35f * onAmt * (1.0f - head)), 16.0f);
        sp92::anim::blurredRoundedRect (g, face.translated (0.0f, 6.0f), radius, juce::Colours::black.withAlpha (0.25f * (1.0f - head)), 10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (face.translated (0.0f, lerp (4.0f, 1.0f, head)), radius);

        const float ring = juce::jmax (0.55f * onAmt, head);
        if (ring > 0.001f)
        {
            g.setColour (fin.led.withAlpha (ring));
            g.fillRoundedRectangle (face.expanded (2.0f), radius + 2.0f);
        }

        // Goma: linear-gradient(160deg, hi, pad 55%, lo).
        {
            const float a = 160.0f * juce::MathConstants<float>::pi / 180.0f;
            const juce::Point<float> dir { std::sin (a), -std::cos (a) };
            const float len = std::abs (size * dir.x) + std::abs (size * dir.y);
            const auto c = face.getCentre();
            juce::ColourGradient rubber (fin.padHi, c - dir * (len * 0.5f), fin.padLo, c + dir * (len * 0.5f), false);
            rubber.addColour (0.55, fin.pad);
            g.setGradientFill (rubber);
            g.fillRoundedRectangle (face, radius);
        }

        insetRounded (g, face, radius, 1.0f, juce::Colours::white.withAlpha (lerp (0.16f, 0.12f, head)));

        for (int k = 0; k < 5; ++k)
            insetRounded (g, face, radius, -(lerp (3.0f, 2.0f, head) + lerp (6.0f, 4.0f, head) * ((float) k / 4.0f - 0.5f)),
                          juce::Colours::black.withAlpha (lerp (0.45f, 0.40f, head) * 1.2f / 5.0f));

        // LED del paso.
        const juce::Rectangle<float> ledRect { size - 14.0f, face.getY() + 7.0f, 7.0f, 7.0f };
        sp92::anim::ledGlow (g, ledRect, fin.led, led, 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f).interpolatedWith (fin.led, led));
        g.fillEllipse (ledRect);

        g.setColour (juce::Colours::white.withAlpha (0.28f));
        sp92::fonts::drawText (g, juce::String (index + 1), sp92::fonts::label (9.0f, 0.08f),
                               { 7.0f, face.getBottom() - 15.0f, 24.0f, 9.0f }, juce::Justification::centredLeft);

        if (step.on && step.probability < 0.999f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            sp92::fonts::drawText (g, juce::String (juce::roundToInt (step.probability * 100.0f)), sp92::fonts::label (9.0f, 0.04f),
                                   { size - 30.0f, face.getBottom() - 15.0f, 23.0f, 9.0f }, juce::Justification::centredRight);
        }

        if (step.on && step.ratchet > 1)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));

            for (int k = 0; k < step.ratchet; ++k)
                g.fillEllipse (7.0f + (float) k * 6.0f, face.getY() + 8.5f, 3.5f, 3.5f);
        }

        if (fade < 0.999f)
            g.endTransparencyLayer();

        if (hasKeyboardFocus (false))
        {
            g.setColour (fin.led);
            g.drawRoundedRectangle (face.expanded (4.0f), radius + 4.0f, 2.0f);
        }
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showMenu();
            return;
        }

        dragged = false;
        turnOffOnRelease = step.on;

        if (! step.on)
            owner.toggleStep (index);

        dragStartVelocity = owner.getDisplayedStep (index).velocity;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;

        const int dy = e.getDistanceFromDragStartY();

        if (std::abs (dy) > 3)
            dragged = true;

        if (dragged)
            owner.setVelocity (index, dragStartVelocity - (float) dy / (float) juce::jmax (40, getHeight() * 2));
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu() && turnOffOnRelease && ! dragged)
            owner.toggleStep (index);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
        {
            owner.toggleStep (index);
            return true;
        }

        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            if (step.on)
                owner.setVelocity (index, step.velocity + (key == juce::KeyPress::upKey ? 0.1f : -0.1f));
            return true;
        }

        if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey)
        {
            owner.focusStep (index + (key == juce::KeyPress::rightKey ? 1 : -1));
            return true;
        }

        if (key.getTextCharacter() == 'r' || key.getTextCharacter() == 'R')
        {
            owner.setRatchet (index, step.ratchet % seq::kMaxRatchet + 1);
            return true;
        }

        return false;
    }

    void focusGained (FocusChangeType) override { sp92::repaintWithMargin (*this, 8); }
    void focusLost (FocusChangeType) override   { sp92::repaintWithMargin (*this, 8); }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (
            *this, juce::AccessibilityRole::toggleButton,
            juce::AccessibilityActions()
                .addAction (juce::AccessibilityActionType::press,  [this] { owner.toggleStep (index); })
                .addAction (juce::AccessibilityActionType::toggle, [this] { owner.toggleStep (index); }));
    }

private:
    void showMenu()
    {
        juce::PopupMenu ratchet;
        for (int r = 1; r <= seq::kMaxRatchet; ++r)
            ratchet.addItem (10 + r, r == 1 ? juce::String ("Off") : "x" + juce::String (r), true, step.ratchet == r);

        juce::PopupMenu probability;
        for (int p : { 100, 75, 50, 25 })
            probability.addItem (100 + p, juce::String (p) + " %", true, juce::roundToInt (step.probability * 100.0f) == p);

        juce::PopupMenu menu;
        menu.addSectionHeader ("Step " + juce::String (index + 1));
        menu.addSubMenu ("Ratchet", ratchet);
        menu.addSubMenu ("Probability", probability);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safe = juce::Component::SafePointer<Cell> (this)] (int result)
                            {
                                if (safe == nullptr || result == 0)
                                    return;

                                if (result > 10 && result <= 10 + seq::kMaxRatchet)
                                    safe->owner.setRatchet (safe->index, result - 10);
                                else if (result > 100)
                                    safe->owner.setProbability (safe->index, (float) (result - 100) / 100.0f);
                            });
    }

    StepGrid& owner;
    int index;
    seq::Step step { true, -1.0f, -1.0f, 0 };   // invalido a proposito: fuerza el primer refresh
    bool inLength = true;
    bool playing = false;

    float onAmt = 0.0f, head = 0.0f, led = 0.0f, press = 0.0f, fade = 1.0f;

    float dragStartVelocity = 0.8f;
    bool dragged = false;
    bool turnOffOnRelease = false;
};

//==============================================================================
StepGrid::StepGrid (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    setTitle ("Sequencer steps");
    setInterceptsMouseClicks (false, true);

    for (int i = 0; i < seq::kNumSteps; ++i)
    {
        cells[(size_t) i] = std::make_unique<Cell> (*this, i);
        addAndMakeVisible (*cells[(size_t) i]);
    }

    tick (-1);
}

StepGrid::~StepGrid() = default;

void StepGrid::resized()
{
    for (int i = 0; i < seq::kNumSteps; ++i)
        cells[(size_t) i]->setBounds (kInset + (i % 4) * kPadPitch, kInset + (i / 4) * kPadPitch, kPadSize, kPadSize + 5);
}

bool StepGrid::isCustomPattern() const
{
    return (seq::PatternId) (int) apvts.getRawParameterValue (pid::seqPattern)->load() == seq::PatternId::custom;
}

int StepGrid::getLength() const
{
    return (int) apvts.getRawParameterValue (pid::seqSteps)->load();
}

seq::Step StepGrid::getDisplayedStep (int index) const
{
    const auto i = (size_t) juce::jlimit (0, seq::kNumSteps - 1, index);

    if (! isCustomPattern())
    {
        const auto id = (seq::PatternId) (int) apvts.getRawParameterValue (pid::seqPattern)->load();
        return seq::factoryPattern (id)[i];
    }

    seq::Step s;
    s.on          = apvts.getRawParameterValue (pid::seqStepOn[i])->load() > 0.5f;
    s.velocity    = apvts.getRawParameterValue (pid::seqStepVelocity[i])->load();
    s.probability = apvts.getRawParameterValue (pid::seqStepProbability[i])->load();
    s.ratchet     = (int) apvts.getRawParameterValue (pid::seqStepRatchet[i])->load();
    return s;
}

void StepGrid::tick (int playingStep, float dt)
{
    const int length = getLength();

    for (int i = 0; i < seq::kNumSteps; ++i)
    {
        auto& cell = *cells[(size_t) i];
        cell.refresh (getDisplayedStep (i), i < length, i == playingStep);
        cell.animate (dt);
    }
}

//==============================================================================
void StepGrid::setPlain (const char* paramID, float plainValue)
{
    if (auto* p = apvts.getParameter (paramID))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void StepGrid::makeCustomIfNeeded()
{
    if (isCustomPattern())
        return;

    // Se copia lo que se estaba viendo, y recien despues se cambia a Custom:
    // al reves, por un instante sonarian los pasos editables viejos.
    const auto id = (seq::PatternId) (int) apvts.getRawParameterValue (pid::seqPattern)->load();
    const auto& pattern = seq::factoryPattern (id);

    for (size_t i = 0; i < (size_t) seq::kNumSteps; ++i)
    {
        setPlain (pid::seqStepOn[i], pattern[i].on ? 1.0f : 0.0f);
        setPlain (pid::seqStepVelocity[i], pattern[i].on ? pattern[i].velocity : 0.8f);
        setPlain (pid::seqStepProbability[i], pattern[i].probability);
        setPlain (pid::seqStepRatchet[i], (float) pattern[i].ratchet);
    }

    setPlain (pid::seqPattern, (float) seq::PatternId::custom);
}

void StepGrid::toggleStep (int index)
{
    const auto i = (size_t) juce::jlimit (0, seq::kNumSteps - 1, index);
    const bool wasOn = getDisplayedStep (index).on;

    makeCustomIfNeeded();
    setPlain (pid::seqStepOn[i], wasOn ? 0.0f : 1.0f);
}

void StepGrid::setVelocity (int index, float velocity)
{
    makeCustomIfNeeded();
    setPlain (pid::seqStepVelocity[(size_t) juce::jlimit (0, seq::kNumSteps - 1, index)], juce::jlimit (0.0f, 1.0f, velocity));
}

void StepGrid::setProbability (int index, float probability)
{
    makeCustomIfNeeded();
    setPlain (pid::seqStepProbability[(size_t) juce::jlimit (0, seq::kNumSteps - 1, index)], juce::jlimit (0.0f, 1.0f, probability));
}

void StepGrid::setRatchet (int index, int ratchet)
{
    makeCustomIfNeeded();
    setPlain (pid::seqStepRatchet[(size_t) juce::jlimit (0, seq::kNumSteps - 1, index)],
              (float) juce::jlimit (1, seq::kMaxRatchet, ratchet));
}

void StepGrid::focusStep (int index)
{
    if (index >= 0 && index < seq::kNumSteps)
        cells[(size_t) index]->grabKeyboardFocus();
}
}
