#include "PresetBar.h"
#include "Palette.h"

namespace stabr::gui
{
namespace
{
    constexpr int kUserIdBase = 1001;
}

PresetBar::PresetBar (presets::PresetManager& m) : manager (m)
{
    auto setup = [this] (juce::Component& c, const juce::String& title)
    {
        c.setTitle (title);
        c.getProperties().set (layoutItemProperty, true);
        addAndMakeVisible (c);
    };

    setup (prevButton,   "Previous preset");
    setup (presetBox,    "Preset");
    setup (nextButton,   "Next preset");
    setup (saveButton,   "Save preset");
    setup (createButton, "Create STAB: generate a new sound");
    setup (undoButton,   "Undo sound change");
    setup (redoButton,   "Redo sound change");
    setup (lockChord,    "Lock the chord during Create STAB");
    setup (lockFx,       "Lock the effects during Create STAB");
    setup (lockSeq,      "Lock the sequencer during Create STAB");

    lockLabel.setText ("LOCK", juce::dontSendNotification);
    lockLabel.setFont (fonts::title (10.0f));
    lockLabel.setColour (juce::Label::textColourId, colours::textMuted);
    lockLabel.setJustificationType (juce::Justification::centredRight);
    lockLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (lockLabel);

    createButton.getProperties().set ("accent", true);
    createButton.setColour (juce::TextButton::textColourOffId, colours::background);
    createButton.setColour (juce::TextButton::textColourOnId, colours::background);

    presetBox.getProperties().set ("fontHeight", 14.0f);

    prevButton.onClick   = [this] { step (-1); };
    nextButton.onClick   = [this] { step (+1); };
    presetBox.onChange   = [this] { loadSelected(); };
    saveButton.onClick   = [this] { save(); };
    createButton.onClick = [this] { create(); };
    undoButton.onClick   = [this] { manager.undo(); tick(); };
    redoButton.onClick   = [this] { manager.redo(); tick(); };

    rebuildList();
    tick();
}

PresetBar::~PresetBar() = default;

//==============================================================================
void PresetBar::rebuildList()
{
    presetBox.clear (juce::dontSendNotification);

    const auto& factory = manager.getFactoryPresets();
    juce::String category;

    for (int i = 0; i < (int) factory.size(); ++i)
    {
        const auto& preset = factory[(size_t) i];

        if (preset.category != category)
        {
            category = preset.category;
            presetBox.addSectionHeading (category);
        }

        presetBox.addItem (preset.name, i + 1);
    }

    userFiles = manager.getUserPresetFiles();

    if (! userFiles.isEmpty())
    {
        presetBox.addSectionHeading ("User");

        for (int i = 0; i < userFiles.size(); ++i)
            presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), kUserIdBase + i);
    }

    shownName = {};   // fuerza a que tick() vuelva a mostrar el nombre
}

void PresetBar::loadSelected()
{
    const int id = presetBox.getSelectedId();

    if (id <= 0)
        return;

    if (id < kUserIdBase)
        manager.loadFactoryPreset (id - 1);
    else if (id - kUserIdBase < userFiles.size())
        manager.loadUserPreset (userFiles[id - kUserIdBase]);

    tick();
}

void PresetBar::step (int delta)
{
    const int factoryCount = (int) manager.getFactoryPresets().size();
    const int total = factoryCount + userFiles.size();

    if (total == 0)
        return;

    const int id = presetBox.getSelectedId();
    int index = -1;

    if (id > 0)
        index = id < kUserIdBase ? id - 1 : factoryCount + (id - kUserIdBase);

    // Desde un sonido generado (que no esta en la lista), "siguiente" va al
    // primero y "anterior" al ultimo.
    if (index < 0)
        index = delta > 0 ? -1 : 0;

    index = ((index + delta) % total + total) % total;

    const int newId = index < factoryCount ? index + 1 : kUserIdBase + (index - factoryCount);
    presetBox.setSelectedId (newId, juce::sendNotificationSync);
}

void PresetBar::save()
{
    auto* window = new juce::AlertWindow ("Save preset", "Give this sound a name.",
                                          juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", manager.getCurrentPresetName(), "Name");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBar> safe (this);

    // El callback corre antes de que la ventana se destruya, asi que 'window'
    // todavia es valido adentro.
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        const auto saved = safe->manager.saveUserPreset (window->getTextEditorContents ("name"));

        if (saved.failed())
            juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon,
                                                                                   "Couldn't save the preset",
                                                                                   saved.getErrorMessage()),
                                          nullptr);

        safe->rebuildList();
        safe->tick();
    }), true);
}

void PresetBar::create()
{
    presets::GeneratorLocks locks;
    locks.chord = lockChord.getToggleState();
    locks.fx = lockFx.getToggleState();
    locks.sequencer = lockSeq.getToggleState();

    juce::Component::SafePointer<PresetBar> safe (this);
    manager.createStab (locks, [safe] (bool)
    {
        if (safe != nullptr)
            safe->tick();
    });

    tick();
}

//==============================================================================
void PresetBar::tick()
{
    const auto name = manager.getCurrentPresetName();

    if (name != shownName)
    {
        shownName = name;
        int match = 0;

        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == name)
            {
                match = presetBox.getItemId (i);
                break;
            }

        // Un sonido generado no esta en la lista: se muestra su nombre igual.
        if (match > 0)
            presetBox.setSelectedId (match, juce::dontSendNotification);
        else
            presetBox.setText (name, juce::dontSendNotification);
    }

    if (undoButton.isEnabled() != manager.canUndo())
        undoButton.setEnabled (manager.canUndo());

    if (redoButton.isEnabled() != manager.canRedo())
        redoButton.setEnabled (manager.canRedo());

    const bool generating = manager.isGenerating();

    if (generating != lastGenerating)
    {
        lastGenerating = generating;
        createButton.setEnabled (! generating);
        createButton.setButtonText (generating ? "CREATING..." : "CREATE STAB");
    }
}

void PresetBar::resized()
{
    juce::FlexBox fb;
    fb.alignItems = juce::FlexBox::AlignItems::center;

    auto add = [&fb] (juce::Component& c, float width, float height, float gapAfter)
    {
        fb.items.add (juce::FlexItem (c).withWidth (width).withHeight (height)
                                        .withMargin (juce::FlexItem::Margin (0.0f, gapAfter, 0.0f, 0.0f)));
    };

    add (prevButton, 26.0f, 28.0f, 4.0f);
    fb.items.add (juce::FlexItem (presetBox).withFlex (1.0f).withMinWidth (140.0f).withHeight (28.0f)
                                            .withMargin (juce::FlexItem::Margin (0.0f, 4.0f, 0.0f, 0.0f)));
    add (nextButton, 26.0f, 28.0f, 4.0f);
    add (saveButton, 48.0f, 28.0f, 14.0f);
    add (createButton, 132.0f, 30.0f, 4.0f);
    add (undoButton, 44.0f, 28.0f, 4.0f);
    add (redoButton, 44.0f, 28.0f, 14.0f);
    // 40 px: con 30 el espaciado de la fuente de titulos cortaba "LOCK" en "LO...".
    add (lockLabel, 40.0f, 20.0f, 4.0f);
    add (lockChord, 64.0f, 26.0f, 4.0f);
    add (lockFx, 50.0f, 26.0f, 4.0f);
    add (lockSeq, 54.0f, 26.0f, 0.0f);

    fb.performLayout (getLocalBounds());
}
}
