#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/MainView.h"
#include "GUI/Palette.h"
#include "GUI/Controls.h"
#include "GUI/StepGrid.h"
#include "GUI/DelayVisualizer.h"
#include "GUI/ChordVisualizer.h"
#include "Chord/ChordNaming.h"
#include "Chord/ChordGenerator.h"
#include "Common/GuiFeed.h"
#include "Params/ParameterIDs.h"
#include "FX/Delay.h"

#include <set>
#include <functional>
#include <vector>

namespace
{
using namespace stabr;

juce::String fullName (const chord::ChordResult& c)
{
    const auto n = chord::nameChord (c);

    if (! n.valid)
        return "?";

    juce::String s = juce::String (chord::keyNames[(size_t) n.root]) + n.quality;

    if (n.bass != n.root)
        s << "/" << chord::keyNames[(size_t) n.bass];

    return s;
}

chord::ChordParams literalParams (chord::ChordType type)
{
    chord::ChordParams p;
    p.type = type;
    p.lock = chord::ScaleLock::off;
    p.density = chord::kMaxDensity;
    p.inversion = 0;
    p.spread = 0;
    p.voicing = chord::VoicingType::closed;
    p.doubleRootBelow = false;
    return p;
}

//==============================================================================
class ChordNamingTests : public juce::UnitTest
{
public:
    ChordNamingTests() : juce::UnitTest ("ChordNaming", "GUI") {}

    void runTest() override
    {
        beginTest ("los 13 tipos sobre C se nombran bien");
        {
            using T = chord::ChordType;
            const std::vector<std::pair<T, const char*>> expected
            {
                { T::power, "C5" },   { T::majTriad, "C" },  { T::minTriad, "Cm" },  { T::sus2, "Csus2" },
                { T::sus4, "Csus4" }, { T::min7, "Cm7" },    { T::maj7, "Cmaj7" },   { T::min9, "Cm9" },
                { T::maj9, "Cmaj9" }, { T::min11, "Cm11" },  { T::sixNine, "C6/9" }, { T::add9, "Cadd9" },
                { T::dim7, "Cdim7" }
            };

            for (const auto& [type, expectedName] : expected)
                expectEquals (fullName (chord::generateChord (60, literalParams (type))), juce::String (expectedName));
        }

        beginTest ("DIATONICO en La menor: la tabla del README, detectada desde las notas");
        {
            auto p = literalParams (chord::ChordType::min7);
            p.lock = chord::ScaleLock::diatonic;
            p.keyRoot = 9;
            p.mode = chord::ScaleMode::naturalMinor;

            const std::vector<std::pair<int, const char*>> expected
            {
                { 57, "Am7" }, { 59, "Bm7b5" }, { 60, "Cmaj7" }, { 62, "Dm7" }, { 64, "Em7" }, { 65, "Fmaj7" }, { 67, "G7" }
            };

            for (const auto& [note, expectedName] : expected)
                expectEquals (fullName (chord::generateChord (note, p)), juce::String (expectedName));
        }

        beginTest ("una inversion sale con barra");
        {
            auto p = literalParams (chord::ChordType::min7);
            p.inversion = 1;
            expectEquals (fullName (chord::generateChord (57, p)), juce::String ("Am7/C"));
        }

        beginTest ("sin la quinta (D-022) el acorde conserva su nombre");
        {
            auto p = literalParams (chord::ChordType::min9);
            p.density = 4;
            expectEquals (fullName (chord::generateChord (60, p)), juce::String ("Cm9"));
        }

        beginTest ("12 fundamentales x inversiones x voicings: la fundamental detectada es la tocada");
        {
            // sus2/sus4 y dim7 quedan fuera a proposito: sus en inversion ES otro
            // sus (Csus2/D = Gsus4/D) y dim7 es simetrico. No hay nombre unico.
            using T = chord::ChordType;
            int checked = 0;

            for (auto type : { T::power, T::majTriad, T::minTriad, T::min7, T::maj7, T::min9, T::maj9, T::min11, T::sixNine, T::add9 })
                for (int root = 48; root < 60; ++root)
                    for (int inv = 0; inv < 4; ++inv)
                        for (int v = 0; v < (int) chord::VoicingType::numTypes; ++v)
                        {
                            auto p = literalParams (type);
                            p.inversion = inv;
                            p.voicing = (chord::VoicingType) v;

                            const auto notes = chord::generateChord (root, p);
                            const auto detected = chord::nameChord (notes);

                            expect (detected.valid && detected.root == root % 12,
                                    juce::String (chord::chordTypeNames[(size_t) type]) + " sobre " + juce::String (root)
                                        + " inv " + juce::String (inv) + " se nombro " + fullName (notes));
                            ++checked;
                        }

            logMessage ("acordes nombrados: " + juce::String (checked));
        }

        beginTest ("una nota sola no inventa un acorde");
        {
            chord::ChordResult single;
            single.notes[0] = 60;
            single.count = 1;

            expect (! chord::nameChord (single).valid);
            expectEquals (gui::ChordVisualizer::nameFor (single), juce::String ("C"));
        }
    }
};

//==============================================================================
struct FakePlayHead final : public juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = true;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setIsPlaying (playing);
        return info;
    }
};

void setPlain (StabrProcessor& proc, const char* id, float plain)
{
    auto* param = proc.getAPVTS().getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

class GuiFeedTests : public juce::UnitTest
{
public:
    GuiFeedTests() : juce::UnitTest ("GuiFeed", "GUI") {}

    void runTest() override
    {
        beginTest ("la cola llena descarta en vez de bloquear, y conserva el orden");
        {
            GuiFeed feed;
            int accepted = 0;

            for (int i = 0; i < GuiFeed::kCapacity * 3; ++i)
            {
                GuiEvent e;
                e.velocity = (float) i;
                accepted += feed.push (e) ? 1 : 0;
            }

            expectEquals (accepted, GuiFeed::kCapacity - 1);

            GuiEvent e;
            int expected = 0;
            bool ordered = true;

            while (feed.pop (e))
                ordered = ordered && (int) e.velocity == expected++;

            expect (ordered, "los eventos salieron desordenados");
            expectEquals (expected, GuiFeed::kCapacity - 1);
        }

        beginTest ("el procesador publica cada disparo y la liberacion");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);

            juce::AudioBuffer<float> buffer (2, 256);
            auto& feed = proc.getGuiFeed();
            GuiEvent e;

            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 57, 0.8f), 10);
            proc.processBlock (buffer, on);

            expect (feed.pop (e), "no se publico el disparo");
            expect (e.type == GuiEvent::Type::onset);
            expectWithinAbsoluteError (e.velocity, 0.8f, 0.01f);

            const auto sounding = proc.getSoundingNotes();
            expectEquals (e.notes.count, sounding.count);
            expectEquals (gui::ChordVisualizer::nameFor (e.notes), juce::String ("Am7"));

            juce::MidiBuffer empty;
            proc.processBlock (buffer, empty);
            expect (! feed.pop (e), "sin cambios no deberia publicar nada");

            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
            proc.processBlock (buffer, off);

            expect (feed.pop (e), "no se publico la liberacion");
            expect (e.type == GuiEvent::Type::notesChanged && e.notes.count == 0);
        }

        beginTest ("el secuenciador repitiendo el mismo acorde publica un disparo por golpe");
        {
            StabrProcessor proc;
            FakePlayHead ph;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);
            proc.setPlayHead (&ph);
            setPlain (proc, pid::seqEnabled, 1.0f);

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer empty;
            GuiEvent e;
            int onsets = 0;

            for (int pos = 0; pos < 192000; pos += 256)
            {
                proc.processBlock (buffer, empty);
                ph.ppq += 256 * 120.0 / 60.0 / 48000.0;

                while (proc.getGuiFeed().pop (e))
                    onsets += e.type == GuiEvent::Type::onset ? 1 : 0;
            }

            expectEquals (onsets, 8, "dos compases de contratiempos son 8 golpes");
        }
    }
};

//==============================================================================
class DelayVisualizerTests : public juce::UnitTest
{
public:
    DelayVisualizerTests() : juce::UnitTest ("DelayVisualizer", "GUI") {}

    void runTest() override
    {
        using gui::DelayVisualizer;
        std::array<gui::Echo, DelayVisualizer::kMaxEchoes> echoes {};

        beginTest ("ping pong: L, R, L... con los tiempos acumulados como en Delay.h");
        {
            gui::DelayViewInfo info;
            info.timeL = 0.3f;
            info.timeR = 0.4f;
            info.feedback = 0.5f;
            info.mode = (int) fx::DelayMode::pingPong;

            const int n = DelayVisualizer::computeEchoes (info, echoes);
            expectEquals (n, 6, "0.5^5 todavia se oye, 0.5^6 no");

            const float times[] { 0.3f, 0.7f, 1.0f, 1.4f, 1.7f, 2.1f };

            for (int i = 0; i < juce::jmin (n, 6); ++i)
            {
                expectWithinAbsoluteError (echoes[(size_t) i].time, times[i], 1.0e-4f);
                expectEquals (echoes[(size_t) i].channel, i % 2);
            }
        }

        beginTest ("estereo: cada canal con su propio tiempo (offset)");
        {
            gui::DelayViewInfo info;
            info.timeL = 0.3f;
            info.timeR = 0.39f;
            info.feedback = 0.5f;
            info.mode = (int) fx::DelayMode::stereo;

            const int n = DelayVisualizer::computeEchoes (info, echoes);
            expectEquals (n, 12);
            expectWithinAbsoluteError (echoes[2].time, 0.6f, 1.0e-4f);
            expectWithinAbsoluteError (echoes[3].time, 0.78f, 1.0e-4f);
            expectWithinAbsoluteError (echoes[2].gain, 0.5f, 1.0e-4f);
        }

        beginTest ("feedback 0 da una sola repeticion; freeze llena el visualizador sin desbordar");
        {
            gui::DelayViewInfo info;
            info.feedback = 0.0f;
            info.mode = (int) fx::DelayMode::pingPong;
            expectEquals (DelayVisualizer::computeEchoes (info, echoes), 1);

            info.mode = (int) fx::DelayMode::stereo;
            expectEquals (DelayVisualizer::computeEchoes (info, echoes), 2);

            info.freeze = true;
            expectEquals (DelayVisualizer::computeEchoes (info, echoes), DelayVisualizer::kMaxEchoes);
        }
    }
};

//==============================================================================
std::unique_ptr<StabrEditor> makeEditor (StabrProcessor& proc)
{
    return std::unique_ptr<StabrEditor> (dynamic_cast<StabrEditor*> (proc.createEditor()));
}

void walk (juce::Component& c, const std::function<void (juce::Component&)>& fn)
{
    fn (c);

    for (int i = 0; i < c.getNumChildComponents(); ++i)
        walk (*c.getChildComponent (i), fn);
}

bool isLayoutItem (juce::Component& c)
{
    return c.getProperties().contains (gui::paramIdProperty)
        || c.getProperties().contains (gui::stepParamIdsProperty)
        || dynamic_cast<gui::Panel*> (&c) != nullptr;
}

class EditorTests : public juce::UnitTest
{
public:
    EditorTests() : juce::UnitTest ("Editor", "GUI") {}

    void runTest() override
    {
        constexpr int W = gui::MainView::kBaseWidth;
        constexpr int H = gui::MainView::kBaseHeight;

        beginTest ("TODOS los parametros tienen un control en la interfaz");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);
            expect (editor != nullptr);

            if (editor == nullptr)
                return;

            std::set<juce::String> reachable;

            walk (*editor, [&] (juce::Component& c)
            {
                if (c.getProperties().contains (gui::paramIdProperty))
                    reachable.insert (c.getProperties()[gui::paramIdProperty].toString());

                if (c.getProperties().contains (gui::stepParamIdsProperty))
                    for (auto& id : juce::StringArray::fromTokens (c.getProperties()[gui::stepParamIdsProperty].toString(), ",", ""))
                        reachable.insert (id);
            });

            int total = 0;
            juce::StringArray missing;

            for (auto* param : proc.getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                {
                    ++total;
                    if (reachable.count (ranged->getParameterID()) == 0)
                        missing.add (ranged->getParameterID());
                }

            logMessage ("parametros con control: " + juce::String (total - missing.size()) + " de " + juce::String (total));
            expect (missing.isEmpty(), "sin control: " + missing.joinIntoString (", "));
        }

        beginTest ("ningun control se sale de su panel ni se pisa con otro, en ninguna pestana");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            int problems = 0, items = 0;

            walk (*editor, [&] (juce::Component& parent)
            {
                std::vector<juce::Component*> kids;

                for (int i = 0; i < parent.getNumChildComponents(); ++i)
                    if (isLayoutItem (*parent.getChildComponent (i)))
                        kids.push_back (parent.getChildComponent (i));

                for (size_t i = 0; i < kids.size(); ++i)
                {
                    ++items;
                    const auto b = kids[i]->getBounds();
                    const auto what = kids[i]->getTitle().isNotEmpty() ? kids[i]->getTitle()
                                                                       : kids[i]->getProperties()[gui::paramIdProperty].toString();

                    if (b.isEmpty() || ! parent.getLocalBounds().contains (b))
                    {
                        ++problems;
                        expect (false, "'" + what + "' fuera de su contenedor o vacio: " + b.toString()
                                           + " en " + parent.getLocalBounds().toString());
                    }

                    for (size_t j = i + 1; j < kids.size(); ++j)
                        if (b.intersects (kids[j]->getBounds()))
                        {
                            ++problems;
                            expect (false, "'" + what + "' se pisa con '" + kids[j]->getTitle() + "'");
                        }
                }
            });

            logMessage ("elementos de layout revisados: " + juce::String (items));
            expectEquals (problems, 0);
        }

        beginTest ("accesibilidad: todo control interactivo tiene nombre y se alcanza con teclado");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            int interactive = 0;
            juce::StringArray problems;

            walk (*editor, [&] (juce::Component& c)
            {
                const bool isInteractive = dynamic_cast<juce::Slider*> (&c) != nullptr
                                        || dynamic_cast<juce::ComboBox*> (&c) != nullptr
                                        || dynamic_cast<juce::Button*> (&c) != nullptr
                                        || c.getProperties().contains (gui::stepParamIdsProperty);

                if (! isInteractive)
                    return;

                ++interactive;

                if (c.getTitle().isEmpty())
                    problems.add ("sin nombre accesible: " + c.getName());

                if (! c.getWantsKeyboardFocus())
                    problems.add ("no alcanzable con teclado: " + c.getTitle());
            });

            /*  Cuenta exacta, no un umbral: un parametro por control, salvo los
                64 de los pasos que viven en 16 celdas; mas las pestanas y el
                selector de tamano. Si aparece o desaparece un control, se nota. */
            const int stepParams = 4 * seq::kNumSteps;
            const int expectedInteractive = (proc.getParameters().size() - stepParams)
                                          + seq::kNumSteps + editor->getView().getNumPages() + 1;

            logMessage ("controles interactivos: " + juce::String (interactive));
            expectEquals (interactive, expectedInteractive);
            expect (problems.isEmpty(), problems.joinIntoString ("; "));
        }

        beginTest ("contraste de la paleta (WCAG): texto >= 4.5, acento >= 3");
        {
            using namespace gui::colours;

            for (auto ground : { background, panel, raised })
            {
                expect (gui::contrastRatio (text, ground) >= 4.5, "texto sobre fondo: " + juce::String (gui::contrastRatio (text, ground), 2));
                expect (gui::contrastRatio (textMuted, ground) >= 4.5, "texto secundario: " + juce::String (gui::contrastRatio (textMuted, ground), 2));
                expect (gui::contrastRatio (accent, ground) >= 3.0, "acento: " + juce::String (gui::contrastRatio (accent, ground), 2));
            }

            // Texto oscuro sobre el acento (item resaltado de los menus).
            expect (gui::contrastRatio (background, accent) >= 4.5);

            logMessage ("contraste texto secundario sobre panel: " + juce::String (gui::contrastRatio (textMuted, panel), 2)
                        + ", acento sobre fondo: " + juce::String (gui::contrastRatio (accent, background), 2));
        }

        beginTest ("escala de 0.75x a 2x, y se guarda con el proyecto");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            for (float s : { 0.75f, 1.0f, 1.5f, 2.0f })
            {
                editor->setUiScale (s);
                expectWithinAbsoluteError (editor->getWidth(), juce::roundToInt (W * s), 1);
                expectWithinAbsoluteError (editor->getHeight(), juce::roundToInt (H * s), 1);
                expectWithinAbsoluteError (editor->getView().getTransform().mat00, s, 0.001f);
            }

            // Fuera de rango se limita, no se rompe.
            editor->setUiScale (5.0f);
            expectWithinAbsoluteError (editor->getWidth(), W * 2, 1);

            editor->setUiScale (1.5f);

            juce::MemoryBlock blob;
            proc.getStateInformation (blob);
            editor.reset();

            StabrProcessor other;
            other.setStateInformation (blob.getData(), (int) blob.getSize());
            auto reopened = makeEditor (other);

            if (reopened != nullptr)
                expectWithinAbsoluteError (reopened->getWidth(), juce::roundToInt (W * 1.5f), 1);
        }

        beginTest ("la grilla: editar un patron de fabrica lo copia a Custom sin perder nada");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            auto& grid = editor->getView().getStepGrid();
            expect (! grid.isCustomPattern());   // default: off-beat de fabrica

            grid.toggleStep (0);

            expect (grid.isCustomPattern(), "editar no paso a Custom");
            expect (grid.getDisplayedStep (0).on, "el paso editado no quedo encendido");

            for (int i : { 2, 6, 10, 14 })
                expect (grid.getDisplayedStep (i).on, "se perdio el paso " + juce::String (i + 1) + " del patron de fabrica");

            grid.setRatchet (6, 3);
            grid.setProbability (6, 0.5f);
            expectEquals (grid.getDisplayedStep (6).ratchet, 3);
            expectWithinAbsoluteError (grid.getDisplayedStep (6).probability, 0.5f, 0.01f);
        }

        beginTest ("costo de dibujar la interfaz completa, con visualizadores animando");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 57, 0.9f), 0);
            proc.processBlock (buffer, on);

            for (float s : { 1.0f, 2.0f })
            {
                editor->setUiScale (s);

                const double t0 = juce::Time::getMillisecondCounterHiRes() * 0.001;
                editor->getView().tick (t0);
                expectEquals (editor->getView().getChordVisualizer().getDisplayedName(), juce::String ("Am7"));

                constexpr int frames = 5;
                const double start = juce::Time::getMillisecondCounterHiRes();

                for (int i = 0; i < frames; ++i)
                {
                    editor->getView().tick (t0 + 0.05 * i);
                    auto image = editor->createComponentSnapshot (editor->getLocalBounds());
                    expect (image.isValid());
                }

                const double ms = (juce::Time::getMillisecondCounterHiRes() - start) / frames;
                logMessage ("cuadro completo a " + juce::String (s, 2) + "x (" + juce::String (editor->getWidth()) + "x"
                            + juce::String (editor->getHeight()) + "): " + juce::String (ms, 1) + " ms, render por software");

                // El render por software es el peor caso; en pantalla solo se
                // repintan los visualizadores, no la interfaz entera.
                expect (ms < 250.0, "dibujar la interfaz completa tarda " + juce::String (ms, 1) + " ms");
            }

            /*  Volcado opcional a PNG para revisar la interfaz a ojo: que el
                layout no se pise no dice si se ve bien. Solo con la variable
                STABR_SNAPSHOT_DIR definida; en una corrida normal no escribe. */
            const auto dir = juce::SystemStats::getEnvironmentVariable ("STABR_SNAPSHOT_DIR", {});

            if (dir.isNotEmpty())
            {
                editor->setUiScale (1.0f);

                /*  Un segundo de audio antes de capturar. Recien preparado, el
                    delay en modo cinta todavia esta deslizando desde 0 hacia su
                    tiempo: la primera version de este volcado capturo 19.5 ms
                    en vez de 375 ms y parecia un bug de layout del visualizador. */
                juce::MidiBuffer silence;
                for (int i = 0; i < 200; ++i)
                    proc.processBlock (buffer, silence);

                const double t0 = juce::Time::getMillisecondCounterHiRes() * 0.001;

                proc.processBlock (buffer, on);
                editor->getView().tick (t0);
                editor->getView().tick (t0 + 0.9);   // repeticiones del delay ya encendidas

                // Datos reales del visualizador de delay, para diagnosticar el layout.
                const auto& dv = editor->getView().getDelayVisualizer();
                juce::String echoTimes;
                for (int i = 0; i < dv.getNumEchoes(); ++i)
                    echoTimes << juce::String (dv.getEcho (i).time, 3) << (dv.getEcho (i).channel == 0 ? "L " : "R ");

                logMessage ("delay viz: timeL " + juce::String (dv.getInfo().timeL, 4) + " s, timeR " + juce::String (dv.getInfo().timeR, 4)
                            + " s, fb " + juce::String (dv.getInfo().feedback, 2) + ", ventana " + juce::String (dv.getWindowSeconds(), 3)
                            + " s, ecos: " + echoTimes);

                for (int page = 0; page < editor->getView().getNumPages(); ++page)
                {
                    editor->getView().showPage (page);
                    auto image = editor->createComponentSnapshot (editor->getLocalBounds());
                    juce::File file (juce::File (dir).getChildFile ("stabr_page_" + juce::String (page) + ".png"));
                    file.deleteFile();
                    juce::FileOutputStream stream (file);
                    juce::PNGImageFormat().writeImageToStream (image, stream);
                    logMessage ("snapshot: " + file.getFullPathName());
                }
            }
        }
    }
};

static ChordNamingTests chordNamingTests;
static GuiFeedTests guiFeedTests;
static DelayVisualizerTests delayVisualizerTests;
static EditorTests editorTests;
}
