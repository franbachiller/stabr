# DECISIONS — STABR

Decisiones técnicas con su porqué. Se agrega una entrada cuando la decisión
tiene alternativas reales y alguien (incluido yo dentro de tres meses) podría
preguntarse por qué está hecha así.

Formato: `D-NNN` · fase · decisión · alternativa descartada · motivo.

---

## D-000 · Fase 1 · Identidad del producto

- Nombre: **STABR**
- Desarrollador: **Narfbach**
- `PLUGIN_MANUFACTURER_CODE`: `Nrfb`
- `PLUGIN_CODE`: `Stbr`
- `BUNDLE_ID`: `com.narfbach.stabr`

**Estos identificadores son permanentes.** Cambiar cualquiera rompe todos los
proyectos guardados de todos los usuarios: el host busca el plugin por estos
códigos, no por nombre. No se tocan nunca más.

---

## D-001 · Fase 1 · Motor de voces propio, no `juce::Synthesiser`

**Decisión:** `VoiceManager` + `Voice` propios, con pool fijo.

**Alternativa:** `juce::Synthesiser` / `juce::SynthesiserVoice`.

**Motivo:** en `juce::Synthesiser` una voz es una nota y no existe el concepto
de grupo. STABR necesita que **N voces sean un acorde que nace y muere junto**:
el robo de voces tiene que robar acordes enteros. Robarle la séptima a un min7
para meter la fundamental del acorde siguiente mutila la armonía y suena a bug,
no a limitación de polifonía.

Por eso `Voice` ya tiene un campo `chordId` en la Fase 1, aunque el generador
de acordes recién llegue en la Fase 2: la estructura tiene que estar antes de
que haga falta.

Costo: reimplementar el dispatch de MIDI por sub-bloque (~40 líneas en
`processBlock`). Barato comparado con pelearse con la clase base.

---

## D-002 · Fase 1 · JUCE por `FetchContent` con tag pinneado

**Decisión:** `FetchContent` apuntando al tag exacto `8.0.15`.

**Alternativas:** submódulo de git; JUCE instalado en el sistema; seguir `master`.

**Motivo:** el tag fijo hace el build reproducible y evita que una release nueva
de JUCE rompa el build a mitad de una fase. `FetchContent` sobre submódulo
porque no obliga a nadie a acordarse de `--recursive`, y la versión queda
declarada en un archivo de texto (`cmake/FetchJUCE.cmake`) en vez de escondida
en el estado de git.

Se eligió la línea **8.x y no la 9.x** aunque JUCE 9 ya existe: el brief pide
JUCE 8, la 8.0.15 es madura y toda la documentación y el material de la
comunidad todavía apuntan ahí. Migrar a 9 es una decisión aparte, para después
de que el producto funcione.

---

## D-003 · Fase 1 · Warnings agresivos solo en targets propios

**Decisión:** `/W4 /permissive-` en MSVC y `-Wall -Wextra -Wpedantic` en
clang/gcc, aplicados **solo** a `STABR` y `STABR_Tests`. `-Werror` detrás de la
opción `STABR_WERROR`, apagada por defecto.

**Motivo:** `-Wall -Wextra` no existe en MSVC; el equivalente práctico es `/W4`.
Y JUCE genera warnings propios que no vamos a parchear: activarlos globalmente
convierte el build en ruido. `-Werror` queda opcional para no romper el build
local por un warning nuevo de una versión distinta de compilador, pero se
enciende en CI.

---

## D-004 · Fase 1 · PolyBLEP para las formas analógicas, wavetable para el órgano

**Decisión:** híbrido.
- Sierra, pulso con PWM y triángulo → **PolyBLEP** (`PolyBlepOsc`).
- Forma tipo organ/M1 → **wavetable band-limited mip-mapped** (Fase 3).

**Alternativas:** todo PolyBLEP; todo wavetable; DPW.

**Motivo:**
- PolyBLEP es gratis en memoria, da **PWM exacta y continua** (una wavetable
  con PWM obliga a interpolar entre tablas o a sintetizarlas en tiempo real) y
  no pierde nada por interpolación. Su residuo de aliasing (~−60/−70 dB en el
  registro medio) queda enterrado bajo el filtro y la saturación.
- La forma organ/M1 es una **suma fija de armónicos**: PolyBLEP no la puede
  generar. Necesita wavetable sí o sí.
- DPW se descartó: es más barato para sierra pero se lleva mal con PWM y con
  hard sync, y los dos están en el roadmap.

La wavetable se genera **una sola vez al arrancar el plugin**, `const` y
compartida entre todas las voces (~80 KB). Nunca en `prepareToPlay` ni en el
hilo de audio.

**Riesgo asumido:** PolyBLEP en notas muy agudas (>C7) con detune fuerte alíasa
más que una wavetable. Mitigación si molesta: oversampling ×2 del oscilador
sólo por encima de cierta nota. Medir en Fase 3 antes de decidir.

---

## D-005 · Fase 1 · ADSR exponencial propio, no `juce::ADSR`

**Decisión:** `AdsrExp`, un polo por etapa con target overshoot.

**Motivo:** `juce::ADSR` es lineal, y un decay lineal no suena a stab — el golpe
percusivo vive enteramente en la curva.

Detalle que importa: el release apunta a `-targetRatioDR` (**negativo**), así
que **cruza el cero en tiempo finito** y la voz termina con salida exactamente
`0.0f`. Una exponencial que apunta a cero nunca llega y te deja denormals
colgando para siempre. Hay un test específico para esto.

Además, con `sustain == 0` (el default del plugin, porque un stab es corto y
seco) la envolvente pasa directo a `Idle` al terminar el decay: la voz se
libera sin esperar un note-off. Sin esto, un stab con sustain 0 ocupa una voz
del pool hasta que soltás la tecla.

---

## D-006 · Fase 1 · Fade de 5 ms en el robo de voces, con headroom en el pool

**Decisión:** el pool tiene `kMaxPolyphony * 2` voces. La voz robada hace un
fade lineal de 5 ms y recién ahí vuelve al pool.

**Motivo:** cortar una voz de golpe hace click. Pero si el pool tuviera
exactamente el tamaño de la polifonía, la voz en fade seguiría ocupando lugar y
cada robo forzaría el siguiente, en cascada. El headroom lo resuelve sin
costo (una voz inactiva no procesa nada).

Prioridad de robo: primero la voz en release más vieja (ya la están soltando,
es lo menos audible), después la voz sonando más vieja.

---

## D-007 · Fase 1 · Silencio exacto por detección de cola (`SilenceGate`)

**Decisión:** cuando no hay voces activas y el pico del bloque está bajo
−120 dBFS durante ~200 ms consecutivos, el buffer se fuerza a cero exacto.

**Motivo:** el brief pide salida **exactamente 0.0** en reposo. En la Fase 1
sale casi gratis porque la envolvente termina en cero, pero desde la Fase 4 las
colas de delay y reverb decaen asintóticamente y **nunca llegan a cero**. Hay
que detectarlas y cortarlas a mano. Beneficio extra: ahorra CPU en reposo.

El gate existe desde la Fase 1 aunque todavía no haga falta, para que el test
de silencio sea válido en todas las fases y no haya que reescribirlo.

---

## D-008 · Fase 1 · Render por sub-bloques entre eventos MIDI

**Decisión:** `processBlock` parte el bloque en los `samplePosition` de cada
evento MIDI y renderiza cada tramo por separado.

**Motivo:** sin esto, una nota disparada a mitad de bloque suena cuantizada al
tamaño del buffer — hasta 43 ms de error con buffer 2048. En un instrumento
percusivo el timing es literalmente el producto.

---

## D-009 · Fase 1 · `stateVersion` desde la v1

**Decisión:** el `ValueTree` serializado lleva la propiedad `stateVersion`, y
`state::migrate()` corre siempre al cargar, aunque hoy sea identidad.

**Motivo:** agregar el gancho después significa que los proyectos guardados con
la v1 no tienen número de versión y no se pueden distinguir de nada. El
`switch` con fallthrough deliberado permite que un estado viejo suba escalón por
escalón hasta el formato actual.

---

## D-010 · Fase 1 · Licencias: JUCE Starter + VST3 MIT, código propietario

**Decisión:** JUCE bajo el tier comercial **Starter** (gratis hasta USD 20.000
anuales, permite código cerrado). Código de STABR propietario, **no** AGPL.

**Hallazgo relevante:** el SDK VST3 embebido en JUCE 8.0.15 es **MIT** desde
2025. Ya no hace falta firmar el acuerdo propietario de Steinberg ni
registrarse, y desapareció la incompatibilidad histórica entre JUCE copyleft y
el SDK propietario.

**Consecuencia dura:** no entra al árbol **ningún** código GPL/AGPL de terceros.
Una sola función contamina el binario entero. Detalle completo en
[LICENSING.md](LICENSING.md).

---

## D-018 · Fase 1 · La v1 sale VST3 / Windows. AU queda fuera de alcance.

**Decisión:** el entregable de la v1 es **VST3 en Windows x64** (más Standalone
para debug). El target AU sigue declarado en el CMake pero sólo se activa en
macOS, y se documenta explícitamente como **no verificado**.

**Motivo:** no hay acceso a una mac en este proyecto. AU no se puede compilar ni
testear en Windows — el SDK de AudioUnit viene con Xcode y no existe fuera de
macOS. Un target que nunca se compila no está "pendiente": está roto y todavía
no lo sabemos. Prometerlo en el README sería mentir.

Consecuencia comercial que hay que tener presente: macOS es una porción grande
del mercado de producción musical. Salir sólo en Windows es una decisión de
negocio, no un detalle técnico.

**Caminos para revertirlo más adelante** (evaluar en Fase 7, no ahora):

- **Runners macOS en CI** (GitHub Actions y equivalentes) — compilan y corren
  pluginval sin tener la máquina. Es lo más barato. Limitación real: se puede
  verificar que compila, carga y valida, pero **no se puede escuchar**, y un
  plugin de audio que nadie escuchó en la plataforma destino no está probado.
- **Mac alquilada por hora** (MacStadium, MacinCloud) para las sesiones de
  verificación auditiva.
- **Mac mini usado**, que a la larga sale más barato que lo anterior.

En los tres casos hace falta además una cuenta de **Apple Developer (USD 99/año)**
para firmar y notarizar; sin notarización, Gatekeeper bloquea el plugin en las
máquinas de los usuarios. Detalle en [LICENSING.md](LICENSING.md).

---

## D-019 · Fase 1 · `DiscreteIntParameter`: `juce::AudioParameterInt` se exporta como continuo

**Decisión:** todo parámetro entero de STABR usa `stabr::DiscreteIntParameter`,
una subclase de `juce::AudioParameterInt` que sólo agrega
`bool isDiscrete() const override { return true; }`.

**El problema:** `juce::AudioParameterInt` sobreescribe `getNumSteps()` pero
**no** `isDiscrete()`, que queda en el `false` de la clase base. El wrapper VST3
de JUCE decide el `stepCount` así
(`juce_audio_plugin_client_VST3.cpp`, `updateParameterInfo()`):

```cpp
if (! param.isDiscrete())
    return 0;
```

Es decir: **todo `AudioParameterInt` llega al host como parámetro continuo.**
El valor se redondea bien puertas adentro, pero el host dibuja un control
continuo y automatiza en rampa suave donde deberían verse escalones.

**Cómo apareció:** leyendo el log de pluginval, no por un fallo. pluginval
reportaba `numSteps - 2147483647, isDiscrete - 0` para `Polyphony` (1..16) y
`Pitch Bend Range` (0..24). No lo marca como error porque es legal — es un
defecto de UX, no de conformidad. Después del arreglo reporta `numSteps 16` y
`numSteps 25`, ambos `isDiscrete 1`.

Es seguro cambiar un `AudioParameterInt` ya existente por este: no toca el ID,
ni el rango, ni el mapeo de valores, así que no rompe estados guardados.

---

## Decisiones tomadas y pendientes de implementar

Confirmadas con el usuario, se implementan en la fase que corresponde:

| Ref | Fase | Decisión |
|---|---|---|
| D-011 | 2 | **Scale lock con dos modos:** `Diatonic` (default — el selector de acorde define densidad/intención y la calidad la dicta la escala, nunca suena fuera de tonalidad) y `Literal` (el acorde es exacto, la escala sólo cuantiza la fundamental). |
| D-012 | 2 | **Modo de nota explícito:** `Chord` (default) / `Passthrough` / `Auto`. `Auto` usa una ventana de recolección de ~20 ms y declara esa latencia; no es el default porque 20 ms de latencia al tocar en vivo es inaceptable. |
| D-013 | 3 | **Drive por voz con oversampling ×2 (polyphase IIR)**, no ×4 FIR. La saturación global sí va ×4 FIR. Switch `HQ Mode` sube el drive por voz a ×4 para render offline. Motivo: ×4 por voz con 8 voces × 7 unísonos es CPU que no rinde; la fase no lineal del IIR es irrelevante porque cada voz se filtra y satura por separado. **A revisar con mediciones reales en Fase 3.** |
| D-014 | 3 | **Presupuesto duro de osciladores** (arranca en 128). Cuando un acorde lo excede se baja el unísono por voz (7→5→3) en vez de tirar notas: perder densidad de detune es imperceptible, perder la novena del acorde es catastrófico. |
| D-015 | 4 | **Línea de delay propia**, no `juce::dsp::DelayLine`: hace falta un punto de inserción dentro del lazo de feedback (HP/LP/saturación/band-pass del modo dub) y un puntero de lectura con slew propio para el pitch drift de cinta. Interpolación **Lagrange 3er orden** (lineal modula el brillo con el wow&flutter; allpass hace ruido con el puntero en movimiento). Dos comportamientos al cambiar el tiempo: `Tape` (slew, con drift) por defecto y `Digital` (crossfade). |
| D-016 | 4 | **Reverb FDN 8×8 con matriz Hadamard** + difusor de entrada tipo Dattorro, en vez de Dattorro puro. Un solo motor cubre plate/room/hall escalando largos de línea y ganancias desde RT60. Riesgo conocido: un FDN mal parametrizado hace flutter metálico — mitigación con largos primos entre sí, modulación independiente por línea y damping por realimentación. Plan B si no suena: Dattorro para plate + FDN para room/hall. |
| D-017 | 5 | **El secuenciador latchea:** apretás una tecla, la soltás, y sigue disparando ese acorde. Si no hay nada apretado, cae a la fundamental de la tonalidad. Sin esto no se cumple la promesa de sacar un stab en 30 segundos. |
