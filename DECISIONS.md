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

## D-020 · Fase 2 · Polifonía = disparos simultáneos, no voces

**Decisión:** el parámetro `polyphony` pasa de "1–16 voces" a **"1–8 disparos"**
(acordes en modo Chord, notas sueltas en Passthrough). Default 6.

**Motivo:** con un min9 de 5 notas, "8 voces" no le dice nada al usuario — no
sabe si eso son dos acordes o uno y medio. **"Cuántos stabs se pueden
superponer"** sí es una cantidad que se entiende y se decide. El pool de voces
se dimensiona solo: 8 acordes × 7 notas + holgura.

El ID `polyphony` se mantiene, pero **cambió el rango**, o sea que rompe estados
guardados. Se puede hacer ahora porque nada está publicado; después de la v1
esto sería inaceptable y habría que migrarlo en `state::migrate()`.

---

## D-021 · Fase 2 · Scale lock diatónico apilando GRADOS, no semitonos

**Decisión:** en modo `Diatonic`, el acorde se arma apilando **grados de la
escala** (0, 2, 4, 6… = terceras diatónicas), no semitonos fijos.

**Motivo:** de ahí sale sola la calidad correcta en cada grado, sin una tabla de
casos especiales. Apilar terceras sobre el grado I de menor natural da min7;
sobre el III da maj7; sobre el VII da un dominante. Verificado: los siete grados
de La menor dan Am7, Bm7♭5, Cmaj7, Dm7, Em7, Fmaj7, G7 — una tecla cada uno.

El selector de acorde define entonces **densidad e intención** (¿tríada?
¿séptima? ¿novena? ¿oncena?) y la tonalidad define la calidad. Esa es la
promesa del scale lock: toques la tecla que toques, no puede sonar fuera de
tonalidad. Hay un test que lo verifica sobre las 5 escalas × 12 tonalidades ×
11 tipos × 25 notas.

### Dos excepciones deliberadas

`power` y `dim7` se marcan `alwaysLiteral`: en modo Diatonic se les cuantiza la
fundamental, pero el acorde se arma con la receta literal.

- **power** es una quinta justa por definición. Apilado por grados daría un
  tritono en los grados de quinta disminuida (VII en mayor, II en frigio). Eso
  no es un power chord con otro color: es un acorde roto.
- **dim7** es simétrico (0-3-6-9) y no existe como acorde diatónico salvo en un
  grado puntual del menor armónico. Forzarlo a la escala lo destruye.

Los dos son acordes cuya identidad *son* sus intervalos, no su función.

---

## D-022 · Fase 2 · Al reducir densidad se saca la quinta, no la nota más aguda

**Decisión:** cuando la densidad pedida es menor que las notas del acorde, se
quita primero la **quinta** (índice 2 de la receta) y recién después se recorta
por arriba.

**Motivo:** recortar desde arriba le arranca la séptima a un min7 y lo deja como
tríada menor — o sea, **otro acorde**. La quinta es la nota más prescindible:
no define ni la calidad ni la extensión. Un min7 a 3 voces da `fundamental, ♭3,
♭7`, que es el voicing de jazz de toda la vida, en vez de una tríada aburrida.

---

## D-023 · Fase 2 · La densidad mínima es 2, no 3

**Decisión:** `kMinDensity = 2`.

**Motivo:** apareció al escribir el test de las recetas literales, que pedía
`power` (2 notas) y recibía 3. El mínimo de 3 hacía **imposible** un power chord
de fundamental + quinta, que es el sonido de rave por excelencia, y también las
díadas de techno. Era una restricción arbitraria que le sacaba sonidos al
producto sin dar nada a cambio.

---

## Decisiones tomadas y pendientes de implementar

Confirmadas con el usuario, se implementan en la fase que corresponde:

| Ref | Fase | Decisión |
|---|---|---|
| ~~D-011~~ | ~~2~~ | **Implementada.** Terminó con *tres* modos, no dos: `Off` (sin tonalidad, acorde literal sobre la nota tocada), `Diatonic` (default) y `Literal`. Faltaba `Off`: no todo el mundo quiere cuantizar. Detalle en D-021. |
| ~~D-012~~ | ~~2~~ | **Implementada.** `Chord` (default) / `Passthrough` / `Auto`, con ventana de 20 ms y latencia declarada al host vía `NoteRouter::getLatencySamples()`. `setLatencySamples()` no se puede llamar desde el hilo de audio, así que el procesador publica el valor en un atómico y un `Timer` lo aplica en el hilo de mensajes. |
| ~~D-013~~ | ~~3~~ | **Implementada y validada con datos**, con una corrección: el filtro terminó siendo **FIR halfband generado**, no IIR polifásico (ver D-031). ×2 por voz + ×4 en HQ. La medición confirma la decisión: ×2 mejora **14 dB** de aliasing y ×4 llega a **26 dB** en agudos con drive a tope. Costo real del peor caso: 14,0 % sin drive, 41,2 % con ×2, **91,7 % con ×4**. HQ es para render offline, no para tocar en vivo, y así está documentado. |
| ~~D-014~~ | ~~3~~ | **Implementada, con dos correcciones que aparecieron al medir.** (1) El presupuesto es un techo **blando**, no duro: nunca se dejan acordes incompletos, así que el mínimo de 1 unísono por voz puede pasarlo (256 de presupuesto, 322 osciladores medidos en el peor caso). (2) **No controla el costo del drive**, que escala con VOCES y no con osciladores: subir el unísono de 1 a 7 sube de 11,1 % a 14,0 %, mientras que encender el drive salta a 41,2 %. La palanca para el drive es la polifonía, no el unísono. El presupuesto arranca en 256, no en 128: con 128 se quedaba corto para un acorde de 7 notas con unísono 7. |
| ~~D-015~~ | ~~4~~ | **Implementada y verificada.** Línea propia con Lagrange-3 y puntero deslizante. El impulso sale en la muestra **exacta** (480, 6000, 18000, 48000 medidas) y la interpolación distorsiona **−240 dB** con retardo fraccionario. Los dos modos de cambio de tiempo funcionan: `Tape` desliza (glissando) y `Digital` salta con crossfade. |
| ~~D-016~~ | ~~4~~ | **Implementada y verificada.** FDN 8×8 Hadamard con difusor Dattorro al frente. Las tres mitigaciones del flutter están puestas (largos primos entre sí, modulación independiente por línea, damping por realimentación) y **no hizo falta el plan B**: los tres modos quedan estables con decay y size al máximo (pico 0,900, sin divergencia en 15 s), el decay controla la cola de 0,43 a 5,18 s, y la salida es estéreo de verdad. |
| D-017 | 5 | **El secuenciador latchea:** apretás una tecla, la soltás, y sigue disparando ese acorde. Si no hay nada apretado, cae a la fundamental de la tonalidad. Sin esto no se cumple la promesa de sacar un stab en 30 segundos. |

---

## D-024 · Fase 2 · Todo texto no-ASCII pasa por `utf8()`

**Decisión:** existe `Source/Common/Utf8.h` con un helper `utf8(const char*)`, y
**todo** literal de texto visible que no sea ASCII de 7 bits pasa por ahí.

**El problema:** `juce::String (const char*)` **no interpreta UTF-8**. Verificado
en el código de JUCE 8.0.15 (`juce_String.cpp:307`):

```cpp
String::String (const char* const t)
    : text (StringHolderUtils::createFromCharPointer (CharPointer_ASCII (t)))
```

`CharPointer_ASCII` mapea cada byte a su mismo code point, así que los dos
bytes de `·` (`C2 B7`) se convierten en `Â` + `·`. Igual pasa con cualquier
acento: "Inversión" sale "InversiÃ³n".

**Cómo apareció:** el usuario mandó una captura de la GUI donde el título decía
`STABR Â· fase 1`. Compila sin errores y **sin warnings**: JUCE tiene un
`jassert` para este caso, pero en Release está compilado fuera, así que en un
build de Release es completamente silencioso.

**Test de regresión** en `test_processor.cpp`: recorre los nombres, unidades y
textos de valor de todos los parámetros, **y además camina el árbol de
componentes del editor** buscando U+00C2 y U+00C3, que son la firma de este
error y no aparecen en ninguna palabra real.

Dos cosas que casi hacen que el test no sirviera, y que valen como recordatorio:

1. La primera versión escribía el patrón buscado como literal `"ÂÃ"` — que
   habría tenido **exactamente el mismo bug** que busca, y nunca habría
   coincidido. Se construye por code point.
2. La primera versión sólo revisaba parámetros. El bug original estaba en un
   `Label` del editor, así que habría dejado pasar justo el caso que lo motivó.

**El test se verificó rompiendo el título a propósito y confirmando que falla**
con el mensaje `el Label en editor/0 tiene UTF-8 roto: "STABR  Â·  v0.2.0..."`.
Un test de regresión que nunca se vio fallar no prueba nada.

---

## D-025 · Fase 2 · El título del editor sale de la versión, no de un literal

**Decisión:** el placeholder muestra `JucePlugin_VersionString`, que viene del
`project(STABR VERSION ...)` del CMake. La versión pasa a **0.2.0**.

**Motivo:** decía `fase 1` hardcodeado y siguió diciéndolo después de terminar
la Fase 2 — el usuario lo notó y preguntó si el plugin se había actualizado.
Un texto que hay que acordarse de cambiar a mano siempre termina mintiendo, y
en este caso mentía sobre algo que importa: qué versión está cargada.

---

## D-026 · Fase 3 · Los coeficientes del filtro se separan del estado

**Decisión:** `SvfCoeffs` es una estructura aparte de `SvfTpt`. El filtro solo
guarda las dos variables de estado; los coeficientes llegan de afuera.

**Motivo (medido):** los dos canales de una voz tienen el mismo cutoff y la
misma resonancia, y el filtro de 24 dB tiene dos etapas. Con cada filtro
calculando lo suyo, la voz hacía **cuatro divisiones por muestra para obtener
cuatro veces el mismo resultado**. Calculándolos una vez y compartiéndolos, son
dos — y como `a1`, `a2` y `a3` son funciones suaves de `g`, se **interpolan**
linealmente entre puntos de control en vez de recalcularse, así que la división
desaparece del camino por muestra.

Junto con saltear los osciladores en nivel cero y el bypass del drive (D-027),
esto bajó el costo de 32 voces de **20,3 % a 5,9 %** de un core.

---

## D-027 · Fase 3 · El drive apagado se saltea, con el retardo compensado

**Decisión:** con `drive = 0` la señal pasa por una línea de retardo puro de
**11 muestras** en vez de por el sobremuestreador.

**Motivo:** el sobremuestreo ×2 es el bloque más caro del sintetizador y con el
drive en cero no hace absolutamente nada. Pero apagarlo a secas adelantaría la
voz 11 muestras, y **eso sí hace un salto audible** si pasa en medio de una
nota.

Las 11 muestras no son una estimación: es el retardo de grupo **medido** con un
round trip del sobremuestreador, y hay un test que verifica que los dos caminos
siguen alineados (difieren en −240 dB). Si alguien cambia el kernel del
halfband y el retardo deja de coincidir, el test falla.

---

## D-028 · Fase 3 · Dos bugs del drive que encontraron los tests

Los dos son ejemplos de por qué el test se escribe **además** del código, no
después de darlo por bueno.

### El drive amplificaba en vez de recortar

Con `kMaxBias = 0.6`, la entrada en +0,5 daba +1,0 y en **−0,5 daba −3,37**.
Eso no es "asimetría que agrega armónicos pares": es romper la forma de onda y
meterle un offset enorme al filtro que viene después.

Dos causas sumadas: el bias era demasiado grande, y la compensación normalizaba
**sólo por el extremo positivo**, dejando el negativo fuera de rango. Ahora
`kMaxBias = 0.25` y se normaliza por el mayor de los dos extremos.

### El drive en cero no era transparente

La compensación hacía que la entrada 1 diera salida 1. Pero la curva ya comprime
por su cuenta, así que para lograrlo aplicaba **+2,2 dB con el drive en CERO**, y
mover el control apenas un poco daba un salto de nivel.

Ahora sólo se atenúa, nunca se amplifica: `outputGain = extreme > 1 ? 1/extreme : 1`.
Con drive 0 eso es exactamente la identidad.

---

## D-029 · Fase 3 · Auto-oscilación real, acotada por saturación

**Decisión:** `kMinDamping = 0.0` (k = 1/Q llega a cero), y lo que acota la
oscilación es un saturador suave en el estado del pasabanda, no el
amortiguamiento.

**Motivo:** con `kMinDamping = 0.02` (Q = 50) el filtro sonaba muy resonante
pero la oscilación **se apagaba en ~8 ms**. El test que mide si se sostiene
daba pico 0,000 tras 4 segundos: eso es resonancia, no auto-oscilación, y el
brief pide auto-oscilación.

Con k = 0 el SVF queda marginalmente estable y sostiene el tono. Es exactamente
lo que hace un filtro analógico: lo que le pone techo a la oscilación es la
saturación del circuito, no el amortiguamiento. El test ahora verifica **las dos
cosas**: que se sostenga (pico > 0,01) y que quede acotada (pico < 10).

---

## D-030 · Fase 3 · El triángulo va sin corrección, y hay una medición que lo respalda

**Decisión:** `processTriangle()` es ingenuo, sin polyBLEP ni polyBLAMP.

**Motivo:** a diferencia de la sierra y el pulso, el triángulo no tiene saltos
de valor, sólo de pendiente. Sus armónicos caen como 1/n² en vez de 1/n, así que
su aliasing arranca mucho más abajo. Corregirlo requeriría polyBLAMP, que es
otra familia de residuos y más cara.

**Medido a 3729 Hz:** triángulo ingenuo **−68,3 dB**, sierra con PolyBLEP
−62,9 dB. El triángulo aliasa *menos* que la sierra ya corregida.

Hay un test que compara los dos. Si algún día el triángulo queda por encima, el
test falla y ahí sí se justifica polyBLAMP. La decisión no es "es suficiente":
es "lo medimos y la evidencia se revisa sola".

---

## D-031 · Fase 3 · Halfband FIR generado, no coeficientes IIR de memoria

**Decisión:** el sobremuestreador usa un FIR halfband de 23 taps cuyos
coeficientes se **generan** por sinc enventanado con Blackman.

**Alternativa descartada:** un IIR polifásico, que es más barato por muestra.

**Motivo:** los coeficientes de un IIR polifásico son números mágicos de una
tabla. Los de un FIR halfband se derivan de una fórmula, o sea que son
**verificables**: hay tests que miden la ganancia en continua, la simetría, el
retardo de grupo y el rechazo de imagen reales (−94 dB a 1 kHz, −82 dB a 10 kHz).
DSP delicado se escribe y después se mide.

La estructura halfband (la mitad de los coeficientes son cero) reduce cada
dirección a un FIR corto en una rama y un retardo puro en la otra. Además, los
coeficientes de los extremos quedan en ~1e-18 por la ventana y se **descartan
escaneando la tabla**, no hardcodeando índices: 12 taps pasan a 10, un 17 %
menos de trabajo en el bloque más caro del sintetizador.

> Detalle que costó tiempo: el primer intento los descartaba comparando
> `== 0.0f` y no descartaba ninguno, porque son diminutos pero no exactamente
> cero. El log del test lo delató (`coeficientes utiles: 0..11 de 12`).

---

## D-032 · Fase 3 · Cómo medir aliasing sin engañarse

Tres intentos fallidos antes de tener una medición válida. Queda escrito porque
los tres errores son fáciles de repetir:

1. **Comparar peras con manzanas.** La primera versión medía el aliasing de una
   sierra ingenua pasada por el shaper, contra el del camino con sobremuestreo
   — pero con ganancias distintas. La sierra ya aliasa por sí sola y tapaba
   todo. Ahora se compara **la misma curva con la misma ganancia sobre una
   senoidal pura**, y la curva es una función libre justamente para que el test
   pueda llamarla sin reimplementarla.

2. **Frecuencias conmensurables.** Con f = 1500 Hz a 48 kHz, los componentes
   aliaseados caen **justo encima de armónicos reales** y son indistinguibles:
   la medición daba −270 dB y parecía que no había aliasing. Hay que usar
   frecuencias inconmensurables (1414,21 · 2718,28 · 5432,11).

3. **DFT sin ventana.** La fuga espectral de los armónicos fuertes dejaba un
   piso de ~−50 dB que tapaba el aliasing. Con ventana de Blackman el piso baja
   a −100 dB y el efecto real aparece.

Recién con las tres cosas bien la medición mostró lo que dice D-013: ×2 mejora
**14 dB** y ×4 mejora **26 dB** en agudos con drive a tope.

---

## D-033 · Fase 4 · El orden de las divisiones musicales no es el intuitivo

**Decisión:** la lista de divisiones va estrictamente creciente en duración, con
tresillos y punteados **intercalados**, no agrupados por división.

**Motivo:** agruparlos (1/16T, 1/16, 1/16., después 1/8T…) parece lo natural y
**está mal**: un tresillo de corchea (0,333 negras) es más **corto** que una
semicorchea con puntillo (0,375). Con la lista agrupada, girar el control del
tiempo de delay a veces lo acortaba en vez de alargarlo.

Lo encontró el test que verifica la monotonía, no la lectura del código. El
orden correcto es:

```
1/64 · 1/32T · 1/32 · 1/16T · 1/16 · 1/8T · 1/16. · 1/8 · 1/4T · 1/8.
1/4 · 1/2T · 1/4. · 1/2 · 1/2. · 1 · 2 · 3 · 4 compases
```

`1/8.` es 3/16, EL tiempo de delay del dub techno, y hay un test que verifica
que dé exactamente lo mismo que tres semicorcheas.

---

## D-034 · Fase 4 · El freeze saltea los filtros del lazo

**Decisión:** con `freeze` activo, la realimentación del delay **no** pasa por
el pasaaltos ni el pasabajos.

**Motivo:** cada vuelta por esos filtros saca un poco de energía en los bordes
de la banda. En uso normal eso es exactamente lo que hace que las repeticiones
se disuelvan — es la razón por la que existen. Pero en freeze el material da
cientos de vueltas y las pérdidas se acumulan hasta apagarlo.

El test lo midió: con los filtros en el lazo, el freeze caía a **RMS 0,0002 en
30 segundos**. Un freeze tiene que *sostener*, no filtrar lentamente hasta el
silencio. Ahora sostiene en 0,0997 indefinidamente, acotado por el saturador
del lazo.

---

## D-035 · Fase 4 · Los graves se monofican filtrando el LATERAL, no partiendo bandas

**Decisión:** el mono de graves se hace con un pasaaltos de 12 dB/oct sobre la
señal **side** de una descomposición M/S. El **mid pasa entero, sin filtrar**.

**La versión equivocada** (el primer intento): partir en dos bandas con un
pasabajos, sumar la grave a mono y recombinar.

```cpp
low  = LP(x);  high = x - low;     // <-- esto no es un complemento limpio
```

Un filtro de un polo **desfasa**, así que `x - LP(x)` no es el complemento
exacto: en la zona del cruce las dos bandas no suman a la señal original. El
test lo midió con una señal en contrafase: a 50 Hz, con el corte en 120 Hz,
**el 38 % seguía saliendo por la banda "alta"**, o sea sin monoficar.

Filtrando sólo el lateral, el contenido mono nunca atraviesa un filtro y no hay
problema de fase. Lo único que se filtra es la diferencia entre canales, y
quitarle graves a eso es literalmente lo que se busca. Después del cambio: 0,377
de residuo a 50 Hz contra 2,545 a 2 kHz.

---

## D-036 · Fase 4 · El wow & flutter se siembra explícitamente

**Decisión:** `WowFlutter::prepare()` exige una semilla; cada canal recibe una
distinta pero **fija**.

**Motivo:** `juce::Random` por defecto se siembra con el reloj. Eso significa
que el plugin sonaría distinto en cada carga del proyecto, y que ningún render
sería reproducible.

Lo detectó el test de independencia del tamaño de buffer, que empezó a fallar
sin que nada del camino de render hubiera cambiado. Es un recordatorio útil:
un test de reproducibilidad atrapa fuentes de no-determinismo que uno ni
consideró.

Semillas distintas por canal, no una compartida: con la misma, el bamboleo
sería idéntico en L y R y sonaría mono.

---

## D-037 · Fase 4 · La cadena de efectos se saltea en silencio

**Decisión:** el `SilenceGate` se consulta **antes** de la cadena de efectos, y
si está en silencio la cadena no se procesa.

**Motivo (medido):** la cadena es estéreo, así que su costo **no depende de la
polifonía**: cuesta lo mismo con 56 voces que con ninguna. Procesarla en reposo
costaba **1,13 % de un core por instancia** produciendo exactamente nada. En un
proyecto con veinte instancias son 22 % de un core tirados.

Saltearla es seguro porque el gate sólo se activa cuando ya no hay voces **y**
el pico lleva ~200 ms por debajo de −120 dBFS: en ese punto las colas ya son
inaudibles, así que congelar su estado no produce ningún salto al volver a
sonar. La primera nota nueva levanta el gate en el mismo bloque.

Resultado: **1,130 % → 0,027 %**, 42× más barato en reposo.

---

## D-038 · Fase 4 · Qué se puede reordenar y qué no

**Decisión:** se reordenan saturación / chorus / delay (6 combinaciones). La
reverb, el pump y la etapa de salida quedan fijos al final.

**Motivo**, uno por cada bloque fijo:

- La **reverb** va después del delay porque lo natural es que las repeticiones
  del delay también entren a la sala. Al revés, el delay repetiría la reverb y
  el resultado se vuelve barro.
- El **pump** va después de todo el tiempo porque tiene que duckear **también
  las colas**. Si fuera antes, la reverb seguiría sonando durante el bombo y el
  efecto no se notaría — que es justamente lo que se quiere evitar.
- La **salida** (ancho, graves en mono, limitador) es por definición lo último.
