#include "MainView.h"
#include "Palette.h"
#include "../PluginProcessor.h"
#include "../Params/ParameterIDs.h"
#include "../Common/Utf8.h"
#include "../Chord/ChordTypes.h"
#include "../FX/Delay.h"
#include "../FX/TempoSync.h"

namespace stabr::gui
{
namespace
{
    constexpr int kMargin = 10;
    constexpr int kGap = 8;
    constexpr int kHeaderHeight = 52;
    constexpr int kHeroHeight = 272;
    constexpr int kTabHeight = 30;
    constexpr int kSeqHeight = 100;

    constexpr const char* tabNames[] { "Chord", "Osc", "Filter", "Delay", "Reverb", "FX", "Pump / Out" };

    const juce::String separator = utf8 ("  \xc2\xb7  ");

    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;
    using Px = juce::Grid::Px;
}

//==============================================================================
MainView::MainView (StabrProcessor& p)
    : proc (p), apvts (p.getAPVTS()),
      chordPanel ("Chord"), tonePanel ("Filter / Amp"), delayPanel ("Delay"),
      stepGrid (p.getAPVTS())
{
    setOpaque (true);

    //--------------------------------------------------------------------------
    // Cabecera
    //--------------------------------------------------------------------------
    addAndMakeVisible (header);

    logo.setText ("STABR", juce::dontSendNotification);
    logo.setFont (fonts::title (24.0f));
    logo.setColour (juce::Label::textColourId, colours::text);
    header.addAndMakeVisible (logo);

    subtitle.setText (utf8 ("by Narfbach") + separator + "v" + JucePlugin_VersionString, juce::dontSendNotification);
    subtitle.setFont (fonts::label (12.0f));
    subtitle.setColour (juce::Label::textColourId, colours::textMuted);
    header.addAndMakeVisible (subtitle);

    status.setFont (fonts::value (12.0f));
    status.setJustificationType (juce::Justification::centred);
    status.setColour (juce::Label::textColourId, colours::textMuted);
    header.addAndMakeVisible (status);

    sizeBox.setTitle ("Interface size");
    int id = 1;
    for (int pct : { 75, 100, 125, 150, 200 })
        sizeBox.addItem (juce::String (pct) + " %", id++);
    sizeBox.onChange = [this]
    {
        static constexpr float scales[] { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
        const int index = sizeBox.getSelectedId() - 1;

        if (index >= 0 && index < 5 && onScaleRequested)
            onScaleRequested (scales[index]);
    };
    header.addAndMakeVisible (sizeBox);
    header.addAndMakeVisible (meter);

    //--------------------------------------------------------------------------
    // Fila principal
    //--------------------------------------------------------------------------
    addAndMakeVisible (chordPanel);
    addAndMakeVisible (tonePanel);
    addAndMakeVisible (delayPanel);

    chordType = control (chordPanel, { pid::chordType, "Chord Type", ControlSize::large });
    scaleKey  = control (chordPanel, { pid::scaleKey,  "Key",        ControlSize::medium });
    scaleMode = control (chordPanel, { pid::scaleMode, "Scale",      ControlSize::medium });
    chordPanel.addAndMakeVisible (chordViz);

    cutoff    = control (tonePanel, { pid::filterCutoff,    "Cutoff",    ControlSize::large });
    resonance = control (tonePanel, { pid::filterResonance, "Resonance", ControlSize::large });
    ampDecay  = control (tonePanel, { pid::ampDecay,        "Decay",     ControlSize::large });
    envAmount = control (tonePanel, { pid::filterEnvAmount, "Env Amount", ControlSize::medium });
    envDecay  = control (tonePanel, { pid::filtEnvDecay,    "Env Decay", ControlSize::medium });
    drive     = control (tonePanel, { pid::driveAmount,     "Drive",     ControlSize::medium });

    delayPanel.addAndMakeVisible (delayViz);
    delayTime     = control (delayPanel, { pid::delayDivision, "Time",     ControlSize::small });
    delayMode     = control (delayPanel, { pid::delayMode,     "Mode",     ControlSize::small });
    delayFeedback = control (delayPanel, { pid::delayFeedback, "Feedback", ControlSize::medium });
    delayMix      = control (delayPanel, { pid::delayMix,      "Mix",      ControlSize::medium });

    //--------------------------------------------------------------------------
    // Pestanas
    //--------------------------------------------------------------------------
    buildPages();

    for (int i = 0; i < (int) std::size (tabNames); ++i)
    {
        auto* b = tabs.add (new juce::TextButton (tabNames[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->getProperties().set ("tab", true);
        b->setTitle (juce::String (tabNames[i]) + " page");
        b->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (b);
    }

    //--------------------------------------------------------------------------
    // Secuenciador
    //--------------------------------------------------------------------------
    addAndMakeVisible (seqPanel);
    seqOn      = control (seqPanel, { pid::seqEnabled, "Seq On" });
    seqPattern = control (seqPanel, { pid::seqPattern, "Pattern" });
    seqRate    = control (seqPanel, { pid::seqRate,    "Rate" });
    seqLength  = control (seqPanel, { pid::seqSteps,   "Length" });
    seqSwing   = control (seqPanel, { pid::seqSwing,   "Swing" });
    seqGate    = control (seqPanel, { pid::seqGate,    "Gate" });
    seqPanel.addAndMakeVisible (stepGrid);

    showPage (0);
    refreshStatus();
    refreshChordDetail();
    refreshDelayInfo();
}

MainView::~MainView() = default;

std::unique_ptr<juce::Component> MainView::control (Panel& parent, const ControlSpec& spec)
{
    auto c = makeControl (apvts, spec);
    parent.addAndMakeVisible (*c);
    return c;
}

std::unique_ptr<ParamPanel> MainView::panel (const char* title, std::initializer_list<ControlSpec> specs)
{
    return std::make_unique<ParamPanel> (apvts, juce::String (title), specs);
}

float MainView::raw (const char* paramID) const
{
    return apvts.getRawParameterValue (paramID)->load();
}

//==============================================================================
void MainView::buildPages()
{
    auto add = [this] (std::unique_ptr<ParamPage> page)
    {
        addChildComponent (*page);
        pages.push_back (std::move (page));
    };

    {
        auto page = std::make_unique<ParamPage>();
        page->addPanel (panel ("Voicing", { { pid::chordVoicing, "Voicing" }, { pid::chordInversion, "Inversion" },
                                            { pid::chordSpread, "Spread" }, { pid::chordDensity, "Density" },
                                            { pid::chordOctaveDouble, "Octave Down" } }));
        page->addPanel (panel ("Scale", { { pid::scaleLock, "Scale Lock" }, { pid::noteMode, "Note Mode" } }));
        page->addPanel (panel ("Play", { { pid::polyphony, "Polyphony" }, { pid::pitchBendRange, "Bend" } }));
        add (std::move (page));
    }
    {
        auto page = std::make_unique<ParamPage>();
        page->addPanel (panel ("Osc 1", { { pid::osc1Wave, "Wave" }, { pid::osc1Level, "Level" }, { pid::pulseWidth, "PW" } }));
        page->addPanel (panel ("Osc 2", { { pid::osc2Wave, "Wave" }, { pid::osc2Level, "Level" },
                                          { pid::osc2Detune, "Detune" }, { pid::osc2Octave, "Octave" } }));
        page->addPanel (panel ("Sub", { { pid::subWave, "Wave" }, { pid::subLevel, "Level" }, { pid::subOctave, "Octave" } }));
        page->addPanel (panel ("Unison", { { pid::unisonCount, "Voices" }, { pid::unisonDetune, "Detune" },
                                           { pid::unisonWidth, "Width" }, { pid::unisonRandomPhase, "Rnd Phase" } }));
        add (std::move (page));
    }
    {
        auto page = std::make_unique<ParamPage>();
        page->addPanel (panel ("Filter", { { pid::filterType, "Type" }, { pid::filterSlope, "Slope" },
                                           { pid::filterKeyTrack, "Key Track" }, { pid::filterVelAmount, "Velocity" },
                                           { pid::filterPeakComp, "Reso Tame" } }));
        page->addPanel (panel ("Amp Env", { { pid::ampAttack, "Attack" }, { pid::ampSustain, "Sustain" }, { pid::ampRelease, "Release" } }));
        page->addPanel (panel ("Filter Env", { { pid::filtEnvAttack, "Attack" }, { pid::filtEnvSustain, "Sustain" },
                                               { pid::filtEnvRelease, "Release" } }));
        page->addPanel (panel ("Engine", { { pid::hqMode, "HQ Mode" }, { pid::masterGain, "Master" } }));
        add (std::move (page));
    }
    {
        auto page = std::make_unique<ParamPage>();
        page->addPanel (panel ("Delay", { { pid::delayEnabled, "Delay On" }, { pid::delaySync, "Sync" }, { pid::delayFreeze, "Freeze" } }));
        page->addPanel (panel ("Time", { { pid::delayFreeMs, "Time ms" }, { pid::delayOffset, "Offset R" }, { pid::delayGlide, "Time Change" } }));
        page->addPanel (panel ("Loop", { { pid::delayHighpass, "HP" }, { pid::delayLowpass, "LP" },
                                         { pid::delayModDepth, "Wow" }, { pid::delayDucking, "Duck" } }));
        add (std::move (page));
    }
    {
        auto page = std::make_unique<ParamPage>();
        page->addPanel (panel ("Reverb", { { pid::reverbEnabled, "Reverb On" }, { pid::reverbMode, "Mode" },
                                           { pid::reverbSize, "Size" }, { pid::reverbDecay, "Decay" },
                                           { pid::reverbDamping, "Damping" }, { pid::reverbModDepth, "Mod" },
                                           { pid::reverbLowCut, "Low Cut" }, { pid::reverbDucking, "Duck" },
                                           { pid::reverbMix, "Mix" } }));
        page->addPanel (panel ("Pre-Delay", { { pid::reverbPreDelaySync, "Sync" }, { pid::reverbPreDelayDiv, "Division" },
                                              { pid::reverbPreDelayMs, "Time" } }));
        add (std::move (page));
    }
    {
        auto page = std::make_unique<ParamPage>();
        page->addPanel (panel ("Order", { { pid::fxOrder, "Order" } }));
        page->addPanel (panel ("Saturation", { { pid::satEnabled, "Sat On" }, { pid::satType, "Type" },
                                               { pid::satDrive, "Drive" }, { pid::satBits, "Bits" },
                                               { pid::satRateReduce, "Rate" }, { pid::satMix, "Mix" } }));
        page->addPanel (panel ("Chorus", { { pid::chorusEnabled, "Chorus On" }, { pid::chorusEnsemble, "Ensemble" },
                                           { pid::chorusRate, "Rate" }, { pid::chorusDepth, "Depth" },
                                           { pid::chorusWidth, "Width" }, { pid::chorusMix, "Mix" } }));
        add (std::move (page));
    }
    {
        auto page = std::make_unique<ParamPage>();
        page->addPanel (panel ("Pump", { { pid::pumpEnabled, "Pump On" }, { pid::pumpDivision, "Rate" },
                                         { pid::pumpDepth, "Depth" }, { pid::pumpShape, "Shape" }, { pid::pumpAttack, "Attack" } }));
        page->addPanel (panel ("Output", { { pid::outputWidth, "Width" }, { pid::outputMonoBelow, "Mono Below" },
                                           { pid::outputLimiter, "Limiter" }, { pid::outputGain, "Output" } }));
        add (std::move (page));
    }
}

void MainView::showPage (int index)
{
    for (int i = 0; i < (int) pages.size(); ++i)
    {
        pages[(size_t) i]->setVisible (i == index);

        if (i < tabs.size())
            tabs[i]->setToggleState (i == index, juce::dontSendNotification);
    }
}

void MainView::setScaleIndicator (float scale)
{
    static constexpr float scales[] { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
    int best = 0;

    for (int i = 1; i < 5; ++i)
        if (std::abs (scales[i] - scale) < std::abs (scales[best] - scale))
            best = i;

    // Solo marca el tamano exacto; un tamano arrastrado a mano no es ninguno.
    sizeBox.setSelectedId (std::abs (scales[best] - scale) < 0.01f ? best + 1 : 0, juce::dontSendNotification);
}

//==============================================================================
void MainView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (kMargin);

    header.setBounds (area.removeFromTop (kHeaderHeight));
    area.removeFromTop (kGap);

    const auto hero = area.removeFromTop (kHeroHeight);
    area.removeFromTop (kGap);

    seqPanel.setBounds (area.removeFromBottom (kSeqHeight));
    area.removeFromBottom (kGap);

    const auto tabRow = area.removeFromTop (kTabHeight);

    for (auto& page : pages)
        page->setBounds (area);

    //--------------------------------------------------------------------------
    // Cabecera
    //--------------------------------------------------------------------------
    {
        juce::FlexBox fb;
        fb.alignItems = juce::FlexBox::AlignItems::center;
        fb.items.add (juce::FlexItem (logo).withWidth (100.0f).withHeight (32.0f));
        fb.items.add (juce::FlexItem (subtitle).withWidth (190.0f).withHeight (20.0f));
        fb.items.add (juce::FlexItem (status).withFlex (1.0f).withHeight (20.0f));
        fb.items.add (juce::FlexItem (sizeBox).withWidth (84.0f).withHeight (26.0f).withMargin (juce::FlexItem::Margin (0.0f, 12.0f, 0.0f, 0.0f)));
        fb.items.add (juce::FlexItem (meter).withWidth (150.0f).withHeight (22.0f));
        fb.performLayout (header.getContentBounds());
    }

    //--------------------------------------------------------------------------
    // Fila principal
    //--------------------------------------------------------------------------
    {
        juce::Grid grid;
        grid.templateRows = { Track (Fr (1)) };
        grid.templateColumns = { Track (Fr (30)), Track (Fr (36)), Track (Fr (32)) };
        grid.columnGap = Px (kGap);
        grid.items = { juce::GridItem (chordPanel), juce::GridItem (tonePanel), juce::GridItem (delayPanel) };
        grid.performLayout (hero);
    }

    {
        auto c = chordPanel.getContentBounds();

        juce::Grid grid;
        grid.templateRows = { Track (Fr (1)) };
        grid.templateColumns = { Track (Fr (5)), Track (Fr (2)), Track (Fr (4)) };
        grid.columnGap = Px (6);
        grid.items = { juce::GridItem (*chordType), juce::GridItem (*scaleKey), juce::GridItem (*scaleMode) };
        grid.performLayout (c.removeFromTop (62));

        c.removeFromTop (6);
        chordViz.setBounds (c);
    }

    {
        juce::Grid grid;
        grid.templateColumns = { Track (Fr (1)), Track (Fr (1)), Track (Fr (1)) };
        grid.templateRows = { Track (Px (preferredSize (*cutoff).y)), Track (Fr (1)) };
        grid.items = { juce::GridItem (*cutoff), juce::GridItem (*resonance), juce::GridItem (*ampDecay),
                       juce::GridItem (*envAmount), juce::GridItem (*envDecay), juce::GridItem (*drive) };
        grid.performLayout (tonePanel.getContentBounds());
    }

    {
        auto d = delayPanel.getContentBounds();
        auto controls = d.removeFromBottom (preferredSize (*delayFeedback).y);
        d.removeFromBottom (4);
        delayViz.setBounds (d);

        auto selectors = controls.removeFromLeft (controls.getWidth() * 4 / 10);
        delayTime->setBounds (selectors.removeFromTop (selectors.getHeight() / 2).reduced (0, 2));
        delayMode->setBounds (selectors.reduced (0, 2));
        delayFeedback->setBounds (controls.removeFromLeft (controls.getWidth() / 2));
        delayMix->setBounds (controls);
    }

    //--------------------------------------------------------------------------
    // Pestanas
    //--------------------------------------------------------------------------
    {
        juce::FlexBox fb;
        for (auto* t : tabs)
            fb.items.add (juce::FlexItem (*t).withWidth (112.0f).withHeight ((float) tabRow.getHeight())
                                             .withMargin (juce::FlexItem::Margin (0.0f, 4.0f, 0.0f, 0.0f)));
        fb.performLayout (tabRow);
    }

    //--------------------------------------------------------------------------
    // Secuenciador
    //--------------------------------------------------------------------------
    {
        auto s = seqPanel.getContentBounds();

        auto column = s.removeFromLeft (preferredSize (*seqPattern).x);
        seqOn->setBounds (column.removeFromTop (preferredSize (*seqOn).y));
        column.removeFromTop (4);
        seqPattern->setBounds (column.withHeight (juce::jmin (column.getHeight(), preferredSize (*seqPattern).y)));

        s.removeFromLeft (kGap);
        const auto rateSize = preferredSize (*seqRate);
        seqRate->setBounds (s.removeFromLeft (96).withSizeKeepingCentre (96, rateSize.y));

        for (auto* k : { seqLength.get(), seqSwing.get(), seqGate.get() })
        {
            s.removeFromLeft (kGap);
            k->setBounds (s.removeFromLeft (preferredSize (*k).x));
        }

        s.removeFromLeft (kGap * 2);
        stepGrid.setBounds (s);
    }
}

//==============================================================================
void MainView::tick (double now)
{
    auto& feed = proc.getGuiFeed();
    GuiEvent e;

    while (feed.pop (e))
    {
        if (e.type == GuiEvent::Type::onset)
        {
            chordViz.showNotes (e.notes, true, e.velocity, now);
            delayViz.onset (e.velocity, now);
        }
        else
        {
            chordViz.showNotes (e.notes, false, 0.0f, now);
        }
    }

    refreshChordDetail();
    chordViz.tick (now);

    refreshDelayInfo();
    delayViz.tick (now);

    stepGrid.tick (proc.getSequencerStep());

    float peakL = 0.0f, peakR = 0.0f, reduction = 0.0f;
    proc.getFxChain().getOutput().readAndClearMeters (peakL, peakR, reduction);
    meter.push (peakL, peakR);

    // Sin sync, la division no hace nada: se apaga para que se note.
    const bool sync = raw (pid::delaySync) > 0.5f;
    if (delayTime->isEnabled() != sync)
        delayTime->setEnabled (sync);

    refreshStatus();
}

void MainView::refreshChordDetail()
{
    const std::array<int, 3> key { (int) raw (pid::chordVoicing), (int) raw (pid::chordInversion), (int) raw (pid::chordDensity) };

    if (key == chordDetailKey)
        return;

    chordDetailKey = key;

    const auto voicing = (size_t) juce::jlimit (0, (int) chord::VoicingType::numTypes - 1, key[0]);
    chordViz.setDetail (juce::String (chord::voicingNames[voicing]) + separator + "inv " + juce::String (key[1])
                        + separator + juce::String (key[2]) + " voices");
}

void MainView::refreshDelayInfo()
{
    auto& feed = proc.getGuiFeed();

    DelayViewInfo info;
    info.timeL    = feed.delayTimeL.load (std::memory_order_relaxed);
    info.timeR    = feed.delayTimeR.load (std::memory_order_relaxed);
    info.feedback = raw (pid::delayFeedback);
    info.mix      = raw (pid::delayMix);
    info.mode     = (int) raw (pid::delayMode);
    info.enabled  = raw (pid::delayEnabled) > 0.5f;
    info.freeze   = raw (pid::delayFreeze) > 0.5f;

    const bool sync = raw (pid::delaySync) > 0.5f;
    const std::array<int, 8> key { info.mode, sync ? 1 : 0, (int) raw (pid::delayDivision), juce::roundToInt (raw (pid::delayFreeMs)),
                                   juce::roundToInt (info.feedback * 100.0f), juce::roundToInt (info.mix * 100.0f),
                                   info.freeze ? 1 : 0, info.enabled ? 1 : 0 };

    if (key != delayCaptionKey)
    {
        delayCaptionKey = key;

        const auto mode = (size_t) juce::jlimit (0, (int) fx::DelayMode::numModes - 1, info.mode);
        const auto division = (size_t) juce::jlimit (0, (int) fx::Division::numDivisions - 1, key[2]);

        delayCaption = juce::String (fx::delayModeNames[mode]) + separator
                     + (sync ? juce::String (fx::divisionNames[division]) : juce::String (key[3]) + " ms")
                     + separator + "FB " + juce::String (key[4]) + "%"
                     + separator + "MIX " + juce::String (key[5]) + "%"
                     + (info.freeze ? separator + "FREEZE" : juce::String());
    }

    delayViz.setInfo (info, delayCaption);
}

void MainView::refreshStatus()
{
    const std::array<int, 3> key { (int) raw (pid::scaleKey), (int) raw (pid::scaleMode),
                                   juce::roundToInt (proc.getGuiFeed().bpm.load (std::memory_order_relaxed)) };

    if (key == statusKey)
        return;

    statusKey = key;

    const auto root = (size_t) juce::jlimit (0, 11, key[0]);
    const auto mode = (size_t) juce::jlimit (0, (int) chord::ScaleMode::numModes - 1, key[1]);

    status.setText (juce::String (chord::keyNames[root]) + " " + chord::scaleModeNames[mode]
                        + separator + juce::String (key[2]) + " BPM",
                    juce::dontSendNotification);
}
}
