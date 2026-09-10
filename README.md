# STABR

Máquina de stabs de house y techno. Instrumento **VST3 para Windows x64** que
convierte una sola tecla en un acorde corto, percusivo y con carácter.

**No es un sintetizador generalista.** Cada decisión responde a una sola
pregunta: *¿esto ayuda a que alguien saque un stab que suene a club en menos de
30 segundos?*

Desarrollado por **Narfbach**.

---

## Estado

**Fase 1 completa y verificada.** Plugin que compila, carga y suena: oscilador
sierra PolyBLEP polifónico con ADSR exponencial, APVTS versionado, motor de
voces propio con robo por edad y fade.

Build limpio sin warnings, 2.105.387 aserciones en verde, y **pluginval
strictness 10 en SUCCESS** sobre 6 sample rates (44.1 a 192 kHz) × 7 tamaños de
buffer (32 a 2048).

### Mediciones de la Fase 1

Medido en el propio target de tests (`Tests/test_performance.cpp`), 48 kHz,
buffer 128. Estos números son la línea de base para comparar contra las fases
siguientes:

| Voces | Factor tiempo real | CPU de un core | µs por bloque |
|---:|---:|---:|---:|
| 1 | 1445× | 0,07 % | 1,8 |
| 4 | 797× | 0,13 % | 3,3 |
| 8 | 476× | 0,21 % | 5,6 |
| 16 | 255× | 0,39 % | 10,5 |

Con 8 voces: 0,22 % a 48 kHz, 0,44 % a 96 kHz, 0,86 % a 192 kHz — escala lineal
con el sample rate, como corresponde. En reposo cae a 0,058 % gracias al
`SilenceGate`.

Calidad del oscilador y del motor:

| Medición | Resultado |
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
| 2 | Motor de acordes: escalas, voicings, inversiones, passthrough | — |
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

```powershell
.\Scripts\build.ps1              # Release
.\Scripts\build.ps1 -Config Debug
.\Scripts\build.ps1 -Clean
```

El VST3 se copia solo a `C:\Program Files\Common Files\VST3`.

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
  DSP/                  voces, osciladores, envolventes
  Common/               utilidades compartidas (silencio, versionado de estado)
  Chord/ FX/ Sequencer/ GUI/    fases 2 a 6
Tests/                  tests headless
Scripts/                build y validación
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
