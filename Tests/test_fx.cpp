#include <juce_core/juce_core.h>

#include "FX/FxChain.h"
#include "PluginProcessor.h"
#include "Params/ParameterIDs.h"

#include <vector>
#include <cmath>

namespace
{
using namespace stabr;
using namespace stabr::fx;

constexpr double kPi = 3.14159265358979323846;

/*  Posicion del pico de un impulso en el buffer, o -1 si no hay nada. */
int findPeak (const std::vector<float>& x, int from, float threshold)
{
    for (int i = from; i < (int) x.size(); ++i)
        if (std::abs (x[(size_t) i]) > threshold)
            return i;

    return -1;
}

//==============================================================================
class TempoSyncTests : public juce::UnitTest
{
public:
    TempoSyncTests() : juce::UnitTest ("TempoSync", "FX") {}

    void runTest() override
    {
        beginTest ("las divisiones dan los tiempos correctos a 120 BPM");
        {
            // A 120 BPM una negra dura exactamente 0.5 s.
            expectWithinAbsoluteError (divisionToSeconds (Division::n1_4, 120.0), 0.5f, 1.0e-6f);
            expectWithinAbsoluteError (divisionToSeconds (Division::n1_8, 120.0), 0.25f, 1.0e-6f);
            expectWithinAbsoluteError (divisionToSeconds (Division::n1_16, 120.0), 0.125f, 1.0e-6f);
            expectWithinAbsoluteError (divisionToSeconds (Division::bar1, 120.0), 2.0f, 1.0e-6f);
        }

        beginTest ("1/8 punteado ES 3/16, el tiempo del dub techno");
        {
            /*  El delay clasico de dub techno es 3/16. Si esto no da exacto,
                las repeticiones se corren del pulso y todo el efecto se pierde. */
            const float dotted8th = divisionToSeconds (Division::n1_8D, 120.0);
            const float threeSixteenths = 3.0f * divisionToSeconds (Division::n1_16, 120.0);

            expectWithinAbsoluteError (dotted8th, threeSixteenths, 1.0e-6f,
                                       "1/8. no coincide con 3/16");
            expectWithinAbsoluteError (dotted8th, 0.375f, 1.0e-6f);
        }

        beginTest ("los tresillos son dos tercios de su division");
        {
            for (auto pair : { std::pair { Division::n1_8T,  Division::n1_8 },
                               std::pair { Division::n1_4T,  Division::n1_4 },
                               std::pair { Division::n1_16T, Division::n1_16 } })
            {
                const float triplet = divisionToSeconds (pair.first, 128.0);
                const float straight = divisionToSeconds (pair.second, 128.0);

                expectWithinAbsoluteError (triplet, straight * 2.0f / 3.0f, 1.0e-6f,
                                           "el tresillo no es 2/3 de la division");
            }
        }

        beginTest ("un BPM invalido no produce infinito");
        {
            // Un host puede informar 0 mientras no hay transporte.
            for (double bpm : { 0.0, -10.0, 1.0e9 })
            {
                const float s = divisionToSeconds (Division::n1_4, bpm);

                expect (std::isfinite (s) && s > 0.0f,
                        "BPM " + juce::String (bpm) + " -> " + juce::String (s));
            }
        }

        beginTest ("las divisiones estan ordenadas de mas corta a mas larga");
        {
            // El orden es lo que hace que el control se sienta bien al girarlo.
            for (int i = 1; i < (int) Division::numDivisions; ++i)
            {
                const float previous = divisionToSeconds ((Division) (i - 1), 120.0);
                const float current  = divisionToSeconds ((Division) i, 120.0);

                expect (current > previous,
                        juce::String (divisionNames[(size_t) i]) + " no es mas larga que "
                            + juce::String (divisionNames[(size_t) (i - 1)]));
            }
        }
    }
};

//==============================================================================
class DelayLineTests : public juce::UnitTest
{
public:
    DelayLineTests() : juce::UnitTest ("DelayLine", "FX") {}

    void runTest() override
    {
        beginTest ("el impulso sale EXACTAMENTE en la muestra pedida");
        {
            /*  Es el test mas importante del delay: si el retardo no cae donde
                dice, el delay sincronizado no sirve para nada. */
            constexpr double sr = 48000.0;

            for (float ms : { 10.0f, 125.0f, 375.0f, 1000.0f })
            {
                DelayLine line;
                line.prepare (sr);
                line.setDelaySeconds (ms * 0.001f);
                line.snapToTarget();

                const int expectedSample = (int) std::round (ms * 0.001f * sr);
                const int total = expectedSample + 64;

                std::vector<float> out ((size_t) total, 0.0f);

                for (int i = 0; i < total; ++i)
                {
                    line.advanceGlide();
                    out[(size_t) i] = line.read();
                    line.write (i == 0 ? 1.0f : 0.0f);
                }

                const int peak = findPeak (out, 0, 0.4f);

                logMessage ("  " + juce::String (ms, 0) + " ms -> pico en la muestra "
                            + juce::String (peak) + " (esperado " + juce::String (expectedSample) + ")");

                expect (peak >= 0, "no aparecio el eco para " + juce::String (ms) + " ms");
                expect (std::abs (peak - expectedSample) <= 1,
                        "el eco cayo en " + juce::String (peak) + " en vez de "
                            + juce::String (expectedSample));
            }
        }

        beginTest ("la interpolacion Lagrange no distorsiona un tono");
        {
            /*  Con un retardo fraccionario, una interpolacion pobre mete
                armonicos. Se mide la energia fuera de la fundamental. */
            constexpr double sr = 48000.0;
            constexpr double freq = 997.0;

            DelayLine line;
            line.prepare (sr);
            line.setDelaySeconds (100.5f / (float) sr);   // retardo con fraccion 0.5
            line.snapToTarget();

            constexpr int n = 8192;
            std::vector<double> out ((size_t) n);

            for (int i = 0; i < n; ++i)
            {
                line.advanceGlide();
                out[(size_t) i] = line.read();
                line.write ((float) std::sin (2.0 * kPi * freq * i / sr));
            }

            // Energia total vs energia en la fundamental, descartando el inicio.
            double total = 0.0;
            double re = 0.0, im = 0.0;
            int count = 0;

            for (int i = 1024; i < n; ++i)
            {
                const double v = out[(size_t) i];
                total += v * v;

                const double w = 2.0 * kPi * freq * i / sr;
                re += v * std::cos (w);
                im -= v * std::sin (w);
                ++count;
            }

            const double fundamental = 2.0 * std::sqrt (re * re + im * im) / count;
            const double totalRms = std::sqrt (total / count);
            const double fundamentalRms = fundamental / std::sqrt (2.0);

            const double thd = std::sqrt (std::max (0.0, totalRms * totalRms
                                                           - fundamentalRms * fundamentalRms))
                                   / (fundamentalRms + 1.0e-12);

            logMessage ("  distorsion con retardo fraccionario: "
                        + juce::String (20.0 * std::log10 (thd + 1.0e-12), 1) + " dB");

            expect (thd < 0.01, "la interpolacion distorsiona: THD "
                                    + juce::String (thd * 100.0, 2) + " %");
        }

        beginTest ("modo cinta: cambiar el tiempo desliza en vez de saltar");
        {
            constexpr double sr = 48000.0;

            DelayLine line;
            line.prepare (sr);
            line.setGlideMode (DelayLine::GlideMode::tape);
            line.setDelaySeconds (0.25f);
            line.snapToTarget();

            const float before = line.getCurrentDelaySamples();
            line.setDelaySeconds (0.5f);

            // Justo despues del cambio, el puntero casi no se movio.
            line.advanceGlide();
            const float justAfter = line.getCurrentDelaySamples();

            expect (std::abs (justAfter - before) < 100.0f,
                    "el modo cinta salto en vez de deslizar");

            // Tras un segundo tiene que haber llegado (casi) al destino.
            for (int i = 0; i < (int) sr; ++i)
                line.advanceGlide();

            const float target = 0.5f * (float) sr;
            expect (std::abs (line.getCurrentDelaySamples() - target) < target * 0.02f,
                    "el modo cinta no llego al destino: "
                        + juce::String (line.getCurrentDelaySamples()));
        }

        beginTest ("modo digital: el tiempo cambia de una");
        {
            DelayLine line;
            line.prepare (48000.0);
            line.setGlideMode (DelayLine::GlideMode::digital);
            line.setDelaySeconds (0.25f);
            line.snapToTarget();

            line.setDelaySeconds (0.5f);
            line.advanceGlide();

            expectWithinAbsoluteError (line.getCurrentDelaySamples(), 0.5f * 48000.0f, 1.0f,
                                       "el modo digital deberia saltar al destino de una");
        }
    }
};

//==============================================================================
class DelayTests : public juce::UnitTest
{
public:
    DelayTests() : juce::UnitTest ("Delay", "FX") {}

    void runTest() override
    {
        beginTest ("con feedback al maximo NO diverge");
        {
            /*  El saturador del lazo es lo unico que separa un delay musical de
                uno que desborda. Sin el, feedback 0.98 crece sin limite. */
            Delay delay;
            delay.prepare (48000.0, 512);

            DelayParams p;
            p.feedback = 1.0f;       // se clampea internamente
            p.mix = 1.0f;
            p.ducking = 0.0f;
            p.tempoSync = false;
            p.freeMs = 50.0f;
            delay.setParams (p, 120.0);

            float peak = 0.0f;

            for (int i = 0; i < 48000 * 20; ++i)
            {
                float l = (i < 4800) ? 0.9f * (float) std::sin (2.0 * kPi * 220.0 * i / 48000.0) : 0.0f;
                float r = l;

                delay.process (l, r);

                if (! std::isfinite (l) || ! std::isfinite (r))
                {
                    expect (false, "NaN/inf en la muestra " + juce::String (i));
                    return;
                }

                peak = juce::jmax (peak, std::abs (l));
            }

            logMessage ("  pico tras 20 s con feedback al maximo: " + juce::String (peak, 3));
            expect (peak < 8.0f, "el lazo diverge: pico " + juce::String (peak));
        }

        beginTest ("freeze se sostiene sin desbordar");
        {
            Delay delay;
            delay.prepare (48000.0, 512);

            DelayParams p;
            p.tempoSync = false;
            p.freeMs = 200.0f;
            p.mix = 1.0f;
            p.ducking = 0.0f;
            delay.setParams (p, 120.0);

            // Se carga el lazo.
            for (int i = 0; i < 24000; ++i)
            {
                float l = 0.7f * (float) std::sin (2.0 * kPi * 330.0 * i / 48000.0);
                float r = l;
                delay.process (l, r);
            }

            p.freeze = true;
            delay.setParams (p, 120.0);

            float peak = 0.0f, lateRms = 0.0f;
            int lateCount = 0;

            for (int i = 0; i < 48000 * 30; ++i)
            {
                float l = 0.0f, r = 0.0f;    // sin entrada: el freeze se sostiene solo
                delay.process (l, r);

                if (! std::isfinite (l))
                {
                    expect (false, "NaN en freeze, muestra " + juce::String (i));
                    return;
                }

                peak = juce::jmax (peak, std::abs (l));

                if (i > 48000 * 25)
                {
                    lateRms += l * l;
                    ++lateCount;
                }
            }

            lateRms = std::sqrt (lateRms / juce::jmax (1, lateCount));

            logMessage ("  freeze: pico " + juce::String (peak, 3)
                        + ", RMS a los 25-30 s " + juce::String (lateRms, 4));

            expect (peak < 8.0f, "el freeze desborda: pico " + juce::String (peak));
            expect (lateRms > 0.001f,
                    "el freeze se apago: RMS " + juce::String (lateRms, 5)
                        + ". Deberia sostenerse indefinidamente");
        }

        beginTest ("el ducking baja las repeticiones mientras suena lo seco");
        {
            /*  Sin ducking cada repeticion se superpone con el stab siguiente y
                a los cuatro golpes hay papilla. Es lo que separa un delay que
                suena bien solo de uno que suena bien en una mezcla. */
            auto measureWetDuringDry = [] (float ducking)
            {
                Delay delay;
                delay.prepare (48000.0, 512);

                DelayParams p;
                p.tempoSync = false;
                p.freeMs = 100.0f;
                p.feedback = 0.6f;
                p.mix = 1.0f;
                p.ducking = ducking;
                delay.setParams (p, 120.0);

                // Se carga el lazo con un golpe, se espera, y despues se mide
                // la salida MIENTRAS entra un segundo golpe sostenido.
                for (int i = 0; i < 4800; ++i)
                {
                    float l = 0.8f * (float) std::sin (2.0 * kPi * 220.0 * i / 48000.0);
                    float r = l;
                    delay.process (l, r);
                }

                float sum = 0.0f;
                int count = 0;

                for (int i = 0; i < 4800; ++i)
                {
                    const float dry = 0.8f * (float) std::sin (2.0 * kPi * 220.0 * i / 48000.0);
                    float l = dry, r = dry;
                    delay.process (l, r);

                    // Lo que agrego el delay por encima de lo seco.
                    sum += std::abs (l - dry);
                    ++count;
                }

                return sum / juce::jmax (1, count);
            };

            const float withoutDuck = measureWetDuringDry (0.0f);
            const float withDuck    = measureWetDuringDry (0.9f);

            logMessage ("  humedo durante lo seco: sin ducking " + juce::String (withoutDuck, 4)
                        + ", con ducking " + juce::String (withDuck, 4));

            expect (withDuck < withoutDuck * 0.8f,
                    "el ducking no baja las repeticiones lo suficiente");
        }

        beginTest ("ping pong reparte las repeticiones entre los canales");
        {
            Delay delay;
            delay.prepare (48000.0, 512);

            DelayParams p;
            p.mode = DelayMode::pingPong;
            p.tempoSync = false;
            p.freeMs = 100.0f;
            p.feedback = 0.7f;
            p.mix = 1.0f;
            p.ducking = 0.0f;
            delay.setParams (p, 120.0);

            std::vector<float> outL, outR;
            outL.reserve (48000);
            outR.reserve (48000);

            for (int i = 0; i < 48000; ++i)
            {
                float l = (i < 48) ? 1.0f : 0.0f;
                float r = l;
                delay.process (l, r);
                outL.push_back (l);
                outR.push_back (r);
            }

            // La primera repeticion va a la izquierda, la segunda a la derecha.
            const int firstL = findPeak (outL, 100, 0.05f);
            const int firstR = findPeak (outR, 100, 0.05f);

            logMessage ("  primer eco: L en " + juce::String (firstL)
                        + ", R en " + juce::String (firstR));

            expect (firstL > 0 && firstR > 0, "no aparecieron ecos en ping pong");
            expect (firstR > firstL,
                    "en ping pong el eco de la derecha tiene que llegar despues");
        }
    }
};

//==============================================================================
class ReverbTests : public juce::UnitTest
{
public:
    ReverbTests() : juce::UnitTest ("Reverb", "FX") {}

    void runTest() override
    {
        beginTest ("no diverge en ningun modo ni con decay al maximo");
        {
            for (int m = 0; m < (int) ReverbMode::numModes; ++m)
            {
                Reverb reverb;
                reverb.prepare (48000.0);

                ReverbParams p;
                p.mode = (ReverbMode) m;
                p.decay = 1.0f;
                p.size = 1.0f;
                p.damping = 0.0f;
                p.mix = 1.0f;
                p.ducking = 0.0f;
                reverb.setParams (p, 120.0);

                float peak = 0.0f;

                for (int i = 0; i < 48000 * 15; ++i)
                {
                    float l = (i < 480) ? 0.9f : 0.0f;
                    float r = l;

                    reverb.process (l, r);

                    if (! std::isfinite (l) || ! std::isfinite (r))
                    {
                        expect (false, juce::String (reverbModeNames[(size_t) m])
                                           + ": NaN en la muestra " + juce::String (i));
                        return;
                    }

                    peak = juce::jmax (peak, std::abs (l));
                }

                logMessage ("  " + juce::String (reverbModeNames[(size_t) m])
                            + ": pico " + juce::String (peak, 3));

                expect (peak < 8.0f, juce::String (reverbModeNames[(size_t) m])
                                         + " diverge: pico " + juce::String (peak));
            }
        }

        beginTest ("el decay controla cuanto dura la cola");
        {
            auto tailLength = [] (float decay)
            {
                Reverb reverb;
                reverb.prepare (48000.0);

                ReverbParams p;
                p.decay = decay;
                p.damping = 0.3f;
                p.mix = 1.0f;
                p.ducking = 0.0f;
                reverb.setParams (p, 120.0);

                float peak = 0.0f;
                int lastLoud = 0;

                for (int i = 0; i < 48000 * 12; ++i)
                {
                    float l = (i < 480) ? 0.9f : 0.0f;
                    float r = l;
                    reverb.process (l, r);

                    const float a = std::abs (l);
                    peak = juce::jmax (peak, a);

                    if (a > peak * 0.001f)   // -60 dB del pico
                        lastLoud = i;
                }

                return (double) lastLoud / 48000.0;
            };

            const double shortTail = tailLength (0.2f);
            const double longTail  = tailLength (0.9f);

            logMessage ("  cola con decay 0.2: " + juce::String (shortTail, 2)
                        + " s,  con 0.9: " + juce::String (longTail, 2) + " s");

            expect (longTail > shortTail * 2.0,
                    "el control de decay casi no cambia la cola");
        }

        beginTest ("la salida es estereo, no dos canales iguales");
        {
            // Si los dos canales salieran identicos, la reverb no daria ancho y
            // el reparto de lineas pares/impares estaria mal.
            Reverb reverb;
            reverb.prepare (48000.0);

            ReverbParams p;
            p.mix = 1.0f;
            p.ducking = 0.0f;
            reverb.setParams (p, 120.0);

            double difference = 0.0;
            int count = 0;

            for (int i = 0; i < 48000 * 2; ++i)
            {
                float l = (i < 480) ? 0.9f : 0.0f;
                float r = l;
                reverb.process (l, r);

                if (i > 4800)
                {
                    difference += std::abs (l - r);
                    ++count;
                }
            }

            difference /= juce::jmax (1, count);
            logMessage ("  diferencia media entre canales: " + juce::String (difference, 5));

            expect (difference > 1.0e-4, "los dos canales salen identicos: no hay estereo");
        }
    }
};

//==============================================================================
class PumpAndOutputTests : public juce::UnitTest
{
public:
    PumpAndOutputTests() : juce::UnitTest ("Pump y salida", "FX") {}

    void runTest() override
    {
        beginTest ("el pump cierra en el pulso y abre despues");
        {
            Pump pump;
            pump.prepare (48000.0);

            PumpParams p;
            p.enabled = true;
            p.division = Division::n1_4;
            p.depth = 1.0f;
            p.shape = 0.5f;
            pump.setParams (p);
            pump.setTransport (0.0, 120.0, true, true);

            // Un pulso a 120 BPM son 0.5 s = 24000 muestras.
            std::vector<float> gain;
            gain.reserve (24000);

            for (int i = 0; i < 24000; ++i)
                gain.push_back (pump.processSample());

            /*  El minimo NO esta en la muestra 0: con un ataque de 8 ms la
                envolvente tarda ~384 muestras en terminar de cerrar. La primera
                version de este test miraba la muestra 100, que todavia esta en
                mitad de la rampa de cierre, y fallaba contra codigo correcto.
                Hay que buscar el minimo real. */
            int minIndex = 0;

            for (int i = 1; i < (int) gain.size(); ++i)
                if (gain[(size_t) i] < gain[(size_t) minIndex])
                    minIndex = i;

            const float minimum = gain[(size_t) minIndex];
            const float atMiddle = gain[12000];
            const float atEnd = gain[23000];

            logMessage ("  minimo " + juce::String (minimum, 3)
                        + " en la muestra " + juce::String (minIndex)
                        + ",  medio " + juce::String (atMiddle, 3)
                        + ",  final " + juce::String (atEnd, 3));

            expect (minimum < 0.1f, "el pump no cierra: minimo " + juce::String (minimum, 3));
            expect (minIndex < 2000, "el cierre llega demasiado tarde en el ciclo");
            expect (atEnd > atMiddle, "el pump no se recupera hacia el final del ciclo");
            expect (atEnd > 0.8f, "el pump no llega a abrir del todo");
        }

        beginTest ("apagado, el pump no toca la senal");
        {
            Pump pump;
            pump.prepare (48000.0);

            PumpParams p;
            p.enabled = false;
            pump.setParams (p);
            pump.setTransport (0.0, 120.0, true, true);

            for (int i = 0; i < 1000; ++i)
                expectEquals (pump.processSample(), 1.0f);
        }

        beginTest ("los graves salen en mono por debajo de la frecuencia elegida");
        {
            /*  Requisito de club: los graves en estereo se cancelan al sumarse
                a mono y el sub de la pista los reproduce en mono igual. */
            StereoOutput out;
            out.prepare (48000.0);

            StereoOutputParams p;
            p.monoBelowHz = 120.0f;
            p.width = 2.0f;          // ancho al maximo, el peor caso
            p.limiterOn = false;
            out.setParams (p);

            auto measureDifference = [&] (double freq)
            {
                out.reset();
                out.setParams (p);

                double difference = 0.0;
                int count = 0;

                for (int i = 0; i < 24000; ++i)
                {
                    // Senal en contrafase: es lo que se cancela al sumar a mono.
                    const float s = (float) std::sin (2.0 * kPi * freq * i / 48000.0);
                    float l = s, r = -s;

                    out.process (l, r);

                    if (i > 4800)
                    {
                        difference += std::abs (l - r);
                        ++count;
                    }
                }

                return difference / juce::jmax (1, count);
            };

            const double lowDifference  = measureDifference (50.0);    // debajo del corte
            const double highDifference = measureDifference (2000.0);  // encima

            logMessage ("  contrafase a 50 Hz: " + juce::String (lowDifference, 4)
                        + ",  a 2 kHz: " + juce::String (highDifference, 4));

            expect (lowDifference < highDifference * 0.3,
                    "los graves no se estan monoficando: 50 Hz mantiene "
                        + juce::String (lowDifference, 4) + " de diferencia");
        }

        beginTest ("el limitador frena los picos");
        {
            StereoOutput out;
            out.prepare (48000.0);

            StereoOutputParams p;
            p.limiterOn = true;
            p.outputGainDb = 12.0f;   // de sobra para pasarse
            out.setParams (p);

            float peak = 0.0f;

            for (int i = 0; i < 48000; ++i)
            {
                float l = 0.9f * (float) std::sin (2.0 * kPi * 220.0 * i / 48000.0);
                float r = l;

                out.process (l, r);
                peak = juce::jmax (peak, std::max (std::abs (l), std::abs (r)));
            }

            logMessage ("  pico de salida con +12 dB: " + juce::String (peak, 4));
            expect (peak <= 1.02f, "el limitador dejo pasar " + juce::String (peak));
        }
    }
};

//==============================================================================
class FxIntegrationTests : public juce::UnitTest
{
public:
    FxIntegrationTests() : juce::UnitTest ("FX integracion", "FX") {}

    void runTest() override
    {
        beginTest ("los seis ordenes de la cadena producen audio valido");
        {
            for (int o = 0; o < (int) FxOrder::numOrders; ++o)
            {
                FxChain chain;
                chain.prepare (48000.0, 512);

                FxParams p;
                p.order = (FxOrder) o;
                p.saturation.drive = 0.7f;
                p.chorus.mix = 0.5f;
                p.delay.mix = 0.5f;
                p.delay.feedback = 0.6f;
                p.reverb.mix = 0.4f;
                p.pump.enabled = true;
                chain.setParams (p, 128.0);
                chain.setTransport (0.0, 128.0, true, true);

                for (int i = 0; i < 48000 * 3; ++i)
                {
                    float l = (i < 4800) ? 0.6f * (float) std::sin (2.0 * kPi * 220.0 * i / 48000.0)
                                         : 0.0f;
                    float r = l;

                    chain.process (l, r);

                    if (! std::isfinite (l) || ! std::isfinite (r) || std::abs (l) > 4.0f)
                    {
                        expect (false, juce::String (fxOrderNames[(size_t) o])
                                           + ": salida invalida en " + juce::String (i)
                                           + " -> " + juce::String (l));
                        return;
                    }
                }
            }

            expect (true);
        }

        beginTest ("60 s con todos los efectos y parametros aleatorios: sin NaN");
        {
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 1024);
            proc.prepareToPlay (48000.0, 1024);

            // Todos los efectos con mezcla audible.
            auto& apvts = proc.getAPVTS();
            apvts.getParameter (pid::delayMix)->setValueNotifyingHost (0.6f);
            apvts.getParameter (pid::reverbMix)->setValueNotifyingHost (0.5f);
            apvts.getParameter (pid::chorusMix)->setValueNotifyingHost (0.5f);
            apvts.getParameter (pid::satDrive)->setValueNotifyingHost (0.7f);
            apvts.getParameter (pid::pumpEnabled)->setValueNotifyingHost (1.0f);

            juce::AudioBuffer<float> buffer (2, 1024);
            juce::Random rng (0xF4C0);

            auto& params = proc.getParameters();
            int rendered = 0;
            const int total = (int) (60.0 * 48000.0);

            while (rendered < total)
            {
                const int n = rng.nextInt ({ 32, 1025 });

                if (rng.nextFloat() < 0.2f)
                    for (auto* param : params)
                        if (rng.nextFloat() < 0.15f)
                            param->setValueNotifyingHost (rng.nextFloat());

                juce::MidiBuffer midi;

                if (rng.nextFloat() < 0.3f)
                    midi.addEvent (juce::MidiMessage::noteOn (1, rng.nextInt ({ 30, 90 }),
                                                              rng.nextFloat()),
                                   rng.nextInt (n));

                buffer.setSize (2, n, false, false, true);
                buffer.clear();
                proc.processBlock (buffer, midi);

                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto* d = buffer.getReadPointer (ch);

                    for (int i = 0; i < n; ++i)
                    {
                        if (! std::isfinite (d[i]))
                        {
                            expect (false, "NaN/inf tras "
                                               + juce::String (rendered / 48000.0, 2) + " s");
                            return;
                        }
                    }
                }

                rendered += n;
            }

            expect (true);
            logMessage ("  60 s con toda la cadena activa y parametros aleatorios: todo finito");
        }

        beginTest ("silencio exacto con las colas de delay y reverb");
        {
            /*  Desde la Fase 4 esto es lo que justifica el SilenceGate (D-007):
                las colas decaen asintoticamente y NUNCA llegan a cero solas. */
            StabrProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 256);
            proc.prepareToPlay (48000.0, 256);

            auto& apvts = proc.getAPVTS();
            apvts.getParameter (pid::delayMix)->setValueNotifyingHost (0.8f);
            apvts.getParameter (pid::delayFeedback)->setValueNotifyingHost (0.7f);
            apvts.getParameter (pid::reverbMix)->setValueNotifyingHost (0.6f);

            juce::AudioBuffer<float> buffer (2, 256);

            juce::MidiBuffer note;
            note.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
            buffer.clear();
            proc.processBlock (buffer, note);

            juce::MidiBuffer empty;

            // Tiempo de sobra para que todas las colas mueran.
            for (int i = 0; i < 48000 * 40 / 256; ++i)
            {
                buffer.clear();
                proc.processBlock (buffer, empty);
            }

            float peak = 0.0f;

            for (int i = 0; i < 100; ++i)
            {
                buffer.clear();
                proc.processBlock (buffer, empty);

                for (int ch = 0; ch < 2; ++ch)
                    peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, 256));
            }

            expectEquals (peak, 0.0f,
                          "queda cola despues de 40 s: " + juce::String (peak));
        }
    }
};

static TempoSyncTests tempoSyncTests;
static DelayLineTests delayLineTests;
static DelayTests delayTests;
static ReverbTests reverbTests;
static PumpAndOutputTests pumpAndOutputTests;
static FxIntegrationTests fxIntegrationTests;
}
