# STABR

Máquina de stabs de house y techno. Instrumento **VST3 para Windows x64** que
convierte una sola tecla en un acorde corto, percusivo y con carácter.

**No es un sintetizador generalista.** Cada decisión responde a una sola
pregunta: *¿esto ayuda a que alguien saque un stab que suene a club en menos de
30 segundos?*

Desarrollado por **Narfbach**.

---

## Estado

**Fase 2 completa y verificada.** Una tecla dispara un acorde. Escalas con
scale lock diatónico, 13 tipos de acorde, voicings, inversiones, densidad,
spread, y tres modos de nota (Chord / Passthrough / Auto).

Build limpio sin warnings, 2.154.986 aserciones en verde, y **pluginval
strictness 10 en SUCCESS** sobre 6 sample rates (44.1 a 192 kHz) × 7 tamaños de
buffer (32 a 2048).

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

### Mediciones

Medido en el propio target de tests (`Tests/test_performance.cpp`), 48 kHz,
buffer 128:

| Acordes | Voces | Factor tiempo real | CPU de un core |
|---:|---:|---:|---:|
| 1 | 4 | 560× | 0,18 % |
| 4 | 16 | 228× | 0,44 % |
| 8 | 32 | 131× | 0,76 % |
| 8 (densidad 7) | 56 | — | **1,27 %** |

Peor caso absoluto: 8 acordes × 7 voces = 56 voces sonando, 1,27 % de un core.
A 96 kHz sube a 1,52 %, a 192 kHz a 3,06 %. En reposo cae a 0,12 % gracias al
`SilenceGate`.

| Calidad | Resultado |
|---|---|
| Aliasing PolyBLEP vs sierra ingenua (C8, 48 kHz) | **−72,6 dB** vs −57,3 dB |
| Salida con buffer 32 vs 512 | diferencia **0.000000000000** (bit a bit) |
| Duración del stab a 44,1 / 48 / 96 / 192 kHz | 168,30 / 168,31 / 168,30 / 168,30 ms |
| Salto máximo de muestra al robar voces | 0,52 (umbral 1,2) |
| Salida en reposo | exactamente `0.0f` |

Roadmap completo en [DECISIONS.md](DECISIONS.md).

| Fase | Qué | Estado |
|---|---|---|
| 1 | Esqueleto: CMake, APVTS, sierra polifónica, tests, pluginval | **completa** |
| 2 | Motor de acordes: escalas, voicings, inversiones, passthrough | **completa** |
| 3 | Carácter: osciladores, unísono, filtro SVF, drive | — |
| 4 | Efectos: delay, reverb, chorus, saturación, pump | — |
| 5 | Secuenciador: gate rítmico, swing, sync al host | — |
| 6 | Identidad visual: GUI completa con visualizadores | — |
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
  PluginEditor.*        GUI placeholder (se tira entera en Fase 6)
  Params/               IDs estables y layout de parámetros
  DSP/                  voces, osciladores, envolventes, robo por acorde
  Common/               utilidades compartidas (silencio, versionado de estado)
  Chord/                generador de acordes, escalas y ruteo de notas
  FX/ Sequencer/ GUI/   fases 4 a 6
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
