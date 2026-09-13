#include "StepGrid.h"
#include "Palette.h"
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
}

//==============================================================================
class StepGrid::Cell final : public juce::Component
{
public:
    Cell (StepGrid& o, int i) : owner (o), index (i)
    {
        setWantsKeyboardFocus (true);
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

        const bool contentChanged = ! sameStep (s, step);
        step = s;
        inLength = isInLength;
        playing = isPlaying;

        if (contentChanged)
        {
            setDescription (! step.on ? juce::String ("Off")
                                      : "On, velocity " + juce::String (juce::roundToInt (step.velocity * 100.0f))
                                            + "%, probability " + juce::String (juce::roundToInt (step.probability * 100.0f))
                                            + "%, ratchet " + (step.ratchet > 1 ? "x" + juce::String (step.ratchet) : juce::String ("off")));
        }

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace colours;

        auto b = getLocalBounds().toFloat().reduced (1.5f);
        const float dim = inLength ? 1.0f : 0.35f;

        g.setColour ((index % 4 == 0 ? raised.brighter (0.06f) : raised).withMultipliedAlpha (dim));
        g.fillRoundedRectangle (b, 3.0f);

        if (step.on)
        {
            auto inner = b.reduced (3.0f);
            const float h = inner.getHeight() * juce::jlimit (0.12f, 1.0f, step.velocity);
            g.setColour ((playing ? accent : accent.withAlpha (0.55f)).withMultipliedAlpha (dim));
            g.fillRoundedRectangle (inner.removeFromBottom (h), 2.0f);

            auto marks = b.reduced (4.0f).removeFromTop (12.0f);

            // Ratchet: una marca por disparo.
            if (step.ratchet > 1)
            {
                g.setColour (text.withMultipliedAlpha (dim));

                for (int k = 0; k < step.ratchet; ++k)
                    g.fillRect (marks.getX() + (float) k * 5.0f, marks.getY(), 3.0f, 3.0f);
            }

            if (step.probability < 0.999f)
            {
                g.setColour (text.withMultipliedAlpha (dim));
                g.setFont (fonts::value (10.5f));
                g.drawText (juce::String (juce::roundToInt (step.probability * 100.0f)),
                            marks.withTrimmedTop (2.0f), juce::Justification::centredRight, false);
            }
        }

        if (playing)
        {
            g.setColour (accent);
            g.drawRoundedRectangle (b, 3.0f, 2.0f);
        }

        if (hasKeyboardFocus (false))
        {
            g.setColour (text);
            g.drawRoundedRectangle (b.reduced (1.0f), 3.0f, 1.2f);
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

    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

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

    float dragStartVelocity = 0.8f;
    bool dragged = false;
    bool turnOffOnRelease = false;
};

//==============================================================================
StepGrid::StepGrid (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    setTitle ("Sequencer steps");

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
    // Separacion extra cada 4 pasos: se lee el compas de un vistazo.
    constexpr float gap = 3.0f, groupGap = 8.0f;

    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.alignItems = juce::FlexBox::AlignItems::stretch;

    for (int i = 0; i < seq::kNumSteps; ++i)
    {
        const float right = i + 1 == seq::kNumSteps ? 0.0f : ((i + 1) % 4 == 0 ? groupGap : gap);
        fb.items.add (juce::FlexItem (*cells[(size_t) i]).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0.0f, right, 0.0f, 0.0f)));
    }

    fb.performLayout (getLocalBounds());
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

void StepGrid::tick (int playingStep)
{
    const int length = getLength();

    for (int i = 0; i < seq::kNumSteps; ++i)
        cells[(size_t) i]->refresh (getDisplayedStep (i), i < length, i == playingStep);
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
