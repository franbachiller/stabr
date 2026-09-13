#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

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

    bool operator== (const DelayViewInfo& o) const noexcept
    {
        return std::abs (timeL - o.timeL) < 1.0e-4f && std::abs (timeR - o.timeR) < 1.0e-4f
            && std::abs (feedback - o.feedback) < 1.0e-4f && std::abs (mix - o.mix) < 1.0e-4f
            && mode == o.mode && enabled == o.enabled && freeze == o.freeze;
    }
};

struct Echo
{
    float time = 0.0f;        // segundos desde el golpe
    int   channel = 0;        // 0 = L, 1 = R
    float gain = 1.0f;        // relativa a la primera repeticion
};

//==============================================================================
/*  Visualizador de delay (direccion A): cada repeticion es un punto en el
    tiempo (horizontal) y en el estereo (carril L arriba, R abajo).

    Las repeticiones se calculan con el MISMO modelo que Delay.h: estereo con
    el offset del canal derecho, ping-pong que entra por la izquierda y cruza
    la realimentacion. Cada golpe real que llega por el FIFO las enciende en su
    momento, asi que lo que se ve es lo que va a sonar.
*/
class DelayVisualizer final : public juce::Component
{
public:
    static constexpr int kMaxEchoes = 24;

    DelayVisualizer();

    static int computeEchoes (const DelayViewInfo&, std::array<Echo, kMaxEchoes>& out) noexcept;

    void setInfo (const DelayViewInfo&, const juce::String& caption);
    void onset (float velocity, double now);
    void tick (double now);

    void paint (juce::Graphics&) override;

    int getNumEchoes() const noexcept { return numEchoes; }
    float getWindowSeconds() const noexcept { return windowSeconds(); }
    const Echo& getEcho (int i) const noexcept { return echoes[(size_t) juce::jlimit (0, kMaxEchoes - 1, i)]; }
    const DelayViewInfo& getInfo() const noexcept { return info; }

private:
    float windowSeconds() const noexcept;

    DelayViewInfo info;
    juce::String caption;
    std::array<Echo, kMaxEchoes> echoes {};
    int numEchoes = 0;

    double onsetTime = -100.0;
    double now = 0.0;
    float velocity = 1.0f;
    bool dirty = true;
    bool wasAnimating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayVisualizer)
};
}
