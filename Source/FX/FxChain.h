#pragma once

#include "Saturation.h"
#include "Chorus.h"
#include "Delay.h"
#include "Reverb.h"
#include "Pump.h"
#include "StereoOutput.h"

namespace stabr::fx
{
/*  Orden de los tres bloques reordenables.

    El brief pide poder reordenar saturacion / chorus / delay. Los otros tres
    (reverb, pump y salida) quedan fijos al final, y hay motivo:

      - la REVERB va despues del delay porque lo natural es que las
        repeticiones del delay tambien entren a la sala, no al reves;
      - el PUMP va despues de todo el tiempo porque tiene que duckear TAMBIEN
        las colas: si fuera antes, la reverb seguiria sonando durante el bombo
        y el efecto no se notaria;
      - la SALIDA (ancho, graves en mono, limitador) es por definicion lo
        ultimo.
*/
enum class FxOrder
{
    satChorusDelay = 0,
    satDelayChorus,
    chorusSatDelay,
    chorusDelaySat,
    delaySatChorus,
    delayChorusSat,
    numOrders
};

inline constexpr std::array<const char*, (size_t) FxOrder::numOrders> fxOrderNames
{
    "Sat > Cho > Dly",
    "Sat > Dly > Cho",
    "Cho > Sat > Dly",
    "Cho > Dly > Sat",
    "Dly > Sat > Cho",
    "Dly > Cho > Sat"
};

//==============================================================================
struct FxParams
{
    FxOrder order = FxOrder::satChorusDelay;

    SaturationParams saturation;
    ChorusParams     chorus;
    DelayParams      delay;
    ReverbParams     reverb;
    PumpParams       pump;
    StereoOutputParams output;
};

//==============================================================================
class FxChain
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        saturation.prepare (sampleRate);
        chorus.prepare (sampleRate);
        delay.prepare (sampleRate, maxBlockSize);
        reverb.prepare (sampleRate);
        pump.prepare (sampleRate);
        output.prepare (sampleRate);
    }

    void reset() noexcept
    {
        saturation.reset();
        chorus.reset();
        delay.reset();
        reverb.reset();
        pump.reset();
        output.reset();
    }

    void setParams (const FxParams& p, double bpm) noexcept
    {
        params = p;

        saturation.setParams (p.saturation);
        chorus.setParams (p.chorus);
        delay.setParams (p.delay, bpm);
        reverb.setParams (p.reverb, bpm);
        pump.setParams (p.pump);
        output.setParams (p.output);
    }

    void setTransport (double ppq, double bpm, bool playing, bool valid) noexcept
    {
        pump.setTransport (ppq, bpm, playing, valid);
    }

    inline void process (float& l, float& r) noexcept
    {
        switch (params.order)
        {
            case FxOrder::satChorusDelay: saturation.process (l, r); chorus.process (l, r); delay.process (l, r); break;
            case FxOrder::satDelayChorus: saturation.process (l, r); delay.process (l, r);  chorus.process (l, r); break;
            case FxOrder::chorusSatDelay: chorus.process (l, r);     saturation.process (l, r); delay.process (l, r); break;
            case FxOrder::chorusDelaySat: chorus.process (l, r);     delay.process (l, r);  saturation.process (l, r); break;
            case FxOrder::delaySatChorus: delay.process (l, r);      saturation.process (l, r); chorus.process (l, r); break;
            case FxOrder::delayChorusSat: delay.process (l, r);      chorus.process (l, r); saturation.process (l, r); break;

            case FxOrder::numOrders:
            default:                      saturation.process (l, r); chorus.process (l, r); delay.process (l, r); break;
        }

        reverb.process (l, r);

        // El pump duckea TODO, colas incluidas. Ver la nota en FxOrder.
        const float pumpGain = pump.processSample();
        l *= pumpGain;
        r *= pumpGain;

        output.process (l, r);
    }

    /*  Para los visualizadores de la Fase 6. */
    const Delay& getDelay() const noexcept { return delay; }
    StereoOutput& getOutput() noexcept { return output; }

private:
    FxParams params;

    Saturation saturation;
    Chorus chorus;
    Delay delay;
    Reverb reverb;
    Pump pump;
    StereoOutput output;
};
}
