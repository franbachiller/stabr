#pragma once

#include "DelayLine.h"
#include "FxUtils.h"
#include "TempoSync.h"

namespace stabr::fx
{
enum class DelayMode
{
    stereo = 0,   // cada canal por su cuenta
    pingPong,     // las repeticiones rebotan de un lado al otro
    dub,          // realimentacion con saturacion y pasabanda: delay de cinta
    numModes
};

inline constexpr std::array<const char*, (size_t) DelayMode::numModes> delayModeNames
{
    "Stereo", "Ping Pong", "Dub"
};

//==============================================================================
struct DelayParams
{
    bool  enabled     = true;
    DelayMode mode    = DelayMode::stereo;

    bool  tempoSync   = true;
    Division division = Division::n1_8D;   // 3/16, el clasico del dub techno
    float freeMs      = 375.0f;
    float offsetRight = 0.0f;              // -1..1, desfasa el canal derecho

    float feedback    = 0.45f;             // 0..1
    bool  freeze      = false;

    float highpassHz  = 180.0f;            // dentro del lazo
    float lowpassHz   = 6000.0f;

    float modDepth    = 0.15f;             // wow & flutter 0..1
    DelayLine::GlideMode glide = DelayLine::GlideMode::tape;

    float ducking     = 0.4f;              // 0..1
    float mix         = 0.3f;              // 0..1
};

//==============================================================================
/*  El bloque mas importante del plugin.

    Un acorde seco es solo un acorde. Lo que lo convierte en un stab de club es
    lo que pasa DESPUES: las repeticiones que se disuelven, el filtro del lazo
    que las va oscureciendo, y el ducking que les hace lugar al golpe seco.

    DUCKING (no es un extra): sin el, cada repeticion se superpone con el stab
    siguiente y a los cuatro golpes hay una papilla. Con el, las repeticiones
    viven en los huecos. Es la diferencia entre un delay que suena bien solo y
    uno que suena bien en una mezcla.
*/
class Delay
{
public:
    void prepare (double sr, int /*maxBlockSize*/)
    {
        sampleRate = sr;

        lineL.prepare (sr);
        lineR.prepare (sr);

        for (auto* f : { &hpL, &hpR, &lpL, &lpR, &bandL, &bandR })
            f->setSampleRate (sr);

        // Semillas distintas y fijas: reproducible, pero L y R no correlacionados.
        wowL.prepare (sr, 0x5741);
        wowR.prepare (sr, 0x7C2B);

        duckFollower.setSampleRate (sr);
        duckFollower.setTimes (1.0f, 140.0f);

        maxModSamples = (float) (0.004 * sr);   // +-4 ms de bamboleo maximo

        reset();
    }

    void reset() noexcept
    {
        lineL.reset();
        lineR.reset();

        for (auto* f : { &hpL, &hpR, &lpL, &lpR, &bandL, &bandR })
            f->reset();

        wowL.reset();
        wowR.reset();
        duckFollower.reset();

        feedbackL = 0.0f;
        feedbackR = 0.0f;
    }

    void setParams (const DelayParams& p, double bpm) noexcept
    {
        params = p;

        const float seconds = p.tempoSync ? divisionToSeconds (p.division, bpm)
                                          : p.freeMs * 0.001f;

        /*  El offset desfasa el canal derecho hasta un 30%. Es lo que le da
            ancho al delay sin recurrir a un chorus: las dos repeticiones caen
            en momentos distintos y el oido las separa. */
        const float offset = 1.0f + 0.3f * std::clamp (p.offsetRight, -1.0f, 1.0f);

        lineL.setGlideMode (p.glide);
        lineR.setGlideMode (p.glide);
        lineL.setDelaySeconds (seconds);
        lineR.setDelaySeconds (seconds * offset);

        hpL.setCutoff (p.highpassHz);
        hpR.setCutoff (p.highpassHz);
        lpL.setCutoff (p.lowpassHz);
        lpR.setCutoff (p.lowpassHz);

        // El pasabanda del modo dub vive dentro del lazo y es lo que hace que
        // cada repeticion pierda graves y agudos, como una cinta gastada.
        bandL.setCutoff (kDubBandHz);
        bandR.setCutoff (kDubBandHz);
    }

    /*  dryL/dryR entran como la senal seca; l/r salen ya mezclados. */
    inline void process (float& l, float& r) noexcept
    {
        if (! params.enabled)
            return;

        const float dryL = l;
        const float dryR = r;

        //----------------------------------------------------------------
        // Ducking: se mide la senal SECA, no la mezcla. Medir la mezcla
        // realimentaria el ducking consigo mismo.
        //----------------------------------------------------------------
        const float duckEnv = duckFollower.process ((dryL + dryR) * 0.5f);
        const float duckGain = 1.0f - params.ducking * std::min (1.0f, duckEnv * 3.0f);

        //----------------------------------------------------------------
        // Modulacion del puntero de lectura.
        //----------------------------------------------------------------
        lineL.advanceGlide();
        lineR.advanceGlide();

        const float modL = wowL.process (params.modDepth, maxModSamples);
        const float modR = wowR.process (params.modDepth, maxModSamples);

        float wetL = lineL.read (modL);
        float wetR = lineR.read (modR);

        //----------------------------------------------------------------
        // Lazo de realimentacion: filtros y, en modo dub, saturacion.
        //
        // EN FREEZE LOS FILTROS SE SALTEAN. Cada pasada por el HP y el LP saca
        // un poco de energia en los bordes de la banda; en uso normal eso es
        // justamente lo que hace que las repeticiones se disuelvan, pero en
        // freeze el material da cientos de vueltas y las perdidas se acumulan
        // hasta apagarlo. El test lo midio: se caia a RMS 0.0002 en 30 s.
        // Un freeze tiene que SOSTENER, no filtrar lentamente hasta el silencio.
        //----------------------------------------------------------------
        float fbL = wetL;
        float fbR = wetR;

        if (! params.freeze)
        {
            fbL = hpL.processHighpass (fbL);
            fbR = hpR.processHighpass (fbR);

            fbL = lpL.processLowpass (fbL);
            fbR = lpR.processLowpass (fbR);

            if (params.mode == DelayMode::dub)
            {
                fbL = loopSaturate (bandL.processLowpass (fbL) * kDubDrive);
                fbR = loopSaturate (bandR.processLowpass (fbR) * kDubDrive);
            }
        }

        /*  FREEZE: el lazo se cierra del todo y la entrada se corta.

            Con realimentacion 1.0 exacta el lazo seria marginalmente estable y
            cualquier asimetria numerica lo haria crecer. Por eso se usa 0.999 y
            ademas el saturador del lazo le pone techo: el freeze se sostiene
            indefinidamente pero NO puede desbordar. */
        const float fbAmount = params.freeze ? 0.999f
                                             : std::clamp (params.feedback, 0.0f, 0.98f);
        const float inputGain = params.freeze ? 0.0f : 1.0f;

        if (params.mode == DelayMode::pingPong)
        {
            /*  Ping pong: la entrada entra por la izquierda y la realimentacion
                cruza. Asi la primera repeticion suena a la izquierda, la
                segunda a la derecha, y sigue rebotando. */
            const float monoIn = (dryL + dryR) * 0.5f * inputGain;

            lineL.write (loopSaturate (monoIn + fbR * fbAmount));
            lineR.write (loopSaturate (fbL * fbAmount));
        }
        else
        {
            lineL.write (loopSaturate (dryL * inputGain + fbL * fbAmount));
            lineR.write (loopSaturate (dryR * inputGain + fbR * fbAmount));
        }

        //----------------------------------------------------------------
        // Mezcla.
        //----------------------------------------------------------------
        wetL *= duckGain;
        wetR *= duckGain;

        l = dryL + wetL * params.mix;
        r = dryR + wetR * params.mix;
    }

    /*  Para el visualizador de la Fase 6: tiempo actual de cada canal. */
    float getDelaySamplesL() const noexcept { return lineL.getCurrentDelaySamples(); }
    float getDelaySamplesR() const noexcept { return lineR.getCurrentDelaySamples(); }
    float getFeedback() const noexcept { return params.freeze ? 0.999f : params.feedback; }

private:
    static constexpr float kDubBandHz = 2200.0f;
    static constexpr float kDubDrive  = 1.6f;

    DelayLine lineL, lineR;
    OnePoleTpt hpL, hpR, lpL, lpR, bandL, bandR;
    WowFlutter wowL, wowR;
    EnvelopeFollower duckFollower;

    DelayParams params;
    double sampleRate = 44100.0;
    float maxModSamples = 100.0f;
    float feedbackL = 0.0f, feedbackR = 0.0f;
};
}
