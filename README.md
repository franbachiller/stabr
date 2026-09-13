# STABR

Máquina de stabs de house y techno. Instrumento **VST3 para Windows x64** que
convierte una sola tecla en un acorde corto, percusivo y con carácter.

**No es un sintetizador generalista.** Cada decisión responde a una sola
pregunta: *¿esto ayuda a que alguien saque un stab que suene a club en menos de
30 segundos?*

Desarrollado por **Narfbach**.

---

## Estado

**Fase 6 completa y verificada.** STABR tiene cara propia: interfaz vectorial,
escalable de 0,75× a 2×, con visualizador de acorde y de delay animados.

Build limpio sin warnings, 2.228.617 aserciones en verde, y **pluginval
strictness 10 en SUCCESS** sobre 6 sample rates (44.1 a 192 kHz) × 7 tamaños de
buffer (32 a 2048), con 163 parámetros.

### La interfaz

Estilo hardware de estudio: grafito, un solo acento naranja, perillas con
cuerpo. Arriba y siempre a la vista, lo que más se toca: acorde y tonalidad,
cutoff / resonancia / decay, el delay y el secuenciador. El resto, en siete
pestañas.

- **Visualizador de acorde:** las notas que suenan, ubicadas por altura, así se
  ve el voicing y qué nota queda en el bajo. Arriba, el nombre **detectado** del
  acorde: con Diatonic, "Min7" en la tecla Si se muestra como lo que suena,
  Bm7b5.
- **Visualizador de delay:** cada repetición en el tiempo y en el estéreo
  (L arriba, R abajo), calculada con el mismo modelo que el delay, y encendida
  por cada golpe real.
- **Grilla de pasos:** clic enciende, arrastrar ajusta la velocity, clic
  derecho abre ratchet y probabilidad. Editar un patrón de fábrica lo copia a
  `Custom`.
- **Teclado:** todo control se alcanza con Tab; flechas para mover perillas y
  pasos, espacio para encender. Doble clic vuelve una perilla a su default.

Verificado por test: los 163 parámetros tienen control, nada se superpone en
ninguna pestaña, los 123 controles interactivos tienen nombre accesible, y el
contraste del texto secundario es 5,51:1 (WCAG pide 4,5). Detalle en
[D-044](DECISIONS.md).

### El secuenciador

Encendé **Seq On**, apretá play en el DAW y el stab suena en el contratiempo,
sin mandar una sola nota MIDI: dispara el acorde de la tónica de la tonalidad.
Tocá una tecla y soltala — el patrón pasa a disparar **ese** acorde y lo sigue
haciendo (latch). Con el transporte parado, las teclas tocan en vivo.

- **16 pasos** con on/off, velocity, probabilidad y ratchet (×2, ×3) por paso.
  Largo de 1 a 16 (polimetría) y resolución de 1/32 a 1/4, con tresillos.
- **Swing** de 50 % (recto) a 75 %, y gate por paso.
- **Patrones de fábrica:** off-beat de corcheas, 3+3+2, skippy de semicorcheas
  y sparse techno. Son de solo lectura y se pueden automatizar desde el DAW;
  `Custom` usa los 16 pasos editables.
- **Sin reloj propio:** cada disparo se calcula desde la posición del host.
  Arranca alineado aunque le des play a mitad de compás, se realinea en cada
  loop, y el render offline es idéntico al de tiempo real. La probabilidad
  también es determinística. Detalle en [D-039](DECISIONS.md).

Medido en el plugin entero, sin MIDI, 120 BPM a 48 kHz: los ocho stabs de dos
compases arrancan en las muestras 12012, 36012, 60012… — el grid exacto más
**12 muestras fijas** (0,25 ms, el retardo del camino de audio), idénticas en
los ocho.

### El motor de acordes

Con **scale lock en Diatonic** (el default), el acorde se arma apilando grados
de la escala, no semitonos. La calidad sale sola de la tonalidad — una tecla
por grado en La menor natural da:

| Grado | Tecla | Acorde |
|---|---|---|
| I | A | Am7 |
| II | B | Bm7♭5 |
| III | C | Cmaj7 |
| IV | D | Dm7 |
| V | E | Em7 |
| VI | F | Fmaj7 |
| VII | G | G7 |

Las teclas fuera de escala se cuantizan al grado más cercano, así que **no hay
forma de tocar una nota fuera de tonalidad**. Verificado por test sobre
5 escalas × 12 tonalidades × 11 tipos × 25 notas.

`power` y `dim7` son excepciones deliberadas: mantienen sus intervalos exactos
porque su identidad *son* sus intervalos. Detalle en [DECISIONS.md](DECISIONS.md) D-021.

### El carácter

- **Osciladores:** sierra y pulso con PWM por PolyBLEP; triángulo ingenuo
  (justificado: aliasa *menos* que la sierra corregida, ver D-030); órgano
  tipo M1 por wavetable band-limited con mipmap de 8 tablas.
- **Unísono:** 1 a 7 copias por voz, detune simétrico, paneo de potencia
  constante y fase inicial fija o aleatoria.
- **Filtro:** SVF topology-preserving (Zavalishin), 12 y 24 dB/oct, LP/BP/HP,
  con auto-oscilación real acotada por saturación. Cutoff modulado por
  envolvente propia (bipolar), key tracking y velocity — todo sumado en
  **octavas**, no en Hz.
- **Drive:** waveshaper asimétrico con sobremuestreo ×2 (×4 en HQ), bypass con
  retardo compensado.

### La cadena de efectos

Orden: **saturación → chorus → delay** (reordenables entre sí, 6 combinaciones)
→ reverb → pump → salida. Los últimos tres son fijos por buenas razones
([D-038](DECISIONS.md)).

- **Delay** — el bloque más importante. Sincronizado al host con tresillos y
  puntillos (`1/8.` = 3/16, el del dub techno), o en ms libres. Tres modos:
  estéreo, ping-pong y dub (saturación + pasabanda en el lazo). HP y LP dentro
  de la realimentación, wow & flutter, **ducking**, y freeze que sostiene
  indefinidamente sin desbordar. Cambiar el tiempo desliza como una cinta
  (con glissando) o salta con crossfade, a elección.
- **Reverb** — FDN 8×8 con matriz de Hadamard y difusor Dattorro al frente.
  Plate / room / hall, pre-delay sincronizable, damping, modulación y ducking.
- **Chorus** — BBD de dos líneas en contrafase con el pasabajos que le da el
  carácter Juno, más un modo ensemble de cuatro líneas.
- **Saturación** — tape / transistor / diode con ×4 de sobremuestreo, más
  bitcrush con reducción de sample rate (ese **sin** sobremuestrear: su
  aliasing es el efecto).
- **Pump** — sidechain interno alineado a la **posición del transporte**, no a
  un LFO libre: cae siempre en el tiempo fuerte aunque arranques desde el
  compás 37.
- **Salida** — ancho M/S con graves en mono (requisito de club), limitador
  suave y medidores.

### Mediciones

Medido en el propio target de tests (`Tests/test_performance.cpp`), 48 kHz,
buffer 128, con 8 acordes × 7 voces = **56 voces sonando** (el peor caso):

| Configuración | Osciladores | CPU de un core |
|---|---:|---:|
| default (unísono 1, drive off) | 112 | 9,5 % |
| unísono 7, drive off | 322 | 11,6 % |
| unísono 7, drive ×2 | 322 | 32,9 % |
| unísono 7, drive ×4 (HQ) | 322 | 70,8 % |

**En reposo: 0,027 %** — la cadena de efectos se saltea cuando el `SilenceGate`
detecta silencio. Sin eso costaba 1,13 % por instancia sin producir nada
([D-037](DECISIONS.md)). **HQ Mode es para render offline**, no para tocar.

El costo del drive escala con *voces*, no con osciladores: subir el unísono de
1 a 7 cuesta 2 puntos, encender el drive cuesta 21. La palanca es la polifonía.

| Calidad (medida por test) | Resultado |
|---|---|
| Delay: posición del eco a 10 / 125 / 375 / 1000 ms | muestra **exacta** en los cuatro |
| Delay: distorsión con retardo fraccionario | **−240 dB** (Lagrange-3) |
| Delay: ducking de las repeticiones | 0,467 → 0,047 (**10×**) |
| Reverb: los 3 modos con decay y size al máximo | pico 0,900, sin divergencia |
| Reverb: rango del control de decay | 0,43 s a 5,18 s |
| Graves en mono: residuo en contrafase a 50 Hz vs 2 kHz | 0,38 vs 2,54 |
| Aliasing del drive con ×2 / ×4 | **−14 dB** / **−26 dB** vs sin sobremuestreo |
| Round trip del sobremuestreador | −95,4 dB, retardo 11 muestras exactas |
| Pendiente asintótica 12 / 24 dB/oct | 10,7 / 24,2 dB por octava |
| Aliasing triángulo vs sierra PolyBLEP | −68,3 dB vs −62,9 dB |
| Salida con buffer 32 vs 512 | diferencia **0.000000000000** (bit a bit) |
| Salida en reposo, con colas de delay y reverb | exactamente `0.0f` |

Roadmap completo en [DECISIONS.md](DECISIONS.md).

| Fase | Qué | Estado |
|---|---|---|
| 1 | Esqueleto: CMake, APVTS, sierra polifónica, tests, pluginval | **completa** |
| 2 | Motor de acordes: escalas, voicings, inversiones, passthrough | **completa** |
| 3 | Carácter: osciladores, unísono, filtro SVF, drive | **completa** |
| 4 | Efectos: delay, reverb, chorus, saturación, pump | **completa** |
| 5 | Secuenciador: gate rítmico, swing, sync al host | **completa** |
| 6 | Identidad visual: GUI completa con visualizadores | **completa** |
| 7 | Presets, pulido, distribución | — |

---

## Compilar

### Plataformas

| Formato | Plataforma | Estado |
|---|---|---|
| **VST3** | Windows x64 | entregable de la v1 |
| Standalone | Windows x64 | sólo para debug |
| AU | macOS | **fuera de alcance, sin verificar** |

El target AU está declarado en el CMake y se activa solo en macOS, pero no hay
mac en este proyecto: ese código **nunca se compiló**. No es un entregable.
El porqué y los caminos para revertirlo están en [DECISIONS.md](DECISIONS.md) D-018.

### Requisitos

- Visual Studio Build Tools 2022 con el workload *Desktop development with C++*
  (incluye MSVC, CMake y el Windows SDK)
- CMake 3.22+ (el que trae Visual Studio sirve, no hace falta instalarlo aparte)

JUCE se descarga sola vía `FetchContent`, pinneada al tag `8.0.15`. No hay que
instalar nada aparte.

### Windows

La forma facil: doble clic en **`Compilar e instalar STABR.bat`** (en
`Scripts/`, o copiado al escritorio). Se eleva solo pidiendo UAC —lo necesita
para escribir en `Program Files`—, compila en Release e instala el VST3.

O desde PowerShell:

```powershell
.\Scripts\build.ps1              # Release
.\Scripts\build.ps1 -Config Debug
.\Scripts\build.ps1 -Clean
.\Scripts\build.ps1 -NoCopy      # sin instalar en el sistema
```

El VST3 se copia solo a `C:\Program Files\Common Files\VST3`. Si esa copia falla
—porque un DAW tiene el plugin cargado y el archivo queda bloqueado, o porque la
shell no tiene permisos ahí— usá `-NoCopy`: compila y los tests corren igual, y
el plugin queda en `build/STABR_artefacts/`.

> **Si el build falla por falta del Windows SDK**, el script te imprime el
> comando exacto. Tres formas de que ese comando falle:
>
> - El verbo del instalador tiene que ser **`modify`**, no `install`. Con
>   `install`, si Build Tools ya está instalado, aborta con *"ya se ha
>   instalado"*, no agrega nada, y winget igual reporta éxito.
> - En PowerShell el comando tiene que empezar con **`&`**. Sin él, la ruta
>   entre comillas se interpreta como cadena y tira
>   `ParserError: Token inesperado "modify"`.
> - **No uses `--wait`**: no existe en el instalador 4.10 y aborta con
>   `exit 87`. Sin `--wait` el comando devuelve el prompt enseguida y la
>   instalación sigue en segundo plano; usá `--passive` para ver el progreso.

---

## Tests

Target headless basado en `juce::UnitTest`, sin dependencias externas. Cubre
generación, envolventes, aliasing, independencia de sample rate y de tamaño de
buffer, ausencia de NaN/inf y silencio exacto.

```bash
ctest --test-dir build -C Release --output-on-failure
```

O directamente el ejecutable, que imprime el detalle:

```powershell
& (Get-ChildItem build -Recurse -Filter "STABR_Tests.exe" | Select-Object -First 1).FullName
```

## Validación

pluginval en **strictness 10**, obligatorio al cerrar cada fase. El script
descarga pluginval a `Tools/` la primera vez.

```powershell
.\Scripts\validate.ps1
```

---

## Estructura

```
Source/
  PluginProcessor.*     procesador principal, dispatch de MIDI por sub-bloque
  PluginEditor.*        escala la interfaz y le da el pulso de animacion (VBlank)
  Params/               IDs estables y layout de parámetros
  DSP/                  voces, osciladores, filtros, drive, robo por acorde
  Common/               utilidades compartidas (silencio, versionado de estado)
  Chord/                generador de acordes, escalas y ruteo de notas
  FX/                   delay, reverb, chorus, saturacion, pump, salida
  Sequencer/            secuenciador de pasos, patrones de fabrica, latch
  GUI/                  LookAndFeel, controles, visualizadores, grilla de pasos
Tests/                  tests headless
Scripts/                build, validación y el .bat de instalación
cmake/                  JUCE pinneada, flags de warnings
```

---

## Reglas del hilo de audio

Innegociables. Dentro de `processBlock`: cero asignaciones de memoria, cero
locks, cero I/O, cero `std::string`, cero excepciones. Todo buffer se reserva en
`prepareToPlay`. Denormals con `ScopedNoDenormals`. Todos los parámetros
suavizados.

---

## Licencia

Código propietario. JUCE bajo tier Starter, SDK VST3 bajo MIT.
Detalle completo y checklist previo a distribuir en [LICENSING.md](LICENSING.md).

VST es marca registrada de Steinberg Media Technologies GmbH.
