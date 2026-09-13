#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

#include "../FX/Delay.h"

namespace stabr::gui
{
struct DelayViewInfo
{
    float timeL = 0.375f;     // segundos reales, con glide
    float timeR = 0.375f;
    float feedback = 0.45f;
    float mix = 0.3f;
    int   mode = 0;           // fx::DelayMode
    bool  enabled = true;
    bool  freeze = false;
};

struct Echo
{
    float time = 0.0f;        // segundos desde el golpe
    int   channel = 0;        // 0 = L, 1 = R
    float gain = 1.0f;        // relativa a la primera repeticion
};

inline constexpr int kMaxEchoes = 24;

/*  Las repeticiones que va a producir el delay, con el MISMO modelo que
    Delay.h: estereo con el offset del canal derecho, y ping-pong que entra por
    la izquierda y cruza la realimentacion. Lo usa la pantalla ECHO. */
inline int computeEchoes (const DelayViewInfo& in, std::array<Echo, kMaxEchoes>& out) noexcept
{
    constexpr float kAudible = 0.03f;   // -30 dB respecto de la primera repeticion

    const float fb = in.freeze ? 0.999f : juce::jlimit (0.0f, 0.98f, in.feedback);
    const float tL = juce::jmax (0.001f, in.timeL);
    const float tR = juce::jmax (0.001f, in.timeR);
    int n = 0;

    if (in.mode == (int) fx::DelayMode::pingPong)
    {
        double t = 0.0;

        for (int k = 0; n < kMaxEchoes; ++k)
        {
            const float gain = std::pow (fb, (float) k);

            if (gain < kAudible)
                break;

            const int channel = k % 2;
            t += channel == 0 ? tL : tR;
            out[(size_t) n++] = { (float) t, channel, gain };
        }
    }
    else
    {
        for (int k = 0; n + 1 < kMaxEchoes; ++k)
        {
            const float gain = std::pow (fb, (float) k);

            if (gain < kAudible)
                break;

            out[(size_t) n++] = { (float) (k + 1) * tL, 0, gain };
            out[(size_t) n++] = { (float) (k + 1) * tR, 1, gain };
        }
    }

    return n;
}

inline float echoWindowSeconds (const std::array<Echo, kMaxEchoes>& echoes, int count) noexcept
{
    float maxTime = 0.0f;

    for (int i = 0; i < count; ++i)
        maxTime = juce::jmax (maxTime, echoes[(size_t) i].time);

    return juce::jmax (0.2f, maxTime * 1.08f);
}
}
