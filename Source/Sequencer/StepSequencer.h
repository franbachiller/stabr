#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "Patterns.h"

namespace stabr::seq
{
struct SeqParams
{
    bool      enabled = false;
    PatternId pattern = PatternId::offbeat8;
    StepRate  rate    = StepRate::n1_16;
    int       length  = kNumSteps;   // 1..16
    float     swing   = 0.5f;        // 0.5 (recto) .. 0.75
    float     gate    = 0.5f;        // fraccion del (sub)paso que la nota queda apretada
    Pattern   custom {};
};

/*  Lo que informa el host al comienzo del bloque. */
struct Transport
{
    double bpm         = 120.0;
    double ppq         = 0.0;
    bool   isPlaying   = false;
    bool   hasPosition = false;   // false = no hay playhead (Standalone): corre libre
};

//==============================================================================
/*  Secuenciador de 16 pasos alineado al transporte del host.

    SIN RELOJ PROPIO. Cada disparo se calcula a partir de la posicion en negras
    que informa el host, como funcion pura de esa posicion: el paso N empieza en
    N * largoDelPaso (+ swing si es impar). Por eso:

      - arranca en el lugar correcto aunque el transporte empiece a mitad de
        compas, y se realinea solo si el host salta (loop, click en la regla);
      - el resultado NO depende del tamano de bloque: los bloques son
        intervalos semiabiertos [inicio, fin) contiguos, y cada disparo cae en
        exactamente uno;
      - un render offline es identico al de tiempo real.

    El unico estado que se arrastra entre bloques es la nota abierta (para
    poder cerrarla en el bloque en que vence) y la posicion esperada (para
    distinguir un bloque contiguo de un salto del transporte).

    Emite eventos de gate con su muestra exacta. QUE suena en cada gate lo
    decide el procesador (latch, modo de nota); el secuenciador solo sabe
    CUANDO. Ver DECISIONS.md D-039.
*/
class StepSequencer
{
public:
    static constexpr int kMaxEvents = 256;

    struct Event
    {
        enum class Type { gateOn, gateOff };

        Type  type     = Type::gateOn;
        int   offset   = 0;
        float velocity = 0.0f;
        int   step     = 0;   // indice dentro del patron (solo gateOn)
    };

    struct EventList
    {
        std::array<Event, (size_t) kMaxEvents> events {};
        int count = 0;

        void clear() noexcept { count = 0; }

        void add (const Event& e) noexcept
        {
            if (count < kMaxEvents)
                events[(size_t) count++] = e;
        }

        const Event& operator[] (int i) const noexcept { return events[(size_t) i]; }
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setParams (const SeqParams& p) noexcept { params = p; }

    /*  latencySamples: la latencia que el plugin le declara al host (modo
        Auto). El host adelanta la salida esa cantidad, asi que los disparos
        tienen que generarse esa cantidad MAS TARDE para sonar en el tiempo. */
    void process (const Transport& transport, int numSamples, int latencySamples,
                  EventList& out) noexcept;

    bool isRunning() const noexcept  { return running; }
    bool isGateOpen() const noexcept { return gateOpen; }

    /*  Paso que esta sonando, para la GUI de la Fase 6. -1 = parado. */
    int getCurrentStep() const noexcept { return currentStep.load (std::memory_order_relaxed); }

    //==========================================================================
    /*  Donde empieza el paso stepIndex, en negras desde el comienzo del tema.

        Swing: los pasos impares se atrasan. Con swing s, el primer paso del par
        ocupa la fraccion s del par: 50 % es recto, 75 % deja el impar a media
        distancia del siguiente (la "negra con puntillo + semicorchea" del
        shuffle). */
    static double stepOnsetBeats (std::int64_t stepIndex, double stepBeats, float swing) noexcept;

    /*  Tirada de dados de la probabilidad, deterministica: depende SOLO del
        indice absoluto del paso. Cambia en cada vuelta del patron, pero el
        mismo compas del tema da siempre el mismo resultado, en tiempo real,
        offline y con cualquier tamano de bloque. */
    static bool passesProbability (std::int64_t stepIndex, float probability) noexcept;

private:
    const Step& stepAt (std::int64_t stepIndex) const noexcept;
    int patternIndexOf (std::int64_t stepIndex) const noexcept;

    SeqParams params;
    double sampleRate = 44100.0;

    bool   running  = false;
    bool   gateOpen = false;
    double gateOffBeats = 0.0;
    double expectedStartBeats = 0.0;
    double freeRunBeats = 0.0;

    std::atomic<int> currentStep { -1 };
};
}
