#pragma once

#include "FxUtils.h"
#include "TempoSync.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <cmath>

namespace stabr::fx
{
enum class ReverbMode
{
    plate = 0,
    room,
    hall,
    numModes
};

inline constexpr std::array<const char*, (size_t) ReverbMode::numModes> reverbModeNames
{
    "Plate", "Room", "Hall"
};

struct ReverbParams
{
    bool enabled = true;
    ReverbMode mode = ReverbMode::plate;

    float size      = 0.5f;     // 0..1
    float decay     = 0.5f;     // 0..1 -> RT60
    float damping   = 0.5f;     // 0..1, cuanto se oscurece la cola
    float modDepth  = 0.3f;     // 0..1

    bool  preDelaySync = false;
    Division preDelayDivision = Division::n1_16;
    float preDelayMs = 20.0f;

    float lowCutHz  = 150.0f;   // saca graves de la cola: el club no perdona
    float ducking   = 0.35f;
    float mix       = 0.2f;
};

//==============================================================================
/*  Reverb FDN 8x8 con matriz de Hadamard. Ver DECISIONS.md D-016.

    POR QUE FDN Y NO DATTORRO: Dattorro es mas barato y suena a plate de
    lujo, pero es UNA topologia. Aca hacen falta tres modos, size, ducking y
    modulacion desde un solo motor, y el FDN escala en tamano de forma limpia:
    se cambian los largos de las lineas y las ganancias salen de una formula de
    RT60.

    POR QUE HADAMARD: es la matriz unitaria mas barata que existe. Con la
    mariposa rapida son 24 sumas y restas para mezclar las ocho lineas, sin una
    sola multiplicacion. Una matriz densa costaria 64 multiplicaciones.

    RIESGO DECLARADO EN D-016: un FDN mal parametrizado hace flutter metalico.
    Las tres mitigaciones estan puestas: largos primos entre si, modulacion
    independiente por linea, y damping en cada realimentacion.

    Al frente va un difusor de allpasses tipo Dattorro: el FDN solo tarda en
    construir densidad, y sin el, los primeros milisegundos suenan a ecos
    sueltos en vez de a reverb.
*/
class Reverb
{
public:
    static constexpr int kNumLines = 8;
    static constexpr int kNumDiffusers = 4;

    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;

        // Escala respecto de las longitudes de referencia, medidas a 48 kHz.
        const double rateScale = sampleRate / 48000.0;

        for (int i = 0; i < kNumLines; ++i)
        {
            const int maxLength = (int) std::ceil (kBaseLengths[(size_t) i]
                                                     * rateScale * kMaxSizeScale) + 8;
            lines[(size_t) i].assign ((size_t) maxLength, 0.0f);
            lineSize[(size_t) i] = maxLength;
            lineWrite[(size_t) i] = 0;
            damp[(size_t) i].setSampleRate (sampleRate);
        }

        for (int i = 0; i < kNumDiffusers; ++i)
        {
            const int length = (int) std::ceil (kDiffuserLengths[(size_t) i] * rateScale) + 4;
            diffuser[(size_t) i].assign ((size_t) length, 0.0f);
            diffuserSize[(size_t) i] = length;
            diffuserIndex[(size_t) i] = 0;
        }

        const int preDelayMax = (int) std::ceil (kMaxPreDelaySeconds * sampleRate) + 8;
        preDelayL.assign ((size_t) preDelayMax, 0.0f);
        preDelayR.assign ((size_t) preDelayMax, 0.0f);
        preDelaySize = preDelayMax;

        lowCutL.setSampleRate (sampleRate);
        lowCutR.setSampleRate (sampleRate);
        duckFollower.setSampleRate (sampleRate);
        duckFollower.setTimes (1.0f, 180.0f);

        modInc = (float) (kModHz / sampleRate);

        reset();
    }

    void reset() noexcept
    {
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);

        for (auto& d : diffuser)
            std::fill (d.begin(), d.end(), 0.0f);

        std::fill (preDelayL.begin(), preDelayL.end(), 0.0f);
        std::fill (preDelayR.begin(), preDelayR.end(), 0.0f);

        for (auto& d : damp)
            d.reset();

        lowCutL.reset();
        lowCutR.reset();
        duckFollower.reset();

        preDelayIndex = 0;
        modPhase = 0.0f;
    }

    void setParams (const ReverbParams& p, double bpm) noexcept
    {
        params = p;

        const double rateScale = sampleRate / 48000.0;

        /*  El modo cambia la escala de los largos: un plate es un espacio
            chico y muy denso, un hall es grande y mas espaciado. */
        const float modeScale = kModeScale[(size_t) std::clamp ((int) p.mode, 0,
                                                                (int) ReverbMode::numModes - 1)];
        const float sizeScale = modeScale * (0.4f + 1.2f * std::clamp (p.size, 0.0f, 1.0f));

        // RT60 de 0.2 a 12 segundos, con curva exponencial para que el control
        // sea util en todo su recorrido.
        const float rt60 = 0.2f * std::pow (60.0f, std::clamp (p.decay, 0.0f, 1.0f));

        for (int i = 0; i < kNumLines; ++i)
        {
            const int wanted = (int) (kBaseLengths[(size_t) i] * rateScale * sizeScale);
            lineDelay[(size_t) i] = std::clamp (wanted, 8, lineSize[(size_t) i] - 4);

            /*  Ganancia por linea desde el RT60. Cada linea tiene largo
                distinto, asi que necesita su propia ganancia para que TODAS
                decaigan en el mismo tiempo. Usar una sola ganancia para las
                ocho haria que las cortas se apaguen antes y la cola cambie de
                color mientras decae. */
            const double lengthSeconds = (double) lineDelay[(size_t) i] / sampleRate;
            lineGain[(size_t) i] = (float) std::pow (10.0, -3.0 * lengthSeconds / rt60);
            lineGain[(size_t) i] = std::clamp (lineGain[(size_t) i], 0.0f, 0.9995f);

            // Damping: mas alto = cola mas oscura. De 18 kHz a 900 Hz.
            const float dampHz = 18000.0f * std::pow (0.05f, std::clamp (p.damping, 0.0f, 1.0f));
            damp[(size_t) i].setCutoff (dampHz);
        }

        lowCutL.setCutoff (p.lowCutHz);
        lowCutR.setCutoff (p.lowCutHz);

        const float preSeconds = p.preDelaySync ? divisionToSeconds (p.preDelayDivision, bpm)
                                                : p.preDelayMs * 0.001f;
        preDelaySamples = std::clamp ((int) (preSeconds * sampleRate),
                                      1, preDelaySize - 2);

        maxModSamples = 6.0f * (float) rateScale * std::clamp (p.modDepth, 0.0f, 1.0f);
    }

    //==========================================================================
    inline void process (float& l, float& r) noexcept
    {
        if (! params.enabled)
            return;

        const float dryL = l;
        const float dryR = r;

        const float duckEnv = duckFollower.process ((dryL + dryR) * 0.5f);
        const float duckGain = 1.0f - params.ducking * std::min (1.0f, duckEnv * 3.0f);

        //----------------------------------------------------------------
        // Pre-delay.
        //----------------------------------------------------------------
        preDelayL[(size_t) preDelayIndex] = dryL;
        preDelayR[(size_t) preDelayIndex] = dryR;

        const int readIdx = (preDelayIndex - preDelaySamples + preDelaySize) % preDelaySize;
        float input = (preDelayL[(size_t) readIdx] + preDelayR[(size_t) readIdx]) * 0.5f;

        preDelayIndex = (preDelayIndex + 1) % preDelaySize;

        //----------------------------------------------------------------
        // Difusion de entrada: cuatro allpasses en serie.
        //----------------------------------------------------------------
        for (int i = 0; i < kNumDiffusers; ++i)
            input = processAllpass (i, input);

        //----------------------------------------------------------------
        // Lectura de las ocho lineas, con modulacion independiente.
        //----------------------------------------------------------------
        modPhase += modInc;
        if (modPhase >= 1.0f)
            modPhase -= 1.0f;

        std::array<float, kNumLines> v {};

        for (int i = 0; i < kNumLines; ++i)
        {
            /*  Cada linea tiene su propia fase de modulacion. Si todas se
                movieran juntas, la modulacion sonaria a vibrato sobre la cola
                entera en vez de a aire. */
            const float phase = modPhase + (float) i / (float) kNumLines;
            const float mod = std::sin ((phase - std::floor (phase))
                                            * juce::MathConstants<float>::twoPi)
                                * maxModSamples;

            v[(size_t) i] = readLine (i, (float) lineDelay[(size_t) i] + mod);
        }

        // Salida estereo: mitad de las lineas a cada lado, intercaladas.
        float wetL = 0.0f, wetR = 0.0f;

        for (int i = 0; i < kNumLines; ++i)
        {
            if ((i & 1) == 0) wetL += v[(size_t) i];
            else              wetR += v[(size_t) i];
        }

        constexpr float outputNorm = 0.35f;
        wetL *= outputNorm;
        wetR *= outputNorm;

        //----------------------------------------------------------------
        // Realimentacion: damping, matriz de Hadamard, ganancia por linea.
        //----------------------------------------------------------------
        for (int i = 0; i < kNumLines; ++i)
            v[(size_t) i] = damp[(size_t) i].processLowpass (v[(size_t) i]);

        hadamard (v);

        for (int i = 0; i < kNumLines; ++i)
        {
            const float fed = v[(size_t) i] * lineGain[(size_t) i] + input;
            writeLine (i, loopSaturate (fed));
        }

        //----------------------------------------------------------------
        // Salida.
        //----------------------------------------------------------------
        /*  Corte de graves en la cola. Un filtro POR CANAL: compartir uno solo
            entre los dos mezclaria sus estados y colapsaria el estereo, que es
            justo lo que la reverb tiene que dar. */
        wetL = lowCutL.processHighpass (wetL);
        wetR = lowCutR.processHighpass (wetR);

        wetL *= duckGain;
        wetR *= duckGain;

        l = dryL + wetL * params.mix;
        r = dryR + wetR * params.mix;
    }

private:
    /*  Transformada rapida de Hadamard, en el lugar.

        Tres etapas de mariposas: 24 sumas y restas, cero multiplicaciones
        (salvo el escalado final). Una matriz densa 8x8 costaria 64
        multiplicaciones y 56 sumas. */
    static inline void hadamard (std::array<float, kNumLines>& v) noexcept
    {
        for (int stride = 1; stride < kNumLines; stride *= 2)
        {
            for (int i = 0; i < kNumLines; i += stride * 2)
            {
                for (int j = i; j < i + stride; ++j)
                {
                    const float a = v[(size_t) j];
                    const float b = v[(size_t) (j + stride)];
                    v[(size_t) j] = a + b;
                    v[(size_t) (j + stride)] = a - b;
                }
            }
        }

        // 1/sqrt(8): la mantiene unitaria, o sea que no agrega ni saca energia.
        constexpr float norm = 0.35355339f;

        for (auto& x : v)
            x *= norm;
    }

    inline float readLine (int index, float delaySamples) const noexcept
    {
        const auto& line = lines[(size_t) index];
        const int size = lineSize[(size_t) index];

        float pos = (float) lineWrite[(size_t) index] - delaySamples;

        while (pos < 0.0f)
            pos += (float) size;

        const int i0 = (int) pos % size;
        const int i1 = (i0 + 1) % size;
        const float frac = pos - std::floor (pos);

        return line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
    }

    inline void writeLine (int index, float x) noexcept
    {
        auto& line = lines[(size_t) index];
        const int size = lineSize[(size_t) index];

        line[(size_t) lineWrite[(size_t) index]] = x;
        lineWrite[(size_t) index] = (lineWrite[(size_t) index] + 1) % size;
    }

    inline float processAllpass (int index, float x) noexcept
    {
        auto& line = diffuser[(size_t) index];
        const int size = diffuserSize[(size_t) index];
        int& idx = diffuserIndex[(size_t) index];

        const float delayed = line[(size_t) idx];
        const float v = x - kDiffuserGain * delayed;

        line[(size_t) idx] = v;
        idx = (idx + 1) % size;

        return kDiffuserGain * v + delayed;
    }

    //==========================================================================
    /*  Largos base en muestras a 48 kHz, PRIMOS ENTRE SI.

        Es la mitigacion principal del flutter metalico: si dos lineas tienen
        largos con un divisor comun, sus ecos coinciden periodicamente y eso se
        escucha como un tono. Con primos, las coincidencias se reparten. */
    static constexpr std::array<int, kNumLines> kBaseLengths
    {
        1153, 1319, 1531, 1693, 1873, 2069, 2251, 2399
    };

    static constexpr std::array<int, kNumDiffusers> kDiffuserLengths
    {
        142, 379, 107, 277
    };

    static constexpr float kDiffuserGain = 0.625f;
    static constexpr float kMaxSizeScale = 2.0f;
    static constexpr float kMaxPreDelaySeconds = 0.5f;
    static constexpr double kModHz = 0.31;

    // plate chico y denso, room medio, hall grande.
    static constexpr std::array<float, (size_t) ReverbMode::numModes> kModeScale
    {
        0.55f, 1.0f, 1.7f
    };

    std::array<std::vector<float>, kNumLines> lines;
    std::array<int, kNumLines> lineSize {};
    std::array<int, kNumLines> lineWrite {};
    std::array<int, kNumLines> lineDelay {};
    std::array<float, kNumLines> lineGain {};
    std::array<OnePoleTpt, kNumLines> damp;

    std::array<std::vector<float>, kNumDiffusers> diffuser;
    std::array<int, kNumDiffusers> diffuserSize {};
    std::array<int, kNumDiffusers> diffuserIndex {};

    std::vector<float> preDelayL, preDelayR;
    int preDelaySize = 0;
    int preDelayIndex = 0;
    int preDelaySamples = 1;

    OnePoleTpt lowCutL, lowCutR;
    EnvelopeFollower duckFollower;

    ReverbParams params;
    double sampleRate = 44100.0;
    float modInc = 0.0f, modPhase = 0.0f;
    float maxModSamples = 3.0f;
};
}
