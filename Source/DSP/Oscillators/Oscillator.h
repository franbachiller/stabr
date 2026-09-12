#pragma once

#include "PolyBlepOsc.h"
#include "OrganTable.h"

namespace stabr
{
enum class Waveform
{
    saw = 0,
    pulse,
    triangle,
    organ,
    sine,
    numWaveforms
};

inline constexpr std::array<const char*, (size_t) Waveform::numWaveforms> waveformNames
{
    "Saw", "Pulse", "Triangle", "Organ", "Sine"
};

//==============================================================================
/*  Envuelve las dos implementaciones (PolyBLEP y wavetable) detras de una sola
    interfaz. Mantiene las dos fases sincronizadas al cambiar de forma, para que
    un cambio de waveform en medio de una nota no produzca un salto.
*/
class Oscillator
{
public:
    void setSampleRate (double sr) noexcept
    {
        blep.setSampleRate (sr);
        organ.setSampleRate (sr);
    }

    void setFrequency (float hz) noexcept
    {
        blep.setFrequency (hz);
        organ.setFrequency (hz);
    }

    void setWaveform (Waveform w) noexcept
    {
        if (w == waveform)
            return;

        waveform = w;

        // La senoidal reusa la ultima tabla del mipmap, que tiene un solo
        // armonico. Asi el sub no paga un std::sin por muestra.
        if (w == Waveform::sine)
            organ.pinTableIndex (OrganTable::kMaxHarmonics - 1);
        else
            organ.unpinTable();
    }

    void setPulseWidth (float pw) noexcept { pulseWidth = pw; }

    void reset (float phase01) noexcept
    {
        blep.reset (phase01);
        organ.reset (phase01);
    }

    inline float process() noexcept
    {
        switch (waveform)
        {
            case Waveform::saw:      return blep.processSaw();
            case Waveform::pulse:    return blep.processPulse (pulseWidth);
            case Waveform::triangle: return blep.processTriangle();
            case Waveform::organ:    return organ.process();
            case Waveform::sine:     return organ.process();

            case Waveform::numWaveforms:
            default:                 return 0.0f;
        }
    }

private:
    PolyBlepOsc blep;
    OrganOsc organ;
    Waveform waveform  = Waveform::saw;
    float pulseWidth   = 0.5f;
};
}
