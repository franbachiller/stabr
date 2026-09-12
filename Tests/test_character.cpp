#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/Oscillators/Oscillator.h"
#include "DSP/Filters/SvfTpt.h"
#include "DSP/Drive/Drive.h"

#include <vector>
#include <cmath>

namespace
{
using namespace stabr;
constexpr double kPi = 3.14159265358979323846;

/*  Magnitud a una frecuencia, con ventana de Blackman.

    LA VENTANA NO ES OPCIONAL: sin ella, la fuga espectral de los armonicos
    fuertes deja un piso de ~-50 dB que tapa por completo el aliasing que
    queremos medir. La primera version de esta medicion no la tenia y concluia
    que el sobremuestreo "no servia para nada".
*/
double magnitudeAt (std::vector<double> x, double freq, double fs)
{
    const int n = (int) x.size();

    for (int i = 0; i < n; ++i)
    {
        const double t = 2.0 * kPi * i / (n - 1);
        x[(size_t) i] *= 0.42 - 0.5 * std::cos (t) + 0.08 * std::cos (2.0 * t);
    }

    double re = 0.0, im = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double w = 2.0 * kPi * freq * i / fs;
        re += x[(size_t) i] * std::cos (w);
        im -= x[(size_t) i] * std::sin (w);
    }

    return 2.0 * std::sqrt (re * re + im * im) / n;
}

/*  Energia RMS en los bins que NO son armonicos de f, en dB.

    OJO CON LA FRECUENCIA DE PRUEBA: tiene que ser inconmensurable con el
    sample rate. Si f divide exacto a fs (por ejemplo 1500 Hz a 48 kHz), los
    componentes aliaseados caen JUSTO encima de armonicos reales y son
    indistinguibles: la medicion da -270 dB y parece que no hay aliasing.
*/
double aliasEnergyDb (const std::vector<double>& y, double f, double fs)
{
    const int n = (int) y.size();
    double acc = 0.0;
    int bins = 0;

    for (int k = 8; k < n / 2; k += 11)
    {
        const double freq = k * fs / n;
        const double ratio = freq / f;

        if (std::abs (ratio - std::round (ratio)) < 0.05)
            continue;   // armonico legitimo

        const double m = magnitudeAt (y, freq, fs);
        acc += m * m;
        ++bins;
    }

    return 10.0 * std::log10 (acc / juce::jmax (1, bins) + 1.0e-30);
}

//==============================================================================
class OversamplerTests : public juce::UnitTest
{
public:
    OversamplerTests() : juce::UnitTest ("Oversampler2x", "Character") {}

    void runTest() override
    {
        beginTest ("el kernel halfband esta bien construido");
        {
            const auto& taps = HalfbandKernel::evenTaps();

            double sum = 0.0;
            for (auto t : taps)
                sum += t;

            // Los pares suman 0.5 y el centro vale 0.5: ganancia unitaria en DC.
            expectWithinAbsoluteError (sum + 0.5, 1.0, 1.0e-4,
                                       "el kernel no tiene ganancia unitaria en continua");

            // Simetria: un FIR de fase lineal tiene que ser simetrico.
            for (int j = 0; j < HalfbandKernel::kFirTaps / 2; ++j)
                expectWithinAbsoluteError (taps[(size_t) j],
                                           taps[(size_t) (HalfbandKernel::kFirTaps - 1 - j)],
                                           1.0e-6f, "el kernel no es simetrico");

            const auto& range = HalfbandKernel::nonZeroRange();
            logMessage ("  coeficientes utiles: " + juce::String (range.first) + ".."
                        + juce::String (range.last) + " de " + juce::String (HalfbandKernel::kFirTaps));
        }

        beginTest ("round trip up -> down reproduce la senal");
        {
            Oversampler2x os;
            os.reset();

            constexpr int n = 8192;
            std::vector<float> in ((size_t) n), out ((size_t) n);

            juce::Random rng (0x0574);

            for (int i = 0; i < n; ++i)
                in[(size_t) i] = (float) (0.6 * std::sin (2.0 * kPi * 437.0 * i / 48000.0)
                                        + 0.3 * std::sin (2.0 * kPi * 2113.0 * i / 48000.0));

            for (int i = 0; i < n; ++i)
            {
                float a = 0.0f, b = 0.0f;
                os.upsample (in[(size_t) i], a, b);
                out[(size_t) i] = os.downsample (a, b);
            }

            // El retardo de grupo tiene que coincidir con el que usa el bypass
            // del drive: si no, encender el drive haria un salto.
            double bestError = 1.0e9;
            int bestDelay = 0;

            for (int d = 0; d <= 24; ++d)
            {
                double e = 0.0;
                int count = 0;

                for (int i = 500; i < n - 30; ++i)
                {
                    const double diff = out[(size_t) i] - in[(size_t) (i - d)];
                    e += diff * diff;
                    ++count;
                }

                e = std::sqrt (e / count);

                if (e < bestError)
                {
                    bestError = e;
                    bestDelay = d;
                }
            }

            logMessage ("  retardo " + juce::String (bestDelay) + " muestras, error "
                        + juce::String (20.0 * std::log10 (bestError + 1.0e-12), 1) + " dB");

            expectEquals (bestDelay, Drive::kGroupDelaySamples,
                          "el retardo real no coincide con el que compensa el bypass del drive");
            expect (bestError < 1.0e-3,
                    "el round trip pierde la senal: error " + juce::String (bestError));

            juce::ignoreUnused (rng);
        }

        beginTest ("rechazo de imagen del sobremuestreador");
        {
            // La imagen del zero-stuffing cae en (fs - f) dentro del flujo al
            // doble de tasa. El halfband tiene que matarla.
            for (double f : { 1000.0, 5000.0, 10000.0 })
            {
                Oversampler2x os;
                os.reset();

                constexpr int n = 8192;
                std::vector<double> up ((size_t) n * 2);

                for (int i = 0; i < n; ++i)
                {
                    float a = 0.0f, b = 0.0f;
                    os.upsample ((float) std::sin (2.0 * kPi * f * i / 48000.0), a, b);
                    up[(size_t) (2 * i)]     = a;
                    up[(size_t) (2 * i + 1)] = b;
                }

                const double signal = magnitudeAt (up, f, 96000.0);
                const double image  = magnitudeAt (up, 48000.0 - f, 96000.0);
                const double dB = 20.0 * std::log10 (image / (signal + 1.0e-12) + 1.0e-12);

                logMessage ("  f=" + juce::String (f, 0) + " Hz -> imagen a "
                            + juce::String (dB, 1) + " dB");

                expect (dB < -60.0,
                        "rechazo de imagen insuficiente a " + juce::String (f, 0) + " Hz: "
                            + juce::String (dB, 1) + " dB");
            }
        }
    }
};

//==============================================================================
class DriveTests : public juce::UnitTest
{
public:
    DriveTests() : juce::UnitTest ("Drive", "Character") {}

    void runTest() override
    {
        beginTest ("la curva esta acotada para cualquier entrada");
        {
            /*  La aproximacion de Pade a tanh VUELVE A CRECER fuera de +-3: sin
                el clamp, un pico fuerte saldria amplificado en vez de
                recortado. Este test lo verifica en un rango absurdo. */
            for (float x = -100.0f; x <= 100.0f; x += 0.1f)
            {
                const float y = drive_detail::shape (x);

                expect (std::isfinite (y), "NaN/inf en shape(" + juce::String (x) + ")");
                expect (std::abs (y) <= 1.0f + 1.0e-5f,
                        "shape(" + juce::String (x) + ") = " + juce::String (y)
                            + ", deberia estar acotado a +-1");
            }

            expectWithinAbsoluteError (drive_detail::shape (3.0f), 1.0f, 1.0e-5f,
                                       "el empalme en +3 no es continuo");
            expectWithinAbsoluteError (drive_detail::shape (0.0f), 0.0f, 1.0e-6f);
        }

        beginTest ("es asimetrico (genera armonicos pares)");
        {
            // Un saturador simetrico solo da impares y suena a distorsion
            // limpia. Los pares son los que dan el caracter de valvula.
            const auto s = drive_detail::settingsFor (1.0f);

            const float up   = drive_detail::shapeWithBias (0.5f, s.gain, s.bias, s.outputGain);
            const float down = drive_detail::shapeWithBias (-0.5f, s.gain, s.bias, s.outputGain);

            logMessage ("  +0.5 -> " + juce::String (up, 4)
                        + ",  -0.5 -> " + juce::String (down, 4));

            expect (std::abs (up + down) > 0.01f,
                    "la curva es simetrica: no va a generar armonicos pares");
        }

        beginTest ("la SALIDA queda acotada para todo drive y toda entrada");
        {
            /*  EL TEST QUE FALTABA. La primera version solo verificaba que
                shape() estuviera acotada, pero shapeWithBias() le resta el
                centro y multiplica por la compensacion: con un bias grande la
                salida se iba a -3.37 con la entrada en -0.5.

                Un drive que amplifica en vez de recortar le mete un offset
                enorme al filtro que viene despues. */
            float worst = 0.0f;
            float worstAmount = 0.0f, worstInput = 0.0f;

            for (int a = 0; a <= 20; ++a)
            {
                const float amount = (float) a / 20.0f;
                const auto s = drive_detail::settingsFor (amount);

                for (float x = -1.5f; x <= 1.5f; x += 0.01f)
                {
                    const float y = drive_detail::shapeWithBias (x, s.gain, s.bias, s.outputGain);

                    expect (std::isfinite (y), "NaN/inf con drive " + juce::String (amount));

                    if (std::abs (y) > worst)
                    {
                        worst = std::abs (y);
                        worstAmount = amount;
                        worstInput = x;
                    }
                }
            }

            logMessage ("  salida maxima: " + juce::String (worst, 4)
                        + " (drive " + juce::String (worstAmount, 2)
                        + ", entrada " + juce::String (worstInput, 2) + ")");

            expect (worst <= 1.05f,
                    "el drive amplifica en vez de recortar: pico " + juce::String (worst, 3)
                        + " con drive " + juce::String (worstAmount, 2));
        }

        beginTest ("el bypass tiene EXACTAMENTE el mismo retardo que el camino activo");
        {
            /*  Si los retardos no coinciden, mover el drive de 0 a algo hace
                saltar la voz 11 muestras y eso se escucha como un click. */
            constexpr int n = 2048;
            std::vector<float> in ((size_t) n), bypassed ((size_t) n), active ((size_t) n);

            for (int i = 0; i < n; ++i)
                in[(size_t) i] = (float) std::sin (2.0 * kPi * 220.0 * i / 48000.0);

            Drive off, on;
            off.reset();
            on.reset();
            off.setAmount (0.0f);
            on.setAmount (0.0001f);   // apenas encendido: la curva es casi identidad

            for (int i = 0; i < n; ++i)
            {
                bypassed[(size_t) i] = off.process (in[(size_t) i]);
                active[(size_t) i]   = on.process (in[(size_t) i]);
            }

            double maxDiff = 0.0;

            for (int i = 200; i < n; ++i)
                maxDiff = juce::jmax (maxDiff,
                                      (double) std::abs (bypassed[(size_t) i] - active[(size_t) i]));

            logMessage ("  diferencia maxima bypass vs activo: "
                        + juce::String (20.0 * std::log10 (maxDiff + 1.0e-12), 1) + " dB");

            expect (maxDiff < 1.0e-3,
                    "bypass y camino activo no estan alineados: diferencia "
                        + juce::String (maxDiff));
        }

        beginTest ("el sobremuestreo reduce el aliasing de verdad");
        {
            /*  Comparacion JUSTA: la MISMA curva con la MISMA ganancia, sobre
                una senoidal pura. Si la entrada fuera una sierra, su propio
                aliasing taparia el efecto que se quiere medir. */
            struct Case { double freq; float amount; };

            const Case cases[] { { 2718.281828, 1.0f }, { 5432.109876, 1.0f } };

            for (const auto& c : cases)
            {
                constexpr int n = 8192;
                const auto s = drive_detail::settingsFor (c.amount);

                std::vector<double> direct ((size_t) n), os2 ((size_t) n), os4 ((size_t) n);

                Drive d2, d4;
                d2.reset();
                d4.reset();
                d2.setAmount (c.amount);
                d2.setHighQuality (false);
                d4.setAmount (c.amount);
                d4.setHighQuality (true);

                for (int i = 0; i < n; ++i)
                {
                    const float x = (float) (0.9 * std::sin (2.0 * kPi * c.freq * i / 48000.0));

                    direct[(size_t) i] = drive_detail::shapeWithBias (x, s.gain, s.bias, s.outputGain);
                    os2[(size_t) i]    = d2.process (x);
                    os4[(size_t) i]    = d4.process (x);
                }

                const double aDirect = aliasEnergyDb (direct, c.freq, 48000.0);
                const double a2      = aliasEnergyDb (os2, c.freq, 48000.0);
                const double a4      = aliasEnergyDb (os4, c.freq, 48000.0);

                logMessage ("  f=" + juce::String (c.freq, 1)
                            + ": directo " + juce::String (aDirect, 1)
                            + " dB, x2 " + juce::String (a2, 1)
                            + " dB, x4 " + juce::String (a4, 1) + " dB");

                expect (a2 < aDirect - 5.0,
                        "x2 no mejora sobre el camino directo a " + juce::String (c.freq, 0) + " Hz");
                expect (a4 <= a2 + 1.0,
                        "x4 deberia ser al menos tan bueno como x2");
            }
        }
    }
};

//==============================================================================
class OscillatorTests : public juce::UnitTest
{
public:
    OscillatorTests() : juce::UnitTest ("Oscillator", "Character") {}

    void runTest() override
    {
        beginTest ("todas las formas son finitas y acotadas en todo el teclado");
        {
            for (int w = 0; w < (int) Waveform::numWaveforms; ++w)
            {
                for (double sr : { 44100.0, 48000.0, 192000.0 })
                {
                    Oscillator osc;
                    osc.setSampleRate (sr);
                    osc.setWaveform ((Waveform) w);

                    for (int note = 0; note <= 127; note += 3)
                    {
                        osc.setFrequency (440.0f * std::pow (2.0f, ((float) note - 69.0f) / 12.0f));
                        osc.reset (0.0f);

                        for (int i = 0; i < 512; ++i)
                        {
                            const float v = osc.process();

                            if (! std::isfinite (v) || std::abs (v) > 1.6f)
                            {
                                expect (false, juce::String (waveformNames[(size_t) w])
                                                   + " nota " + juce::String (note)
                                                   + " @ " + juce::String (sr, 0)
                                                   + " -> " + juce::String (v));
                                return;
                            }
                        }
                    }
                }
            }

            expect (true);
        }

        beginTest ("el pulso responde al ancho y nunca queda en continua");
        {
            for (float pw : { 0.1f, 0.25f, 0.5f, 0.75f, 0.9f })
            {
                Oscillator osc;
                osc.setSampleRate (48000.0);
                osc.setWaveform (Waveform::pulse);
                osc.setPulseWidth (pw);
                osc.setFrequency (220.0f);
                osc.reset (0.0f);

                double positive = 0.0;
                constexpr int n = 4096;

                for (int i = 0; i < n; ++i)
                    if (osc.process() > 0.0f)
                        ++positive;

                const double duty = positive / n;
                logMessage ("  pw " + juce::String (pw, 2) + " -> duty medido "
                            + juce::String (duty, 3));

                expectWithinAbsoluteError (duty, (double) pw, 0.05,
                                           "el duty no sigue al ancho de pulso");
            }
        }

        beginTest ("el aliasing del triangulo esta por debajo del de la sierra");
        {
            /*  El triangulo va SIN correccion de banda limitada a proposito: sus
                armonicos caen como 1/n^2 en vez de 1/n. Este test verifica que
                esa decision se sostiene con datos. Si alguna vez el triangulo
                aliasa mas que la sierra con PolyBLEP, hay que agregar polyBLAMP. */
            const double f = 3729.1;   // inconmensurable con 48 kHz
            constexpr int n = 8192;

            auto measure = [&] (Waveform w)
            {
                Oscillator osc;
                osc.setSampleRate (48000.0);
                osc.setWaveform (w);
                osc.setFrequency ((float) f);
                osc.reset (0.0f);

                std::vector<double> out ((size_t) n);

                for (int i = 0; i < n; ++i)
                    out[(size_t) i] = osc.process();

                return aliasEnergyDb (out, f, 48000.0);
            };

            const double sawAlias = measure (Waveform::saw);
            const double triAlias = measure (Waveform::triangle);

            logMessage ("  sierra (PolyBLEP): " + juce::String (sawAlias, 1) + " dB");
            logMessage ("  triangulo (ingenuo): " + juce::String (triAlias, 1) + " dB");

            expect (triAlias <= sawAlias,
                    "el triangulo ingenuo aliasa MAS que la sierra con PolyBLEP ("
                        + juce::String (triAlias, 1) + " vs " + juce::String (sawAlias, 1)
                        + " dB): ya no se justifica dejarlo sin polyBLAMP");
        }

        beginTest ("la tabla de organo cambia de mipmap sin saltos de volumen");
        {
            // Sin normalizar cada tabla, el volumen saltaria al cruzar de una a
            // otra subiendo por el teclado, y eso se escucha como un escalon.
            double previousRms = -1.0;

            for (int note = 36; note <= 108; note += 2)
            {
                Oscillator osc;
                osc.setSampleRate (48000.0);
                osc.setWaveform (Waveform::organ);
                osc.setFrequency (440.0f * std::pow (2.0f, ((float) note - 69.0f) / 12.0f));
                osc.reset (0.0f);

                double sum = 0.0;
                constexpr int n = 4096;

                for (int i = 0; i < n; ++i)
                {
                    const double v = osc.process();
                    sum += v * v;
                }

                const double rms = std::sqrt (sum / n);

                if (previousRms > 0.0)
                {
                    const double ratioDb = 20.0 * std::log10 (rms / previousRms);

                    expect (std::abs (ratioDb) < 3.0,
                            "salto de volumen de " + juce::String (ratioDb, 1)
                                + " dB en la nota " + juce::String (note));
                }

                previousRms = rms;
            }
        }
    }
};

//==============================================================================
class FilterTests : public juce::UnitTest
{
public:
    FilterTests() : juce::UnitTest ("SvfTpt", "Character") {}

    void runTest() override
    {
        beginTest ("el pasabajos atenua por encima del cutoff");
        {
            constexpr double sr = 48000.0;
            constexpr float cutoff = 1000.0f;

            StereoLadder filter;
            filter.setType (FilterType::lowpass);
            filter.setSlope (StereoLadder::Slope::db12);
            filter.setResonance (0.0f);

            const auto g = gForCutoff (cutoff, sr);
            const auto c1 = filter.coeffsStage1 (g);
            const auto c2 = StereoLadder::coeffsStage2 (g);

            auto responseDb = [&] (double freq)
            {
                filter.reset();

                constexpr int n = 16384;
                std::vector<double> out ((size_t) n);

                for (int i = 0; i < n; ++i)
                {
                    float l = (float) std::sin (2.0 * kPi * freq * i / sr);
                    float r = l;
                    filter.process (l, r, c1, c2);
                    out[(size_t) i] = l;
                }

                // Se descarta el transitorio inicial.
                std::vector<double> steady (out.begin() + 4096, out.end());
                return 20.0 * std::log10 (magnitudeAt (steady, freq, sr) + 1.0e-12);
            };

            const double atCutoff = responseDb (cutoff);
            const double oneOct   = responseDb (cutoff * 2.0);
            const double twoOct   = responseDb (cutoff * 4.0);
            const double threeOct = responseDb (cutoff * 8.0);

            logMessage ("  12 dB/oct: cutoff " + juce::String (atCutoff, 1)
                        + " dB, +1 oct " + juce::String (oneOct, 1)
                        + " dB, +2 oct " + juce::String (twoOct, 1)
                        + " dB, +3 oct " + juce::String (threeOct, 1) + " dB");

            /*  La pendiente se mide en la region ASINTOTICA, de +2 a +3 octavas.
                Entre el cutoff y +1 octava un filtro de 12 dB/oct cae ~8 dB, no
                12: todavia esta en el codo. La primera version de este test
                medía ahí y fallaba por 0.1 dB contra un filtro correcto. */
            const double asymptoticSlope = twoOct - threeOct;

            expect (asymptoticSlope > 9.0 && asymptoticSlope < 15.0,
                    "la pendiente asintotica no es de 12 dB/oct: "
                        + juce::String (asymptoticSlope, 1) + " dB");

            expect (oneOct < atCutoff - 4.0, "no atenua por encima del cutoff");
        }

        beginTest ("24 dB/oct cae el doble que 12 dB/oct");
        {
            constexpr double sr = 48000.0;
            constexpr float cutoff = 1000.0f;

            auto slopeFor = [&] (StereoLadder::Slope s)
            {
                StereoLadder filter;
                filter.setType (FilterType::lowpass);
                filter.setSlope (s);
                filter.setResonance (0.0f);

                const auto g = gForCutoff (cutoff, sr);
                const auto c1 = filter.coeffsStage1 (g);
                const auto c2 = StereoLadder::coeffsStage2 (g);

                auto responseDb = [&] (double freq)
                {
                    filter.reset();

                    constexpr int n = 16384;
                    std::vector<double> out ((size_t) n);

                    for (int i = 0; i < n; ++i)
                    {
                        float l = (float) std::sin (2.0 * kPi * freq * i / sr);
                        float r = l;
                        filter.process (l, r, c1, c2);
                        out[(size_t) i] = l;
                    }

                    std::vector<double> steady (out.begin() + 4096, out.end());
                    return 20.0 * std::log10 (magnitudeAt (steady, freq, sr) + 1.0e-12);
                };

                return responseDb (cutoff * 2.0) - responseDb (cutoff * 4.0);
            };

            const double s12 = slopeFor (StereoLadder::Slope::db12);
            const double s24 = slopeFor (StereoLadder::Slope::db24);

            logMessage ("  caida por octava: 12 dB/oct -> " + juce::String (s12, 1)
                        + " dB,  24 dB/oct -> " + juce::String (s24, 1) + " dB");

            expect (s24 > s12 * 1.6,
                    "24 dB/oct no cae bastante mas que 12 dB/oct");
        }

        beginTest ("la auto-oscilacion queda acotada");
        {
            /*  Con k -> 0 el SVF queda marginalmente estable. En un filtro
                analogico la saturacion del circuito acota la oscilacion; aca
                hay que ponerla explicitamente o crece sin limite. */
            StereoLadder filter;
            filter.setType (FilterType::lowpass);
            filter.setSlope (StereoLadder::Slope::db24);
            filter.setResonance (1.0f);
            filter.reset();

            const auto g = gForCutoff (2000.0f, 48000.0);
            const auto c1 = filter.coeffsStage1 (g);
            const auto c2 = StereoLadder::coeffsStage2 (g);

            float peak = 0.0f;
            juce::Random rng (0x5F11);

            for (int i = 0; i < 48000 * 4; ++i)
            {
                // Un impulso fuerte al principio para excitar la resonancia.
                float l = (i < 64) ? rng.nextFloat() * 2.0f - 1.0f : 0.0f;
                float r = l;

                filter.process (l, r, c1, c2);

                if (i > 4800)   // pasado el transitorio
                    peak = juce::jmax (peak, std::abs (l));

                if (! std::isfinite (l))
                {
                    expect (false, "el filtro diverge a NaN/inf en la muestra " + juce::String (i));
                    return;
                }
            }

            logMessage ("  pico de auto-oscilacion tras 4 s: " + juce::String (peak, 3));

            /*  DOS condiciones, y las dos importan:

                SE SOSTIENE: es lo que pide el brief. La primera version usaba
                kMinDamping = 0.02 (Q = 50) y la oscilacion se apagaba en 8 ms;
                el test media pico 0.000 y eso no es auto-oscilar, es resonar.

                QUEDA ACOTADA: con k = 0 el filtro es marginalmente estable y sin
                saturador la oscilacion crece hasta desbordar. */
            expect (peak > 0.01f,
                    "la resonancia al maximo no se sostiene: pico " + juce::String (peak, 4)
                        + ". Eso es resonancia, no auto-oscilacion");

            expect (peak < 10.0f,
                    "la auto-oscilacion no esta acotada: pico " + juce::String (peak));
        }

        beginTest ("barrer el cutoff a fondo no lo desestabiliza");
        {
            StereoLadder filter;
            filter.setType (FilterType::lowpass);
            filter.setSlope (StereoLadder::Slope::db24);
            filter.setResonance (0.9f);
            filter.reset();

            juce::Random rng (0x5A3E);

            for (int i = 0; i < 48000 * 2; ++i)
            {
                // Barrido rapido de 20 Hz a 20 kHz y vuelta, con resonancia alta.
                const float phase = (float) i / 4800.0f;
                const float cutoff = 20.0f * std::pow (1000.0f,
                                                       0.5f * (1.0f + std::sin (phase)));

                const auto g = gForCutoff (cutoff, 48000.0);
                const auto c1 = filter.coeffsStage1 (g);
                const auto c2 = StereoLadder::coeffsStage2 (g);

                float l = rng.nextFloat() * 0.4f - 0.2f;
                float r = l;
                filter.process (l, r, c1, c2);

                if (! std::isfinite (l) || std::abs (l) > 50.0f)
                {
                    expect (false, "inestable en la muestra " + juce::String (i)
                                       + " con cutoff " + juce::String (cutoff, 0)
                                       + ": " + juce::String (l));
                    return;
                }
            }

            expect (true);
        }
    }
};

static OversamplerTests oversamplerTests;
static DriveTests driveTests;
static OscillatorTests oscillatorTests;
static FilterTests filterTests;
}
