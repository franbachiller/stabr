#include "Voice.h"
#include <cmath>

namespace stabr
{
namespace
{
    inline float centsToRatio (float cents) noexcept
    {
        return std::pow (2.0f, cents / 1200.0f);
    }

    inline float midiToHz (float midiNote) noexcept
    {
        return 440.0f * std::pow (2.0f, (midiNote - 69.0f) / 12.0f);
    }
}

//==============================================================================
void Voice::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;

    for (auto& u : unison)
    {
        u.osc1.setSampleRate (sampleRate);
        u.osc2.setSampleRate (sampleRate);
    }

    subOsc.setSampleRate (sampleRate);

    ampEnv.setSampleRate (sampleRate);
    filterEnv.setSampleRate (sampleRate);


    level.reset (sampleRate, 0.005);
    reset();
}

void Voice::reset() noexcept
{
    ampEnv.reset();
    filterEnv.reset();
    filter.reset();
    driveL.reset();
    driveR.reset();

    for (auto& u : unison)
    {
        u.osc1.reset (0.0f);
        u.osc2.reset (0.0f);
    }

    subOsc.reset (0.0f);

    level.setCurrentAndTargetValue (0.0f);

    active     = false;
    stealing   = false;
    stealGain  = 1.0f;
    stealDelta = 0.0f;
    note       = -1;
    chordId    = -1;
    samplesUntilControlUpdate = 0;
}

//==============================================================================
void Voice::updateUnisonLayout() noexcept
{
    unisonCount = juce::jlimit (1, kMaxUnison, unisonCount);

    for (int i = 0; i < unisonCount; ++i)
    {
        // offset en -1..1, simetrico alrededor del centro.
        const float offset = (unisonCount == 1)
                                 ? 0.0f
                                 : (2.0f * (float) i / (float) (unisonCount - 1) - 1.0f);

        auto& u = unison[(size_t) i];
        u.detuneRatio = centsToRatio (offset * params.unisonDetuneCents);

        // Paneo de potencia constante: sin esto, el centro suena mas fuerte
        // que los extremos y el unisono se escucha desbalanceado.
        const float pan = juce::jlimit (-1.0f, 1.0f, offset * params.unisonWidth);
        const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        u.panL = std::cos (angle);
        u.panR = std::sin (angle);
    }

    // Normalizacion por raiz: sumar N copias descorrelacionadas sube el nivel
    // como sqrt(N), no como N. Dividir por N dejaria el unisono mudo.
    unisonNormalise = 1.0f / std::sqrt ((float) unisonCount);
}

void Voice::updatePitch() noexcept
{
    const float base = (float) note + params.pitchBendSemitones;
    const float baseHz = midiToHz (base);

    const float osc2Hz = midiToHz (base + 12.0f * (float) params.osc2Octave)
                             * centsToRatio (params.osc2DetuneCents);

    for (int i = 0; i < unisonCount; ++i)
    {
        auto& u = unison[(size_t) i];
        u.osc1.setFrequency (baseHz * u.detuneRatio);
        u.osc2.setFrequency (osc2Hz * u.detuneRatio);
    }

    subOsc.setFrequency (midiToHz (base + 12.0f * (float) params.subOctave));
}

//==============================================================================
void Voice::setParams (const VoiceParams& p) noexcept
{
    params = p;

    ampEnv.setParameters (p.ampAttack, p.ampDecay, p.ampSustain, p.ampRelease);
    filterEnv.setParameters (p.filtAttack, p.filtDecay, p.filtSustain, p.filtRelease);

    for (int i = 0; i < unisonCount; ++i)
    {
        auto& u = unison[(size_t) i];
        u.osc1.setWaveform (p.osc1Wave);
        u.osc2.setWaveform (p.osc2Wave);
        u.osc1.setPulseWidth (p.pulseWidth);
        u.osc2.setPulseWidth (p.pulseWidth);
    }

    subOsc.setWaveform (p.subWave);
    subActive = p.subLevel > 0.0001f;

    filter.setResonance (p.resonance);
    filter.setType (p.filterType);
    filter.setSlope (p.slope);
    filter.setPeakCompensation (p.peakCompensation);

    driveL.setAmount (p.driveAmount);
    driveR.setAmount (p.driveAmount);
    driveL.setHighQuality (p.highQuality);
    driveR.setHighQuality (p.highQuality);

    updateUnisonLayout();

    if (active)
        updatePitch();

    level.setTargetValue (velocityGain);
}

//==============================================================================
void Voice::noteOn (int midiNote, float vel, int chordIdIn, std::uint64_t stamp,
                    const VoiceParams& p, int unisonOverride) noexcept
{
    note         = midiNote;
    chordId      = chordIdIn;
    startStamp   = stamp;
    velocity01   = juce::jlimit (0.0f, 1.0f, vel);
    velocityGain = velocity01;
    active       = true;
    stealing     = false;
    stealGain    = 1.0f;
    stealDelta   = 0.0f;

    // El presupuesto global de osciladores puede recortar el unisono pedido.
    // Ver DECISIONS.md D-014.
    unisonCount = juce::jlimit (1, kMaxUnison,
                                unisonOverride > 0 ? unisonOverride : p.unisonCount);

    params = p;
    updateUnisonLayout();

    /*  Fase inicial. Fija = el transitorio del stab es identico en cada
        disparo, que es lo que hace que un stab suene "apretado". Aleatoria =
        el unisono suena mas gordo y menos como un flanger, a costa de que el
        ataque cambie un poco en cada nota. */
    for (int i = 0; i < unisonCount; ++i)
    {
        const float phase1 = p.unisonRandomPhase ? random.nextFloat()
                                                 : (float) i / (float) kMaxUnison;
        const float phase2 = p.unisonRandomPhase ? random.nextFloat() : phase1;

        unison[(size_t) i].osc1.reset (phase1);
        unison[(size_t) i].osc2.reset (phase2);
    }

    subOsc.reset (0.0f);

    filter.reset();
    driveL.reset();
    driveR.reset();

    setParams (p);
    updatePitch();

    ampEnv.noteOn();
    filterEnv.noteOn();

    /*  Los coeficientes arrancan en su valor exacto, no en cero: si no, el
        filtro abriria desde 10 Hz en las primeras 16 muestras de cada nota y
        eso se escucha como un "pop" al principio del ataque. */
    samplesUntilControlUpdate = kControlRateSamples;
    const float startG = gForCutoff (computeCutoff (0.0f), sampleRate);
    coeffs1 = filter.coeffsStage1 (startG);
    coeffs2 = StereoLadder::coeffsStage2 (startG);
    step1 = {};
    step2 = {};
}

void Voice::noteOff() noexcept
{
    ampEnv.noteOff();
    filterEnv.noteOff();
}

void Voice::startSteal() noexcept
{
    if (! active || stealing)
        return;

    stealing   = true;
    stealGain  = 1.0f;
    const auto fadeSamples = juce::jmax (1.0f, (float) (kStealFadeMs * 0.001 * sampleRate));
    stealDelta = 1.0f / fadeSamples;
}

//==============================================================================
float Voice::computeCutoff (float filterEnvValue) const noexcept
{
    /*  Toda la modulacion se suma en OCTAVAS y despues se exponencia una sola
        vez. Sumar en Hz daria saltos musicalmente inconsistentes: +1000 Hz es
        una octava en los graves y un semitono en los agudos. */
    float octaves = params.envAmount * VoiceParams::kEnvOctaves * filterEnvValue;

    octaves += params.keyTrack * ((float) note - 60.0f) / 12.0f;
    octaves += params.velAmount * VoiceParams::kVelOctaves * velocity01;

    return params.cutoffHz * std::pow (2.0f, octaves);
}

//==============================================================================
void Voice::renderAdd (float* left, float* right, int numSamples) noexcept
{
    if (! active)
        return;

    /*  Se evaluan UNA vez por bloque, no por muestra: un oscilador en cero no
        tiene por que costar nada. Con el default (osc2 en 0, sub en 0) esto
        solo saca el 40% del trabajo de la seccion de osciladores. */
    const bool osc1Active = params.osc1Level > 0.0001f;
    const bool osc2Active = params.osc2Level > 0.0001f;

    for (int i = 0; i < numSamples; ++i)
    {
        //------------------------------------------------------------------
        // Envolventes.
        //------------------------------------------------------------------
        const float ampValue = ampEnv.getNextSample();
        const float filtValue = filterEnv.getNextSample();

        if (stealing)
        {
            stealGain -= stealDelta;

            if (stealGain <= 0.0f)
            {
                stealGain = 0.0f;
                active    = false;
                stealing  = false;
                ampEnv.reset();
                filterEnv.reset();
                return;
            }
        }
        else if (! ampEnv.isActive())
        {
            // La envolvente termino en cero exacto: la voz se libera.
            active = false;
            return;
        }

        //------------------------------------------------------------------
        // Filtro: coeficientes a tasa de control, interpolados por muestra.
        //
        // tan() y la division de los coeficientes se pagan una vez cada
        // kControlRateSamples y se comparten entre los dos canales. Antes se
        // hacian cuatro divisiones por muestra para obtener cuatro veces el
        // mismo resultado: era el costo dominante del filtro.
        //------------------------------------------------------------------
        if (samplesUntilControlUpdate <= 0)
        {
            const float targetG = gForCutoff (computeCutoff (filtValue), sampleRate);

            const auto target1 = filter.coeffsStage1 (targetG);
            const auto target2 = StereoLadder::coeffsStage2 (targetG);

            constexpr float inverseSteps = 1.0f / (float) kControlRateSamples;
            step1 = SvfCoeffs::stepBetween (coeffs1, target1, inverseSteps);
            step2 = SvfCoeffs::stepBetween (coeffs2, target2, inverseSteps);

            coeffs1.k = target1.k;
            coeffs2.k = target2.k;

            samplesUntilControlUpdate = kControlRateSamples;
        }

        coeffs1.addStep (step1);
        coeffs2.addStep (step2);
        --samplesUntilControlUpdate;

        //------------------------------------------------------------------
        // Osciladores + unisono.
        //------------------------------------------------------------------
        float l = 0.0f, r = 0.0f;

        for (int u = 0; u < unisonCount; ++u)
        {
            auto& copy = unison[(size_t) u];

            float s = 0.0f;

            if (osc1Active)
                s += copy.osc1.process() * params.osc1Level;

            if (osc2Active)
                s += copy.osc2.process() * params.osc2Level;

            l += s * copy.panL;
            r += s * copy.panR;
        }

        l *= unisonNormalise;
        r *= unisonNormalise;

        if (subActive)
        {
            // El sub va al centro: un grave panoramizado se pierde en mono y
            // no perdona en un sistema de club.
            const float sub = subOsc.process() * params.subLevel;
            l += sub;
            r += sub;
        }

        //------------------------------------------------------------------
        // Drive -> filtro -> amplitud.
        //------------------------------------------------------------------
        l = driveL.process (l);
        r = driveR.process (r);

        filter.process (l, r, coeffs1, coeffs2);

        const float gain = ampValue * level.getNextValue() * stealGain;

        left[i]  += l * gain;
        right[i] += r * gain;
    }
}
}
