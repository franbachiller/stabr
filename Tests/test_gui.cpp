#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/SP92/DeviceView.h"
#include "GUI/SP92/Finish.h"
#include "GUI/SP92/LcdPages.h"
#include "GUI/SP92/DotMatrix.h"
#include "GUI/SP92/LcdDrawing.h"
#include "GUI/SP92/Anim.h"
#include "GUI/SP92/ButtonMotion.h"
#include "GUI/SP92/Hardware.h"
#include "GUI/SP92/HardwareKnob.h"
#include "GUI/SP92/Sp92Fonts.h"
#include "GUI/Palette.h"
#include "GUI/StepGrid.h"
#include "GUI/EchoModel.h"
#include "GUI/ChordText.h"
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

        beginTest ("Density 2 nombra la diada en vez de mostrar notas sueltas");
        {
            auto p = literalParams (chord::ChordType::min9);
            p.lock = chord::ScaleLock::diatonic;
            p.keyRoot = 6;
            p.mode = chord::ScaleMode::phrygian;
            p.density = 2;

            const auto notes = chord::generateChord (64, p);
            expectEquals (notes.count, 2);
            expectEquals (fullName (notes), juce::String ("Em(no5)"));

            p.lock = chord::ScaleLock::off;
            p.type = chord::ChordType::maj9;
            expectEquals (fullName (chord::generateChord (60, p)), juce::String ("C(no5)"));
        }

        beginTest ("una nota sola no inventa un acorde");
        {
            chord::ChordResult single;
            single.notes[0] = 60;
            single.count = 1;

            expect (! chord::nameChord (single).valid);
            expectEquals (gui::chordNameFor (single), juce::String ("C"));
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
            expectEquals (e.notes.count, proc.getSoundingNotes().count);
            expectEquals (gui::chordNameFor (e.notes), juce::String ("Am7"));

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
class EchoModelTests : public juce::UnitTest
{
public:
    EchoModelTests() : juce::UnitTest ("EchoModel", "GUI") {}

    void runTest() override
    {
        std::array<gui::Echo, gui::kMaxEchoes> echoes {};

        beginTest ("ping pong: L, R, L... con los tiempos acumulados como en Delay.h");
        {
            gui::DelayViewInfo info;
            info.timeL = 0.3f;
            info.timeR = 0.4f;
            info.feedback = 0.5f;
            info.mode = (int) fx::DelayMode::pingPong;

            const int n = gui::computeEchoes (info, echoes);
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

            const int n = gui::computeEchoes (info, echoes);
            expectEquals (n, 12);
            expectWithinAbsoluteError (echoes[2].time, 0.6f, 1.0e-4f);
            expectWithinAbsoluteError (echoes[3].time, 0.78f, 1.0e-4f);
            expectWithinAbsoluteError (echoes[2].gain, 0.5f, 1.0e-4f);
        }

        beginTest ("feedback 0 da una sola repeticion; freeze llena la pantalla sin desbordar");
        {
            gui::DelayViewInfo info;
            info.feedback = 0.0f;
            info.mode = (int) fx::DelayMode::pingPong;
            expectEquals (gui::computeEchoes (info, echoes), 1);

            info.mode = (int) fx::DelayMode::stereo;
            expectEquals (gui::computeEchoes (info, echoes), 2);

            info.freeze = true;
            expectEquals (gui::computeEchoes (info, echoes), gui::kMaxEchoes);
        }
    }
};

//==============================================================================
class LcdTests : public juce::UnitTest
{
public:
    LcdTests() : juce::UnitTest ("SP-92 LCD", "GUI") {}

    void runTest() override
    {
        beginTest ("la fuente 5x7 tiene todo lo que escribe la pantalla");
        {
            const juce::String needed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .-/#:%>+()'abdeijmnosu";

            for (auto p = needed.getCharPointer(); ! p.isEmpty(); ++p)
                expect (gui::sp92::dotGlyph (*p) != nullptr, juce::String ("falta el glifo ") + juce::String::charToString (*p));

            gui::sp92::DotMatrix m (20, 8);
            m.text ("A", 0, 0);
            expect (m.isLit (1, 0) && ! m.isLit (0, 0), "la A no quedo dibujada donde corresponde");
        }

        beginTest ("ningun valor ni etiqueta del LCD pasa de 5 caracteres");
        {
            StabrProcessor proc;
            int values = 0;

            for (const auto& pg : gui::sp92::lcdPages())
                for (const auto& sub : pg.subPages)
                    for (const auto& slot : sub)
                    {
                        if (slot.paramID == nullptr)
                            continue;

                        expect (juce::String (slot.label).length() <= 5, juce::String ("etiqueta larga: ") + slot.label);

                        auto* param = proc.getAPVTS().getParameter (slot.paramID);
                        expect (param != nullptr, juce::String ("ID desconocido en una pagina: ") + slot.paramID);

                        if (param == nullptr)
                            continue;

                        // Se barre el recorrido entero, no solo el default.
                        for (int step = 0; step <= 10; ++step)
                        {
                            param->setValueNotifyingHost ((float) step / 10.0f);
                            const auto text = gui::sp92::lcdValue (*param);
                            ++values;

                            if (text.length() > 5)
                                expect (false, juce::String (slot.paramID) + " muestra \"" + text + "\"");

                            for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
                                if (gui::sp92::dotGlyph (*p) == nullptr)
                                    expect (false, juce::String (slot.paramID) + " usa un caracter sin glifo: " + text);
                        }
                    }

            logMessage ("valores de LCD revisados: " + juce::String (values));
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

class EditorTests : public juce::UnitTest
{
public:
    EditorTests() : juce::UnitTest ("Editor SP-92", "GUI") {}

    void runTest() override
    {
        using gui::sp92::DeviceView;
        constexpr int W = DeviceView::kBaseWidth;
        constexpr int H = DeviceView::kBaseHeight;

        beginTest ("TODOS los parametros se pueden tocar desde el panel");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);
            expect (editor != nullptr);

            if (editor == nullptr)
                return;

            const auto reachable = editor->getView().getControlledParameterIDs();
            juce::StringArray missing;
            int total = 0;

            for (auto* param : proc.getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                {
                    ++total;
                    if (! reachable.contains (ranged->getParameterID()))
                        missing.add (ranged->getParameterID());
                }

            for (const auto& id : reachable)
                expect (proc.getAPVTS().getParameter (id) != nullptr, "el panel nombra un parametro que no existe: " + id);

            logMessage ("parametros alcanzables: " + juce::String (total - missing.size()) + " de " + juce::String (total));
            expect (missing.isEmpty(), "sin control: " + missing.joinIntoString (", "));
        }

        beginTest ("cada pagina del LCD engancha sus 6 encoders al parametro correcto");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            auto& view = editor->getView();
            const auto& pages = gui::sp92::lcdPages();
            int subPages = 0;

            for (int p = 0; p < (int) pages.size(); ++p)
                for (int s = 0; s < (int) pages[(size_t) p].subPages.size(); ++s)
                {
                    view.selectPage (p, s);
                    ++subPages;

                    for (int i = 0; i < 6; ++i)
                    {
                        const auto* id = pages[(size_t) p].subPages[(size_t) s][(size_t) i].paramID;
                        expectEquals (view.getEncoderParamID (i), id != nullptr ? juce::String (id) : juce::String());
                    }
                }

            logMessage ("subpaginas revisadas: " + juce::String (subPages));

            // La tecla de funcion cicla subpaginas.
            view.selectPage (1, 0);
            expectEquals (view.getPage(), 1);
            expectEquals (view.getSubPage(), 0);
        }

        beginTest ("ninguna pieza se sale del aparato ni se pisa con otra");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            auto& view = editor->getView();
            std::vector<juce::Component*> items;

            for (int i = 0; i < view.getNumChildComponents(); ++i)
                if (view.getChildComponent (i)->getProperties().contains (gui::sp92::layoutItemProperty))
                    items.push_back (view.getChildComponent (i));

            int problems = 0;

            for (size_t i = 0; i < items.size(); ++i)
            {
                const auto b = items[i]->getBounds();

                if (b.isEmpty() || ! view.getLocalBounds().contains (b))
                {
                    ++problems;
                    expect (false, "'" + items[i]->getTitle() + "' fuera del aparato: " + b.toString());
                }

                for (size_t j = i + 1; j < items.size(); ++j)
                    if (b.intersects (items[j]->getBounds()))
                    {
                        ++problems;
                        expect (false, "'" + items[i]->getTitle() + "' " + b.toString() + " se pisa con '"
                                           + items[j]->getTitle() + "' " + items[j]->getBounds().toString());
                    }
            }

            logMessage ("piezas del panel revisadas: " + juce::String ((int) items.size()));
            expectEquals (problems, 0);
        }

        beginTest ("nada visible se toca: cada pieza tiene al menos 3.5 px de aire");
        {
            auto procPtr = std::make_unique<StabrProcessor>();
            auto& proc = *procPtr;
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            using Item = gui::sp92::DeviceView::VisualItem;
            const auto items = editor->getView().getVisualItems();

            auto pointToRect = [] (juce::Rectangle<float> r, juce::Point<float> p)
            {
                const float dx = juce::jmax (r.getX() - p.x, 0.0f, p.x - r.getRight());
                const float dy = juce::jmax (r.getY() - p.y, 0.0f, p.y - r.getBottom());
                return std::hypot (dx, dy);
            };

            auto gap = [&] (const Item& a, const Item& b)
            {
                if (a.round && b.round)
                    return a.area.getCentre().getDistanceFrom (b.area.getCentre()) - (a.area.getWidth() + b.area.getWidth()) * 0.5f;

                if (a.round || b.round)
                {
                    const auto& circle = a.round ? a : b;
                    const auto& box = a.round ? b : a;
                    return pointToRect (box.area, circle.area.getCentre()) - circle.area.getWidth() * 0.5f;
                }

                const float dx = juce::jmax (a.area.getX() - b.area.getRight(), b.area.getX() - a.area.getRight());
                const float dy = juce::jmax (a.area.getY() - b.area.getBottom(), b.area.getY() - a.area.getBottom());

                if (dx > 0.0f && dy > 0.0f)
                    return std::hypot (dx, dy);

                return juce::jmax (dx, dy);
            };

            int problems = 0;
            float tightest = 1.0e6f;
            juce::String tightestPair;

            for (size_t i = 0; i < items.size(); ++i)
                for (size_t j = i + 1; j < items.size(); ++j)
                {
                    const auto& a = items[i];
                    const auto& b = items[j];

                    // Lo que va adentro de otra pieza (pantalla en su marco, pads en su pozo).
                    if (a.area.contains (b.area) || b.area.contains (a.area))
                        continue;

                    const float g = gap (a, b);

                    if (g < tightest)
                    {
                        tightest = g;
                        tightestPair = a.name + " / " + b.name;
                    }

                    if (g < 3.5f)
                    {
                        ++problems;
                        expect (false, "'" + a.name + "' y '" + b.name + "' a " + juce::String (g, 1) + " px");
                    }
                }

            logMessage ("piezas visibles: " + juce::String ((int) items.size()) + ", separacion minima "
                        + juce::String (tightest, 1) + " px (" + tightestPair + ")");
            expectEquals (problems, 0);
        }

        beginTest ("la pantalla: puntos cuadrados, identicos y centrados");
        {
            gui::sp92::DotMatrix m (190, 72);
            m.setSize (524, 196);

            const auto first = m.getDotRect (0, 0);
            const auto lastDot = m.getDotRect (189, 71);
            expectWithinAbsoluteError (first.getWidth(), first.getHeight(), 1.0e-4f);
            expectWithinAbsoluteError (first.getWidth(), lastDot.getWidth(), 1.0e-4f);

            const auto grid = m.getGridArea();
            expectWithinAbsoluteError (grid.getCentreX(), 262.0f, 0.01f);
            expectWithinAbsoluteError (grid.getCentreY(), 98.0f, 0.01f);

            // Un punto encendido pintado a una escala rara (125 %) pone la misma
            // cantidad de tinta (alfa de la capa de puntos, sin el vidrio) en
            // cualquier lugar de la pantalla: nada de redondeos por posicion.
            auto alphaSum = [] (gui::sp92::DotMatrix& dots)
            {
                juce::Image img (juce::Image::ARGB, 655, 245, true, juce::SoftwareImageType());
                {
                    juce::Graphics g (img);
                    g.addTransform (juce::AffineTransform::scale (1.25f));
                    dots.paintEntireComponent (g, false);
                }

                const auto& layer = dots.getDotLayer();
                double total = 0.0;
                for (int py = 0; py < layer.getHeight(); ++py)
                    for (int px = 0; px < layer.getWidth(); ++px)
                        total += layer.getPixelAt (px, py).getFloatAlpha();

                return total;
            };

            gui::sp92::DotMatrix blank (190, 72);
            blank.setSize (524, 196);
            blank.commit();
            const double base = alphaSum (blank);

            auto lightOf = [&] (int x, int y)
            {
                gui::sp92::DotMatrix dots (190, 72);
                dots.setSize (524, 196);
                dots.set (x, y);
                dots.commit();
                return alphaSum (dots) - base;
            };

            const double a = lightOf (10, 10), b = lightOf (11, 10), c = lightOf (100, 40), d = lightOf (157, 63);
            logMessage ("luz de un punto en cuatro lugares: " + juce::String (a, 2) + ", " + juce::String (b, 2) + ", "
                        + juce::String (c, 2) + ", " + juce::String (d, 2));
            expect (a > 1.0);
            expectWithinAbsoluteError (b / a, 1.0, 0.03);
            expectWithinAbsoluteError (c / a, 1.0, 0.03);
            expectWithinAbsoluteError (d / a, 1.0, 0.03);
        }

        beginTest ("accesibilidad: todo control tiene nombre y se alcanza con teclado");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            int sliders = 0, buttons = 0, steps = 0;
            juce::StringArray problems;

            walk (editor->getView(), [&] (juce::Component& c)
            {
                const bool isSlider = dynamic_cast<juce::Slider*> (&c) != nullptr;
                const bool isButton = dynamic_cast<juce::Button*> (&c) != nullptr;
                const bool isStep = c.getProperties().contains (gui::stepParamIdsProperty);

                if (! (isSlider || isButton || isStep))
                    return;

                sliders += isSlider ? 1 : 0;
                buttons += isButton ? 1 : 0;
                steps += isStep ? 1 : 0;

                if (c.getTitle().isEmpty())
                    problems.add ("sin nombre accesible: " + c.getName());

                if (c.isEnabled() && ! c.getWantsKeyboardFocus())
                    problems.add ("no alcanzable con teclado: " + c.getTitle());
            });

            /*  Cuenta exacta:
                perillas 12 (tono 5, eco 3, swing, gate, rate, output) + selector de patron + 6 encoders = 19
                botones: 7 funciones + 3 disco + disquetera + POWER + 3 modos de eco + SEQ + STAB + Undo + Redo + 3 palancas = 22
                pasos: 16 */
            logMessage ("perillas " + juce::String (sliders) + ", botones " + juce::String (buttons) + ", pasos " + juce::String (steps));
            expectEquals (sliders, 19);
            expectEquals (buttons, 22);
            expectEquals (steps, 16);
            expect (problems.isEmpty(), problems.joinIntoString ("; "));
        }

        beginTest ("contraste de los tres acabados (WCAG)");
        {
            for (int i = 0; i < gui::sp92::kNumFinishes; ++i)
            {
                const auto& f = gui::sp92::finish (i);
                const juce::String finishName (f.name);

                auto check = [&] (juce::Colour a, juce::Colour b, double minimum, const char* what)
                {
                    const double ratio = gui::contrastRatio (a, b);
                    expect (ratio >= minimum, finishName + ": " + what + " " + juce::String (ratio, 2));
                    return ratio;
                };

                const double silk = check (f.silk, f.chassis, 4.5, "serigrafia sobre chasis");
                check (f.stripInk, f.strip, 4.5, "texto de la franja");
                const double lcd = check (f.lcdOn, f.lcdBg2, 4.5, "pantalla");
                check (f.keyInk, f.key, 4.5, "tecla clara");
                check (gui::sp92::darkKey::ink, gui::sp92::darkKey::base, 4.5, "tecla oscura");

                logMessage (finishName + ": serigrafia " + juce::String (silk, 2) + ", pantalla " + juce::String (lcd, 2));
            }
        }

        beginTest ("escala de 0.75x a 2x, y la escala y el acabado se guardan con el proyecto");
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

            editor->setUiScale (1.5f);
            editor->setFinish (2);
            proc.setTapeText ("peak time 128");

            juce::MemoryBlock blob;
            proc.getStateInformation (blob);
            editor.reset();

            StabrProcessor other;
            other.setStateInformation (blob.getData(), (int) blob.getSize());
            auto reopened = makeEditor (other);

            if (reopened != nullptr)
            {
                expectWithinAbsoluteError (reopened->getWidth(), juce::roundToInt (W * 1.5f), 1);
                expectEquals (reopened->getFinish(), 2);
                expectEquals (other.getTapeText(), juce::String ("peak time 128"));
            }
        }

        beginTest ("los pads: editar un patron de fabrica lo copia a Custom sin perder nada");
        {
            StabrProcessor proc;
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            auto& grid = editor->getView().getStepGrid();
            expect (! grid.isCustomPattern());

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

        beginTest ("el LCD muestra el acorde que suena, y el costo de dibujar el aparato entero");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);
            auto editor = makeEditor (proc);

            if (editor == nullptr)
                return;

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer silence;

            // Medio segundo antes: el delay de cinta tiene que llegar a su tiempo.
            for (int i = 0; i < 100; ++i)
                proc.processBlock (buffer, silence);

            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 62, 0.9f), 0);
            proc.processBlock (buffer, on);

            const double t0 = juce::Time::getMillisecondCounterHiRes() * 0.001;
            auto& view = editor->getView();
            view.tick (t0);

            expectEquals (view.getLcdChordName(), juce::String ("Dm7"));

            int lit = 0;
            for (int y = 0; y < view.getLcd().getDotsHigh(); ++y)
                for (int x = 0; x < view.getLcd().getDotsWide(); ++x)
                    lit += view.getLcd().isLit (x, y) ? 1 : 0;

            expect (lit > 200, "la pantalla casi no tiene puntos encendidos: " + juce::String (lit));

            for (float s : { 1.0f, 2.0f })
            {
                editor->setUiScale (s);

                constexpr int frames = 4;
                const double start = juce::Time::getMillisecondCounterHiRes();
                juce::String perFrame;

                for (int i = 0; i < frames; ++i)
                {
                    const double f0 = juce::Time::getMillisecondCounterHiRes();
                    view.tick (t0 + 0.05 * i);
                    auto image = editor->createComponentSnapshot (editor->getLocalBounds());
                    expect (image.isValid());
                    perFrame << juce::String (juce::Time::getMillisecondCounterHiRes() - f0, 1) << (i + 1 < frames ? " / " : "");
                }

                const double ms = (juce::Time::getMillisecondCounterHiRes() - start) / frames;
                logMessage ("cuadro completo a " + juce::String (s, 2) + "x (" + juce::String (editor->getWidth()) + "x"
                            + juce::String (editor->getHeight()) + "): " + juce::String (ms, 1) + " ms, render por software ("
                            + perFrame + ")");
                expect (ms < 400.0, "dibujar el aparato entero tarda " + juce::String (ms, 1) + " ms");
            }

            /*  Volcado opcional a PNG de los tres acabados y de una pagina con
                grafico, para revisar a ojo. Solo con STABR_SNAPSHOT_DIR. */
            const auto dir = juce::SystemStats::getEnvironmentVariable ("STABR_SNAPSHOT_DIR", {});

            if (dir.isNotEmpty())
            {
                editor->setUiScale (1.0f);

                auto save = [&] (const juce::String& fileName)
                {
                    auto image = editor->createComponentSnapshot (editor->getLocalBounds());
                    juce::File file (juce::File (dir).getChildFile (fileName));
                    file.deleteFile();
                    juce::FileOutputStream stream (file);
                    juce::PNGImageFormat().writeImageToStream (image, stream);
                    logMessage ("snapshot: " + file.getFullPathName());
                };

                // Cuarenta cuadros a 60 Hz: todo lo que se mueve llega a su reposo.
                auto settle = [&] (double from)
                {
                    for (int i = 0; i < 40; ++i)
                        view.tick (from + i / 60.0);
                };

                for (int f = 0; f < gui::sp92::kNumFinishes; ++f)
                {
                    editor->setFinish (f);
                    proc.processBlock (buffer, on);
                    settle (t0 + 1.0 + f);
                    save ("sp92_finish_" + juce::String (f) + ".png");
                }

                editor->setFinish (1);
                view.selectPage (2, 0);
                settle (t0 + 10.0);
                save ("sp92_filter_page.png");
            }
        }
    }
};

//==============================================================================
class MotionTests : public juce::UnitTest
{
public:
    MotionTests() : juce::UnitTest ("SP-92 motion y tipografia", "GUI") {}

    void runTest() override
    {
        using namespace gui::sp92;

        beginTest ("las tres tipografias de la maqueta van embebidas y cargan");
        {
            expect (fonts::usingEmbeddedFonts(), "el binario no trae las fuentes");

            auto typefaceName = [] (const juce::Font& f)
            {
                auto tf = f.getTypefacePtr();
                return tf != nullptr ? tf->getName() : juce::String();
            };

            expect (typefaceName (fonts::display (12.0f)).containsIgnoreCase ("Michroma"), typefaceName (fonts::display (12.0f)));
            expect (typefaceName (fonts::label (12.0f)).containsIgnoreCase ("Archivo Narrow"), typefaceName (fonts::label (12.0f)));
            expect (typefaceName (fonts::labelBold (12.0f)).containsIgnoreCase ("Archivo Narrow"), typefaceName (fonts::labelBold (12.0f)));
            expect (typefaceName (fonts::hand (12.0f)).containsIgnoreCase ("Reenie"), typefaceName (fonts::hand (12.0f)));

            // letter-spacing en em, como el CSS: .2 em a 10 px son 2 px por letra.
            const float plain  = fonts::textWidth (fonts::label (10.0f), "ABCDEFGHIJ");
            const float spaced = fonts::textWidth (fonts::label (10.0f, 0.2f), "ABCDEFGHIJ");
            logMessage ("10 letras a 10 px: " + juce::String (plain, 1) + " px, con .2 em: " + juce::String (spaced, 1) + " px");
            expectWithinAbsoluteError (spaced - plain, 20.0f, 2.5f);
        }

        beginTest ("el movimiento no depende de los cuadros por segundo");
        {
            anim::Spring a, b;
            for (int i = 0; i < 30; ++i) a.step (1.0f, 1.0f / 60.0f, 2.1f, 0.78f);
            for (int i = 0; i < 72; ++i) b.step (1.0f, 1.0f / 144.0f, 2.1f, 0.78f);
            expectWithinAbsoluteError (a.pos, b.pos, 0.01f);

            float x = 0.0f, y = 0.0f;
            for (int i = 0; i < 6; ++i)  x = anim::approach (x, 1.0f, 1.0f / 60.0f, 0.05f);
            for (int i = 0; i < 14; ++i) y = anim::approach (y, 1.0f, 1.0f / 144.0f, 0.05f);
            expectWithinAbsoluteError (x, y, 0.02f);

            VuMeter vu60, vu144;
            for (int i = 0; i < 60; ++i)  { vu60.push (0.25f, 0.25f);  vu60.tick (1.0f / 60.0f); }
            for (int i = 0; i < 144; ++i) { vu144.push (0.25f, 0.25f); vu144.tick (1.0f / 144.0f); }
            logMessage ("aguja del VU tras 1 s: " + juce::String (vu60.getNeedle(), 3) + " a 60 Hz, " + juce::String (vu144.getNeedle(), 3) + " a 144 Hz");
            expectWithinAbsoluteError (vu60.getNeedle(), vu144.getNeedle(), 0.02f);

            // cubic-bezier(.3,.7,.3,1) de la disquetera: empieza en 0, termina en 1, nunca retrocede.
            expectWithinAbsoluteError (anim::cubicBezier (0.3f, 0.7f, 0.3f, 1.0f, 0.0f), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (anim::cubicBezier (0.3f, 0.7f, 0.3f, 1.0f, 1.0f), 1.0f, 1.0e-4f);
            float prev = 0.0f;
            bool monotonic = true;
            for (int i = 1; i <= 50; ++i)
            {
                const float v = anim::cubicBezier (0.3f, 0.7f, 0.3f, 1.0f, (float) i / 50.0f);
                monotonic = monotonic && v >= prev - 1.0e-4f;
                prev = v;
            }
            expect (monotonic);
        }

        beginTest ("una perilla gira hasta el sonido nuevo en .45 s con ease-out y escalonado");
        {
            HardwareKnob knob ("small", 11, "Test knob");
            knob.setRange (0.0, 1.0);
            knob.setValue (0.0, juce::dontSendNotification);
            knob.setSize (80, 80);

            auto frame = [&knob] (double t)
            {
                knob.tick (t);
                juce::ignoreUnused (knob.createComponentSnapshot (knob.getLocalBounds()));
                return knob.getShownProportion();
            };

            expectWithinAbsoluteError (frame (100.0), 0.0, 1.0e-6);

            knob.setValue (1.0, juce::dontSendNotification);
            knob.animateToCurrentValue (100.0, 0.05);

            expectWithinAbsoluteError (frame (100.04), 0.0, 1.0e-6, "durante el escalonado la perilla espera");
            const double mid = frame (100.05 + HardwareKnob::kPresetSeconds * 0.5);
            expect (mid > 0.8 && mid < 0.95, "a mitad de camino un ease-out cubico va por 0.875: " + juce::String (mid, 3));
            expectWithinAbsoluteError (frame (100.6), 1.0, 1.0e-6);
        }

        beginTest ("una tecla se hunde aunque el clic dure menos que un cuadro, y su LED prende con rampa");
        {
            juce::TextButton key ("Key");
            key.setSize (60, 30);
            ButtonMotion motion;
            motion.add (key);

            auto prop = [&key] (const char* id) { return (float) (double) key.getProperties()[id]; };
            const double t0 = juce::Time::getMillisecondCounterHiRes() * 0.001;

            key.setState (juce::Button::buttonDown);
            key.setState (juce::Button::buttonNormal);
            motion.tick (t0 + 1.0 / 60.0, 1.0f / 60.0f);
            expect (prop ("sp92Press") > 0.5f, "el clic no se vio: " + juce::String (prop ("sp92Press")));

            for (int i = 2; i < 40; ++i)
                motion.tick (t0 + i / 60.0, 1.0f / 60.0f);
            expectEquals (prop ("sp92Press"), 0.0f, "la tecla no volvio arriba");

            key.setToggleState (true, juce::dontSendNotification);
            motion.tick (t0 + 41.0 / 60.0, 1.0f / 60.0f);
            const float lit = prop ("sp92LitAmt");
            expect (lit > 0.2f && lit < 0.9f, "el LED salto en vez de prender: " + juce::String (lit));
        }

        beginTest ("la pantalla tiene la inercia de un cristal liquido");
        {
            DotMatrix m (8, 8);
            m.set (2, 2);
            m.commit (0.004f);
            const float rising = m.getLevel (2, 2);
            expect (rising > 0.1f && rising < 0.6f, "prende: " + juce::String (rising));

            m.commit (0.05f);
            expectEquals (m.getLevel (2, 2), 1.0f);

            m.clear();
            m.commit (1.0f / 60.0f);
            const float falling = m.getLevel (2, 2);
            expect (! m.isLit (2, 2));
            expect (falling > 0.4f && falling < 0.9f, "se apaga con cola: " + juce::String (falling));

            m.commit (0.3f);
            expectEquals (m.getLevel (2, 2), 0.0f);
        }
    }
};

//==============================================================================
class LcdDrawingTests : public juce::UnitTest
{
public:
    LcdDrawingTests() : juce::UnitTest ("SP-92 pantallas: teclado, eco y columnas", "GUI") {}

    void runTest() override
    {
        using namespace gui::sp92;

        auto chordOf = [] (std::initializer_list<int> notes)
        {
            chord::ChordResult c;
            for (int n : notes)
                c.notes[(size_t) c.count++] = n;
            return c;
        };

        beginTest ("teclado: las teclas del acorde encendidas, las demas vacias");
        {
            DotMatrix m (190, 72);
            const auto dm7 = chordOf ({ 50, 53, 57, 60 });   // D3 F3 A3 C4
            lcdraw::drawKeyboard (m, dm7, 82, 189, 13, false);
            const auto k = lcdraw::keyboardFor (dm7, 82, 189, 13);

            expectEquals (k.octaves, 2);
            expectEquals (k.lowC, 48);
            expect (k.x >= 82 && k.x + k.width() - 1 <= 189, "el teclado se sale de su lugar");

            // Parte baja de cada blanca (debajo de las negras).
            auto lowerCentreLit = [&] (int note)
            {
                const auto r = k.keyInterior (note);
                return m.isLit (r.getCentreX(), r.getBottom() - 2);
            };

            for (int note : { 50, 53, 57, 60 })
                expect (lowerCentreLit (note), "no se encendio la nota " + juce::String (note));

            for (int note : { 48, 52, 55, 59, 62, 64, 71 })
                expect (! lowerCentreLit (note), "se encendio una nota que no suena: " + juce::String (note));

            // Negra que no suena: contorno con el centro vacio.
            const int cSharp = k.blackKeyCentre (49);
            expect (! m.isLit (cSharp, k.y + 6), "C# no suena y esta llena");
            expect (m.isLit (cSharp - 1, k.y + 6), "a C# le falta el contorno");

            // Los nombres, del bajo al agudo, a la derecha bajo el teclado.
            DotMatrix names (190, 72);
            names.textRight ("D F A C", 189, 13 + lcdraw::KeyboardLayout::whiteHeight + 4);
            bool same = true;
            for (int y = 39; y < 46; ++y)
                for (int x = 120; x < 190; ++x)
                    same = same && names.isLit (x, y) == m.isLit (x, y);
            expect (same, "los nombres de las notas no son D F A C");
        }

        beginTest ("teclado: negra que suena llena, y voicings abiertos en 3 y 4 octavas");
        {
            DotMatrix m (190, 72);
            const auto gSharp = chordOf ({ 56, 59, 63 });   // G#3 B3 D#4
            lcdraw::drawKeyboard (m, gSharp, 82, 189, 13, false);
            const auto k = lcdraw::keyboardFor (gSharp, 82, 189, 13);
            expect (m.isLit (k.blackKeyCentre (56), k.y + 6), "G# suena y no esta llena");
            expect (m.isLit (k.blackKeyCentre (63), k.y + 6), "D# suena y no esta llena");

            const auto open = chordOf ({ 36, 55, 64, 71 });   // C2 .. B4
            const auto k3 = lcdraw::keyboardFor (open, 82, 189, 13);
            expectEquals (k3.octaves, 3);
            expect (k3.contains (36) && k3.contains (71));

            const auto wide = chordOf ({ 36, 60, 76, 83 });
            const auto k4 = lcdraw::keyboardFor (wide, 82, 189, 13);
            expectEquals (k4.octaves, 4);
            expect (k4.x >= 82 && k4.x + k4.width() - 1 <= 189);
        }

        beginTest ("eco: carriles L y R, el golpe y una barra por repeticion en su canal");
        {
            DotMatrix m (96, 22);
            gui::DelayViewInfo info;
            info.timeL = 0.3f;
            info.timeR = 0.4f;
            info.feedback = 0.5f;
            info.mode = (int) fx::DelayMode::pingPong;

            lcdraw::drawEcho (m, info, 100.0f, 120.0f);

            std::array<gui::Echo, gui::kMaxEchoes> echoes {};
            const int count = gui::computeEchoes (info, echoes);
            int expected[2] { 1, 1 };   // el golpe original en cada carril
            for (int i = 0; i < count; ++i)
                ++expected[echoes[(size_t) i].channel];

            auto barsOnLane = [&m] (int lane)
            {
                int bars = 0;
                bool prev = false;
                for (int x = lcdraw::EchoScreen::laneStart; x < 96; ++x)
                {
                    const bool on = m.isLit (x, lane - 1);
                    bars += on && ! prev ? 1 : 0;
                    prev = on;
                }
                return bars;
            };

            expectEquals (barsOnLane (lcdraw::EchoScreen::laneL), expected[0]);
            expectEquals (barsOnLane (lcdraw::EchoScreen::laneR), expected[1]);
            expect (m.isLit (0, lcdraw::EchoScreen::laneL), "falta la L");
            expect (m.isLit (0, lcdraw::EchoScreen::laneR), "falta la R");

            // La primera repeticion es la mas alta: el volumen se lee en la altura.
            const int x1 = lcdraw::EchoScreen::xOf (echoes[0].time, gui::echoWindowSeconds (echoes, count));
            const int x3 = lcdraw::EchoScreen::xOf (echoes[2].time, gui::echoWindowSeconds (echoes, count));
            auto height = [&m] (int x, int lane) { int h = 0; while (h < 5 && m.isLit (x, lane - h - 1)) ++h; return h; };
            expect (height (x1, lcdraw::EchoScreen::laneL) > height (x3, lcdraw::EchoScreen::laneL));

            // Durante la repeticion, su barra se duplica.
            DotMatrix playing (96, 22);
            lcdraw::drawEcho (playing, info, echoes[0].time + 0.05f, 120.0f);
            expect (playing.isLit (x1 + 1, lcdraw::EchoScreen::laneL - 1), "la repeticion que suena no se enciende");
        }

        beginTest ("el nombre del acorde nunca pisa el teclado");
        {
            DotMatrix m (190, 72);
            const std::pair<const char*, chord::ChordResult> cases[]
            {
                { "G#m9",     chordOf ({ 56, 59, 63, 66, 70 }) },
                { "F B E",    chordOf ({ 41, 59, 76 }) },           // sin nombre, voicing abierto (3 octavas)
                { "C#m7b5/G", chordOf ({ 43, 49, 52, 59 }) },
                { "Am7",      chordOf ({ 36, 60, 76, 83 }) },       // 4 octavas
            };

            for (const auto& [chordName, notes] : cases)
            {
                const auto k = lcdraw::keyboardFor (notes, 82, 189, 13);
                const int scale = lcdraw::fitTextScale (m, chordName, k.x - 4 - 2);
                const int right = 2 + m.textWidth (chordName, scale) - 1;

                logMessage (juce::String (chordName) + ": x" + juce::String (scale) + ", termina en " + juce::String (right)
                            + ", el teclado empieza en " + juce::String (k.x));
                expect (right + 4 < k.x, juce::String (chordName) + " pisa el teclado");
            }

            expectEquals (lcdraw::fitTextScale (m, "G#m9", 80), 3);
            expectEquals (lcdraw::fitTextScale (m, "F B E", 77), 2);
        }

        beginTest ("columnas del LCD: siempre con aire y dentro de la pantalla");
        {
            juce::Random rng (0x5AB);
            int worstGap = 100;

            for (int trial = 0; trial < 5000; ++trial)
            {
                std::array<int, 6> widths {};
                for (auto& w : widths)
                    w = trial == 0 ? 29 : rng.nextInt (30);

                const auto left = lcdraw::layoutCells (widths, 190);
                expect (left[0] >= 0 && left[5] + widths[5] <= 190, "columnas fuera de la pantalla");

                for (size_t i = 1; i < 6; ++i)
                {
                    const int gap = left[i] - (left[i - 1] + widths[i - 1]);
                    worstGap = juce::jmin (worstGap, gap);
                    if (gap < 3)
                        expect (false, "columnas pegadas: " + juce::String (gap));
                }
            }

            const auto chordPage = lcdraw::layoutCells ({ 23, 17, 29, 29, 17, 23 }, 190);   // TYPE KEY SCALE VOICE INV DENS
            const int scaleVoiceGap = chordPage[3] - (chordPage[2] + 29);
            logMessage ("aire minimo en 5000 casos: " + juce::String (worstGap) + " puntos; SCALE-VOICE: " + juce::String (scaleVoiceGap));
            expect (scaleVoiceGap >= 4);
        }
    }
};

static LcdDrawingTests lcdDrawingTests;
static MotionTests motionTests;
static ChordNamingTests chordNamingTests;
static GuiFeedTests guiFeedTests;
static EchoModelTests echoModelTests;
static LcdTests lcdTests;
static EditorTests editorTests;
}
