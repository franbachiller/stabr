#include "DeviceView.h"
#include "LcdPages.h"
#include "Sp92Fonts.h"
#include "Sp92LookAndFeel.h"
#include "../ChordText.h"
#include "../EchoModel.h"
#include "../../PluginProcessor.h"
#include "../../Params/ParameterIDs.h"
#include "../../Presets/Preset.h"
#include "../../Presets/FactoryPresets.h"
#include "../../Presets/PresetManager.h"
#include "../../Chord/ChordGenerator.h"
#include "../../Common/Utf8.h"

#include <climits>
#include <cmath>

namespace stabr::gui::sp92
{
namespace
{
    using APVTS = juce::AudioProcessorValueTreeState;

    constexpr int kUserIdBase = 1001;
    constexpr int kSaveId = 9000;
    constexpr float kScales[] { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };

    int factoryIndexOf (const juce::String& name)
    {
        const auto& list = presets::factoryPresets();

        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].name == name)
                return (int) i;

        return -1;
    }

    /*  Color del disquete: por estilo en los de fabrica, crema en los del
        usuario, y uno fijo por nombre en los generados. */
    juce::Colour diskColour (const juce::String& category, const juce::String& name, bool user)
    {
        if (user)                    return juce::Colour (0xfff0ece2);
        if (category == "House")     return juce::Colour (0xff2a4ea6);
        if (category == "Techno")    return juce::Colour (0xff1f1f22);
        if (category == "Rave")      return juce::Colour (0xffb8392c);
        if (category == "World")     return juce::Colour (0xff2f7a5a);
        if (category == "Bass")      return juce::Colour (0xffe3a321);
        if (category == "Atmos")     return juce::Colour (0xff5b3b8c);

        const juce::Colour palette[] { juce::Colour (0xff2a4ea6), juce::Colour (0xff1f1f22), juce::Colour (0xffb8392c),
                                       juce::Colour (0xff2f7a5a), juce::Colour (0xffe3a321), juce::Colour (0xff5b3b8c) };
        return palette[(size_t) ((unsigned) name.hashCode() % 6u)];
    }
}

//==============================================================================
DeviceView::DeviceView (StabrProcessor& p)
    : proc (p), apvts (p.getAPVTS()), presets (p.getPresetManager()),
      steps (p.getAPVTS()),
      patternSwitch (juce::StringArray ("Off-beat", "3+3+2", "Skippy", "Sparse", "Custom"), "Sequencer pattern")
{
    cache.resolve (apvts);
    setOpaque (true);
    addAndMakeVisible (chassis);

    //--------------------------------------------------------------------------
    // Franja: encendido y disquetera
    //--------------------------------------------------------------------------
    place (power, 631, 24, 56, 40);
    motion.add (power, 14);
    power.onClick = [this] { showSetupMenu(); };

    place (disk, 688, 12, 262, 71);
    disk.onClick = [this] { showSoundMenu(); };

    setupKey (diskPrev, {}, true, false, "Previous sound");
    diskPrev.getProperties().set ("sp92Arrow", "left");
    place (diskPrev, 962, 14, 46, 33);
    diskPrev.onClick = [this] { stepSound (-1); };

    setupKey (diskNext, {}, true, false, "Next sound");
    diskNext.getProperties().set ("sp92Arrow", "right");
    place (diskNext, 1014, 14, 46, 33);
    diskNext.onClick = [this] { stepSound (+1); };

    setupKey (diskSave, "Save", true, false, "Save the sound to a user disk");
    place (diskSave, 962, 52, 98, 29);
    diskSave.onClick = [this] { saveSound(); };

    //--------------------------------------------------------------------------
    // Pantalla, teclas de funcion y encoders
    //--------------------------------------------------------------------------
    chassis.addBezel ({ 28.0f, 108.0f, 560.0f, 238.0f }, utf8 ("DOT MATRIX 190 \xc3\x97 72 \xc2\xb7 BACKLIT"));
    place (lcd, 46, 124, 524, 196);

    const auto& pages = lcdPages();

    for (int i = 0; i < (int) softKeys.size(); ++i)
    {
        auto& key = softKeys[(size_t) i];
        setupKey (key, pages[(size_t) i].name, true, true, juce::String (pages[(size_t) i].name) + " page");
        place (key, 28 + i * 81, 356, 74, 35);
        key.onClick = [this, i]
        {
            const int count = (int) lcdPages()[(size_t) i].subPages.size();
            selectPage (i, i == page ? (subPage + 1) % count : 0);
        };
    }

    for (int i = 0; i < 6; ++i)
    {
        const float cx = 46.0f + ((float) i + 0.5f) * 524.0f / 6.0f;
        auto enc = std::make_unique<HardwareKnob> ("encoder", 0, "Encoder " + juce::String (i + 1));
        place (*enc, juce::roundToInt (cx - 19.0f), 411, 38, 38);
        chassis.addSilk (juce::String (i + 1), cx, 457.0f);
        encoders[(size_t) i] = std::move (enc);
    }

    //--------------------------------------------------------------------------
    // Tono y eco
    //--------------------------------------------------------------------------
    chassis.addHead ("TONE", { 612.0f, 104.0f, 460.0f, 11.0f });
    addKnob (pid::filterCutoff,    "CUTOFF",     "big",   11, 690.0f, 175.0f, 86.0f);
    addKnob (pid::filterResonance, "RESONANCE",  "big",   11, 842.0f, 175.0f, 86.0f);
    addKnob (pid::ampDecay,        "DECAY",      "big",   11, 994.0f, 175.0f, 86.0f);
    addKnob (pid::filterEnvAmount, "ENV AMOUNT", "small", 11, 662.0f, 288.0f, 54.0f);
    addKnob (pid::driveAmount,     "DRIVE",      "small", 11, 756.0f, 288.0f, 54.0f);

    chassis.addHead ("ECHO", { 812.0f, 252.0f, 260.0f, 11.0f });
    chassis.addBezel ({ 812.0f, 272.0f, 260.0f, 76.0f });
    place (echo, 821, 281, 242, 58);
    addKnob (pid::delayDivision, "TIME",     "switch", 8,  852.0f, 384.0f, 44.0f);
    addKnob (pid::delayFeedback, "FEEDBACK", "small",  11, 942.0f, 384.0f, 44.0f);
    addKnob (pid::delayMix,      "MIX",      "small",  11, 1032.0f, 384.0f, 44.0f);

    const char* modes[] { "STEREO", "PING", "DUB" };
    for (int i = 0; i < 3; ++i)
    {
        auto& key = modeKeys[(size_t) i];
        setupKey (key, modes[i], false, true, juce::String ("Delay mode ") + modes[i]);
        place (key, 812 + i * 88, 430, 84, 27);
        key.onClick = [this, i] { setParam (pid::delayMode, (float) i); };
    }

    place (vu, 612, 342, 178, 110);

    //--------------------------------------------------------------------------
    // Secuencia
    //--------------------------------------------------------------------------
    seqHead.setBounds (28, 486, 548, 11);
    addAndMakeVisible (seqHead);

    chassis.addWell ({ 28.0f, 504.0f, 252.0f, 252.0f });
    place (steps, 28, 504, 252, 252);

    for (int r = 0; r < 4; ++r)
        chassis.addSilk (juce::String (r + 1), 288.0f, 504.0f + 12.0f + (float) r * 59.0f + 20.0f, false, true);
    chassis.addSilk ("BEAT", 284.0f, 506.0f, false, true);

    setupKey (seqKey, "SEQ", true, true, "Sequencer on");
    seqKey.getProperties().set ("sp92Play", true);
    seqKey.setClickingTogglesState (true);
    place (seqKey, 344, 506, 98, 43);
    seqAttachment = std::make_unique<APVTS::ButtonAttachment> (apvts, pid::seqEnabled, seqKey);
    seqKey.getProperties().set (paramIdProperty, juce::String (pid::seqEnabled));

    // La perilla ocupa su caja; las posiciones se serigrafian alrededor sin recorte.
    place (patternSwitch, 459, 545, 82, 82);
    patternAttachment = std::make_unique<APVTS::SliderAttachment> (apvts, pid::seqPattern, patternSwitch.getKnob());
    patternSwitch.getKnob().getProperties().set (paramIdProperty, juce::String (pid::seqPattern));
    chassis.addSilk ("PATTERN", 500.0f, 634.0f);

    addKnob (pid::seqSwing, "SWING", "small",  11, 380.0f, 696.0f, 42.0f);
    addKnob (pid::seqGate,  "GATE",  "small",  11, 464.0f, 696.0f, 42.0f);
    addKnob (pid::seqRate,  "RATE",  "switch", 6,  548.0f, 696.0f, 42.0f);

    //--------------------------------------------------------------------------
    // Crear y candados
    //--------------------------------------------------------------------------
    chassis.addHead ("CREATE", { 604.0f, 486.0f, 196.0f, 11.0f });
    place (stab, 596, 502, 212, 214);
    stab.onClick = [this]
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        stab.flash (now);
        generateStarted = now;
        overlay.until = -10.0;

        const int bits = proc.getUiLocks();
        presets::GeneratorLocks locks;
        locks.chord = (bits & 1) != 0;
        locks.fx = (bits & 2) != 0;
        locks.sequencer = (bits & 4) != 0;
        presets.createStab (locks);
    };

    setupKey (undoKey, "Undo", false, false, "Undo sound change");
    place (undoKey, 604, 718, 94, 33);
    undoKey.onClick = [this] { presets.undo(); };

    setupKey (redoKey, "Redo", false, false, "Redo sound change");
    place (redoKey, 706, 718, 94, 33);
    redoKey.onClick = [this] { presets.redo(); };

    chassis.addHead ("LOCK", { 836.0f, 486.0f, 236.0f, 11.0f });
    const char* lockNames[] { "CHORD", "FX", "SEQ" };

    for (int i = 0; i < 3; ++i)
    {
        auto bat = std::make_unique<BatSwitch> (juce::String ("Lock ") + lockNames[i] + " during Create STAB");
        bat->setToggleState ((proc.getUiLocks() & (1 << i)) != 0, juce::dontSendNotification);
        bat->onClick = [this]
        {
            int bits = 0;
            for (int k = 0; k < 3; ++k)
                bits |= bats[(size_t) k]->getToggleState() ? (1 << k) : 0;
            proc.setUiLocks (bits);
        };
        place (*bat, 849 + i * 80, 508, 46, 80);
        chassis.addSilk (lockNames[i], 872.0f + (float) i * 80.0f, 592.0f);
        bats[(size_t) i] = std::move (bat);
    }

    addKnob (pid::outputGain, "OUTPUT", "small", 11, 872.0f, 646.0f, 50.0f);

    tape.setText (proc.getTapeText(), juce::dontSendNotification);
    tape.onTextChange = [this] { proc.setTapeText (tape.getText()); };
    place (tape, 836, 700, 248, 52);

    presets.onChange = [this] { onSoundChanged(); };

    selectPage (0, 0);
    updateDisk();
}

DeviceView::~DeviceView()
{
    presets.onChange = nullptr;
}

//==============================================================================
void DeviceView::place (juce::Component& c, int x, int y, int w, int h)
{
    c.setBounds (x, y, w, h);
    c.getProperties().set (layoutItemProperty, true);
    addAndMakeVisible (c);
}

void DeviceView::setupKey (juce::TextButton& key, const juce::String& text, bool dark, bool led, const juce::String& title)
{
    key.setButtonText (text);
    key.setTitle (title);
    key.getProperties().set ("sp92Dark", dark);
    key.getProperties().set ("sp92Led", led);
    motion.add (key);
}

HardwareKnob& DeviceView::addKnob (const char* paramID, const juce::String& silk, const juce::String& style, int ticks,
                                   float cx, float cy, float diameter)
{
    auto* param = apvts.getParameter (paramID);
    jassert (param != nullptr);

    auto knob = std::make_unique<HardwareKnob> (style, ticks, param->getName (40));
    const int box = juce::roundToInt (diameter + 24.0f);
    place (*knob, juce::roundToInt (cx - (float) box * 0.5f), juce::roundToInt (cy - (float) box * 0.5f), box, box);

    knobAttachments.push_back (std::make_unique<APVTS::SliderAttachment> (apvts, paramID, *knob));
    knob->setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    knob->getProperties().set (paramIdProperty, juce::String (paramID));

    const juce::String id (paramID);
    knob->onUserChange = [this, silk, id] (HardwareKnob&)
    {
        overlay = { silk, id, juce::Time::getMillisecondCounterHiRes() * 0.001 + 1.3 };
    };

    // La maqueta: la serigrafia a 14 px del borde de la perilla.
    chassis.addSilk (silk, cx, cy + diameter * 0.5f + (ticks > 1 ? 14.0f : 8.0f));

    knobs.push_back (std::move (knob));
    return *knobs.back();
}

void DeviceView::resized()
{
    chassis.setBounds (getLocalBounds());
}

void DeviceView::setFinish (int index)
{
    const auto& f = finish (index);
    chassis.setFinish (f);
    lcd.setFinish (f);
    echo.setFinish (f);
    repaint();
}

float DeviceView::raw (const char* paramID) const
{
    return apvts.getRawParameterValue (paramID)->load();
}

void DeviceView::setParam (const char* paramID, float plainValue)
{
    if (auto* p = apvts.getParameter (paramID))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

//==============================================================================
void DeviceView::selectPage (int newPage, int newSubPage)
{
    const auto& pages = lcdPages();
    page = juce::jlimit (0, (int) pages.size() - 1, newPage);
    subPage = juce::jlimit (0, (int) pages[(size_t) page].subPages.size() - 1, newSubPage);
    overlay.until = -10.0;

    bindEncoders();
    syncLights();
}

void DeviceView::bindEncoders()
{
    const auto& slots = lcdPages()[(size_t) page].subPages[(size_t) subPage];
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (size_t i = 0; i < encoders.size(); ++i)
    {
        auto& enc = *encoders[i];
        encoderAttachments[i].reset();

        if (const auto* id = slots[i].paramID)
        {
            auto* param = apvts.getParameter (id);
            encoderAttachments[i] = std::make_unique<APVTS::SliderAttachment> (apvts, id, enc);
            enc.setEnabled (true);
            enc.setTitle ("Encoder " + juce::String ((int) i + 1) + ": " + param->getName (40));
            enc.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
            enc.getProperties().set (paramIdProperty, juce::String (id));
        }
        else
        {
            enc.setEnabled (false);
            enc.setTitle ("Encoder " + juce::String ((int) i + 1) + " (unused on this page)");
            enc.getProperties().remove (paramIdProperty);
        }

        // Al cambiar de pagina el encoder gira rapido hasta su nuevo parametro.
        enc.animateToCurrentValue (now, 0.0);
    }
}

juce::String DeviceView::getEncoderParamID (int index) const
{
    return encoders[(size_t) juce::jlimit (0, 5, index)]->getProperties()[paramIdProperty].toString();
}

juce::StringArray DeviceView::getControlledParameterIDs() const
{
    juce::StringArray ids;

    for (const auto& k : knobs)
        ids.add (k->getProperties()[paramIdProperty].toString());

    ids.add (pid::seqEnabled);
    ids.add (pid::seqPattern);
    ids.add (pid::delayMode);

    for (int i = 0; i < steps.getNumChildComponents(); ++i)
        for (auto& id : juce::StringArray::fromTokens (steps.getChildComponent (i)->getProperties()[stepParamIdsProperty].toString(), ",", ""))
            ids.add (id);

    for (const auto& pg : lcdPages())
        for (const auto& sub : pg.subPages)
            for (const auto& slot : sub)
                if (slot.paramID != nullptr)
                    ids.add (slot.paramID);

    ids.removeDuplicates (false);
    ids.removeEmptyStrings();
    return ids;
}

//==============================================================================
void DeviceView::onSoundChanged()
{
    // Las perillas giran desde donde estaban hasta el sonido nuevo, en ola.
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    auto stagger = [] (const juce::Component& c) { return kStaggerSeconds * (double) c.getBounds().getCentreX() / (double) kBaseWidth; };

    for (auto& k : knobs)
        k->animateToCurrentValue (now, stagger (*k));

    for (auto& e : encoders)
        e->animateToCurrentValue (now, stagger (*e));

    patternSwitch.getKnob().animateToCurrentValue (now, stagger (patternSwitch));
    updateDisk();
}

void DeviceView::updateDisk()
{
    const auto name = presets.getCurrentPresetName();
    const int index = factoryIndexOf (name);
    const auto& list = presets::factoryPresets();

    bool isUser = false;
    for (const auto& file : presets.getUserPresetFiles())
        isUser = isUser || file.getFileNameWithoutExtension() == name;

    const juce::String header = index >= 0 ? "STABR DISK " + juce::String (index + 1).paddedLeft ('0', 2)
                                           : (isUser ? juce::String ("USER DISK") : juce::String ("CREATED ON THE SP-92"));
    const auto category = index >= 0 ? list[(size_t) index].category : juce::String();

    disk.setDisk (name, header, diskColour (category, name, isUser));
}

void DeviceView::syncLights()
{
    auto setLit = [] (juce::Button& b, bool lit)
    {
        if ((bool) b.getProperties().getWithDefault ("sp92Lit", false) != lit)
            b.getProperties().set ("sp92Lit", lit);
    };

    for (int i = 0; i < (int) softKeys.size(); ++i)
        setLit (softKeys[(size_t) i], i == page);

    const int mode = (int) raw (pid::delayMode);
    for (int i = 0; i < 3; ++i)
        setLit (modeKeys[(size_t) i], i == mode);

    if (undoKey.isEnabled() != presets.canUndo()) undoKey.setEnabled (presets.canUndo());
    if (redoKey.isEnabled() != presets.canRedo()) redoKey.setEnabled (presets.canRedo());
}

//==============================================================================
void DeviceView::tick (double now)
{
    // dt real, acotado: tras una pausa larga (ventana oculta) nada salta.
    const float dt = lastTick > 0.0 ? (float) juce::jlimit (0.0, 0.05, now - lastTick) : 1.0f / 60.0f;
    lastTick = now;
    lastNow = now;

    GuiEvent e;
    while (proc.getGuiFeed().pop (e))
    {
        if (e.notes.count > 0)
            lastNotes = e.notes;

        if (e.type == GuiEvent::Type::onset)
            lastOnset = now;
    }

    syncLights();
    motion.tick (now, dt);

    for (auto& k : knobs)      k->tick (now);
    for (auto& enc : encoders) enc->tick (now);
    patternSwitch.getKnob().tick (now);
    patternSwitch.tick (dt);
    stab.tick (now, dt);

    for (auto& bat : bats)
        bat->tick (dt);

    updateDisk();
    disk.tick (now, dt);

    float peakL = 0.0f, peakR = 0.0f, reduction = 0.0f;
    proc.getFxChain().getOutput().readAndClearMeters (peakL, peakR, reduction);
    vu.push (peakL, peakR);
    vu.tick (dt);

    steps.tick (proc.getSequencerStep(), dt);

    seqHead.setText (utf8 ("SEQUENCE \xc2\xb7 ")
                     + juce::String (juce::roundToInt (proc.getGuiFeed().bpm.load (std::memory_order_relaxed))) + " BPM");

    drawMainLcd (now);
    lcd.commit (dt);
    drawEchoLcd (now);
    echo.commit (dt);
}

//==============================================================================
void DeviceView::drawMainLcd (double now)
{
    constexpr int W = 190;
    lcd.clear();

    const auto name = presets.getCurrentPresetName();
    const int index = factoryIndexOf (name);
    const juce::String number = index >= 0 ? juce::String (index + 1).paddedLeft ('0', 2) : juce::String ("--");

    lcd.text (number + ">" + name.toUpperCase().substring (0, 20), 1, 1);
    lcd.textRight (juce::String (juce::roundToInt (proc.getGuiFeed().bpm.load (std::memory_order_relaxed))) + " BPM", W - 1, 1);
    lcd.hline (0, W - 1, 10, true);

    const bool creating = presets.isGenerating() || now - generateStarted < 0.55;
    const auto& pages = lcdPages();

    if (creating)
    {
        const float progress = (float) juce::jlimit (0.0, 1.0, (now - generateStarted) / 0.55);
        lcd.textCenter ("CREATING", W / 2, 18, 2);

        for (int x = 30; x < 160; ++x) { lcd.set (x, 38); lcd.set (x, 46); }
        for (int x = 31; x < 31 + (int) (progress * 128.0f); ++x)
            for (int y = 40; y < 45; ++y)
                if (x % 2 == 0)
                    lcd.set (x, y);
    }
    else if (page == 0 && subPage == 0)
    {
        auto notes = lastNotes;

        if (notes.count == 0)
        {
            // Todavia no sono nada: el acorde que va a sonar sobre la tonica.
            chord::ChordParams cp;
            cache.fillChordParams (cp);
            notes = chord::generateChord (48 + cp.keyRoot, cp);
        }

        shownChordName = chordNameFor (notes);
        const int scale = shownChordName.length() > 5 ? 2 : 3;
        lcd.text (shownChordName, 2, scale == 3 ? 15 : 19, scale);

        juce::String detail;
        if (auto* voicing = apvts.getParameter (pid::chordVoicing))
            detail << lcdValue (*voicing);
        detail << " INV" << (int) raw (pid::chordInversion) << " " << (int) raw (pid::chordDensity) << "V";
        lcd.text (detail, 2, 40);

        drawVoicing (notes, now);
    }
    else
    {
        lcd.text (pages[(size_t) page].name, 2, 17, 2);
        lcd.text ("PAGE " + juce::String (subPage + 1) + "/" + juce::String ((int) pages[(size_t) page].subPages.size()), 2, 38);
        drawGraph();
    }

    lcd.hline (0, W - 1, 49, true);

    if (overlay.until > now)
    {
        lcd.text (overlay.label, 2, 53);

        if (auto* param = apvts.getParameter (overlay.paramID))
            lcd.textRight (lcdValue (*param), W - 2, 55, 2);
    }
    else if (! creating)
    {
        drawCells();
    }
}

void DeviceView::drawVoicing (const chord::ChordResult& notes, double now)
{
    constexpr int x0 = 104, x1 = 188, y0 = 13, y1 = 46;

    if (notes.count <= 0)
        return;

    int lo = notes[0], hi = notes[0];
    for (int i = 1; i < notes.count; ++i) { lo = juce::jmin (lo, notes[i]); hi = juce::jmax (hi, notes[i]); }

    const int span = juce::jmax (24, hi - lo + 8);
    const int base = (lo + hi) / 2 - span / 2;
    auto yOf = [&] (int n) { return juce::roundToInt ((float) y1 - (float) (n - base) / (float) span * (float) (y1 - y0)); };

    for (int n = ((base + 11) / 12) * 12; n <= base + span; n += 12)
        lcd.hline (x0 + 8, x1, yOf (n), true);

    const bool flash = now - lastOnset < 0.18;

    for (int i = 0; i < notes.count; ++i)
    {
        const int y = yOf (notes[i]), x = x0 + 10 + i * 7;
        lcd.hline (x + 2, x1 - 1, y);
        lcd.block (x - (flash ? 1 : 0), y - 1 - (flash ? 1 : 0), flash ? 4 : 3);
    }
}

void DeviceView::drawGraph()
{
    constexpr int x0 = 84, x1 = 188, y0 = 14, y1 = 46;
    const float h = (float) (y1 - y0), mid = (float) (y0 + y1) * 0.5f;

    auto plot = [this] (const std::function<float (float)>& fn)
    {
        int prev = INT_MIN;

        for (int x = x0; x <= x1; ++x)
        {
            const int y = juce::jlimit (y0, y1, juce::roundToInt (fn ((float) (x - x0) / (float) (x1 - x0))));

            if (prev != INT_MIN)
                for (int yy = prev; yy != y; yy += (y > prev ? 1 : -1))
                    lcd.set (x, yy);

            lcd.set (x, y);
            prev = y;
        }
    };

    switch (page)
    {
        case 1:   // OSC: forma de onda del oscilador 1
        {
            const int wave = (int) raw (pid::osc1Wave);
            plot ([&] (float t)
            {
                const float ph = std::fmod (t * 3.0f, 1.0f);
                float v = 0.0f;
                if      (wave == 0) v = 1.0f - 2.0f * ph;
                else if (wave == 1) v = ph < raw (pid::pulseWidth) ? 1.0f : -1.0f;
                else if (wave == 2) v = 1.0f - 4.0f * std::abs (ph - 0.5f);
                else if (wave == 3) v = (std::sin (ph * 6.283f) + 0.55f * std::sin (ph * 12.566f) + 0.35f * std::sin (ph * 18.85f)) / 1.7f;
                else                v = std::sin (ph * 6.283f);
                return mid - v * (h * 0.5f - 1.0f);
            });
            break;
        }

        case 2:   // FILTER: respuesta aproximada con resonancia
        {
            const float fc = std::log (raw (pid::filterCutoff) / 20.0f) / std::log (1000.0f);
            const float q = raw (pid::filterResonance);
            const float slope = raw (pid::filterSlope) > 0.5f ? 2.0f : 1.0f;
            const int type = (int) raw (pid::filterType);

            plot ([&] (float t)
            {
                const float d = (t - fc) * 6.0f;
                float mag = type == 0 ? (d < 0.0f ? 0.0f : -d * slope)
                          : type == 2 ? (d > 0.0f ? 0.0f : d * slope)
                                      : -std::abs (d) * slope;
                mag += q * 8.0f * std::exp (-std::pow ((t - fc) * 14.0f, 2.0f));
                return (float) y0 + 6.0f - mag * 3.2f;
            });
            break;
        }

        case 3:   // DELAY: cada repeticion mas baja segun el feedback
        {
            float g = 1.0f;
            const float fb = raw (pid::delayFeedback);

            for (int k = 0; k < 9; ++k)
            {
                const int x = x0 + 4 + k * 12;
                const int bar = juce::jmax (1, juce::roundToInt (g * h));
                for (int y = 0; y < bar; ++y) { lcd.set (x, y1 - y); lcd.set (x + 1, y1 - y); }
                g *= fb;
            }
            break;
        }

        case 4:   // REVERB: caida de la cola
        {
            const float len = 0.2f + raw (pid::reverbDecay) * 0.8f;
            plot ([&] (float t) { return (float) y1 - (h - 1.0f) * std::exp (-t / (len * 0.35f)) * (t < 0.03f ? t / 0.03f : 1.0f); });
            break;
        }

        case 5:   // FX: curva de saturacion
        {
            const float drv = 1.0f + raw (pid::satDrive) * 8.0f;
            plot ([&] (float t) { const float x = t * 2.0f - 1.0f; return mid - std::tanh (x * drv) / std::tanh (drv) * (h * 0.5f - 1.0f); });
            break;
        }

        case 6:   // PUMP: la curva del sidechain, cuatro ciclos
        {
            const float depth = raw (pid::pumpDepth);
            const float exponent = 0.4f + 4.0f * raw (pid::pumpShape);
            plot ([&] (float t)
            {
                const float ph = std::fmod (t * 4.0f, 1.0f);
                const float env = ph < 0.06f ? 1.0f - ph / 0.06f : std::pow ((ph - 0.06f) / 0.94f, exponent);
                return (float) y1 - (h - 1.0f) * (1.0f - depth * (1.0f - env));
            });
            break;
        }

        default:  // CHORD 2/2: el voicing tambien
        {
            auto notes = lastNotes;
            if (notes.count == 0)
            {
                chord::ChordParams cp;
                cache.fillChordParams (cp);
                notes = chord::generateChord (48 + cp.keyRoot, cp);
            }
            drawVoicing (notes, lastNow);
            break;
        }
    }
}

void DeviceView::drawCells()
{
    constexpr int W = 190;
    const auto& slots = lcdPages()[(size_t) page].subPages[(size_t) subPage];

    for (int i = 0; i < 6; ++i)
    {
        const int xc = juce::roundToInt (((float) i + 0.5f) * (float) W / 6.0f);
        const auto& slot = slots[(size_t) i];

        if (slot.paramID == nullptr)
        {
            lcd.textCenter ("--", xc, 62);
            continue;
        }

        lcd.textCenter (juce::String (slot.label).substring (0, 5), xc, 52);

        if (auto* param = apvts.getParameter (slot.paramID))
            lcd.textCenter (lcdValue (*param).substring (0, 5), xc, 62);
    }
}

void DeviceView::drawEchoLcd (double now)
{
    constexpr int W = 96, x0 = 3;
    constexpr int lanes[] { 6, 16 };
    echo.clear();

    DelayViewInfo info;
    info.timeL    = proc.getGuiFeed().delayTimeL.load (std::memory_order_relaxed);
    info.timeR    = proc.getGuiFeed().delayTimeR.load (std::memory_order_relaxed);
    info.feedback = raw (pid::delayFeedback);
    info.mix      = raw (pid::delayMix);
    info.mode     = (int) raw (pid::delayMode);
    info.enabled  = raw (pid::delayEnabled) > 0.5f;
    info.freeze   = raw (pid::delayFreeze) > 0.5f;

    if (! info.enabled)
    {
        echo.textCenter ("DELAY OFF", W / 2, 8);
        return;
    }

    std::array<Echo, kMaxEchoes> echoes {};
    const int count = computeEchoes (info, echoes);
    const float window = echoWindowSeconds (echoes, count);
    const float elapsed = (float) (now - lastOnset);

    for (int y : lanes)
        for (int x = x0; x < W; x += 3)
            echo.set (x, y);

    if (elapsed >= 0.0f && elapsed < 0.15f)
        for (int y : lanes)
            echo.block (x0 - 1, y - 1, 3);

    for (int i = 0; i < count; ++i)
    {
        const auto& ec = echoes[(size_t) i];
        const int x = juce::roundToInt ((float) x0 + 4.0f + ec.time / window * (float) (W - x0 - 8));
        const int y = lanes[ec.channel];
        const bool lit = info.freeze || (elapsed >= ec.time && elapsed < ec.time + 0.32f);
        const int s = lit ? (ec.gain > 0.45f ? 3 : 2) : 1;
        echo.block (x - (s > 1 ? 1 : 0), y - (s > 1 ? 1 : 0), s);
    }

    if (elapsed >= 0.0f && elapsed < window)
        echo.vline (juce::roundToInt ((float) x0 + 4.0f + elapsed / window * (float) (W - x0 - 8)), 1, 20, true);
}

//==============================================================================
void DeviceView::stepSound (int delta)
{
    const auto& factory = presets::factoryPresets();
    const auto users = presets.getUserPresetFiles();
    const int total = (int) factory.size() + users.size();

    if (total == 0)
        return;

    const auto name = presets.getCurrentPresetName();
    int index = factoryIndexOf (name);

    if (index < 0)
        for (int i = 0; i < users.size(); ++i)
            if (users[i].getFileNameWithoutExtension() == name)
                index = (int) factory.size() + i;

    if (index < 0)
        index = delta > 0 ? -1 : 0;

    index = ((index + delta) % total + total) % total;

    if (index < (int) factory.size())
        presets.loadFactoryPreset (index);
    else
        presets.loadUserPreset (users[index - (int) factory.size()]);
}

void DeviceView::showSoundMenu()
{
    const auto& factory = presets::factoryPresets();
    const auto current = presets.getCurrentPresetName();

    juce::PopupMenu menu;
    juce::String category;

    for (size_t i = 0; i < factory.size(); ++i)
    {
        if (factory[i].category != category)
        {
            category = factory[i].category;
            menu.addSectionHeader (category);
        }

        menu.addItem ((int) i + 1, factory[i].name, true, factory[i].name == current);
    }

    const auto users = presets.getUserPresetFiles();

    if (! users.isEmpty())
    {
        menu.addSectionHeader ("User disks");
        for (int i = 0; i < users.size(); ++i)
            menu.addItem (kUserIdBase + i, users[i].getFileNameWithoutExtension(), true, users[i].getFileNameWithoutExtension() == current);
    }

    menu.addSeparator();
    menu.addItem (kSaveId, "Save this sound...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&disk),
                        [safe = juce::Component::SafePointer<DeviceView> (this), users] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result == kSaveId)
                                safe->saveSound();
                            else if (result >= kUserIdBase && result - kUserIdBase < users.size())
                                safe->presets.loadUserPreset (users[result - kUserIdBase]);
                            else if (result >= 1 && result <= (int) presets::factoryPresets().size())
                                safe->presets.loadFactoryPreset (result - 1);
                        });
}

void DeviceView::showSetupMenu()
{
    juce::PopupMenu finishes;
    for (int i = 0; i < kNumFinishes; ++i)
        finishes.addItem (100 + i, finish (i).name, true, proc.getUiFinish() == i);

    juce::PopupMenu sizes;
    for (int i = 0; i < 5; ++i)
        sizes.addItem (200 + i, juce::String (juce::roundToInt (kScales[i] * 100.0f)) + " %", true, std::abs (proc.getUiScale() - kScales[i]) < 0.01f);

    juce::PopupMenu menu;
    menu.addSectionHeader (utf8 ("STABR SP-92 \xc2\xb7 v") + JucePlugin_VersionString);
    menu.addSubMenu ("Finish", finishes);
    menu.addSubMenu ("Interface size", sizes);
    menu.addItem (300, "Write on the tape...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&power),
                        [safe = juce::Component::SafePointer<DeviceView> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result >= 100 && result < 100 + kNumFinishes)
                            {
                                if (safe->onFinishRequested) safe->onFinishRequested (result - 100);
                            }
                            else if (result >= 200 && result < 205)
                            {
                                if (safe->onScaleRequested) safe->onScaleRequested (kScales[result - 200]);
                            }
                            else if (result == 300)
                            {
                                safe->tape.showEditor();
                            }
                        });
}

void DeviceView::saveSound()
{
    auto* window = new juce::AlertWindow ("Save sound", "Name the disk.", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", presets.getCurrentPresetName(), "Name");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<DeviceView> safe (this);

    // El callback corre antes de que la ventana se destruya: 'window' es valido.
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        const auto saved = safe->presets.saveUserPreset (window->getTextEditorContents ("name"));

        if (saved.failed())
            juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon,
                                                                                   "Couldn't save the sound", saved.getErrorMessage()),
                                          nullptr);
    }), true);
}
}
