# STABR

Máquina de stabs de house y techno. Instrumento **VST3 para Windows x64** que
convierte una sola tecla en un acorde corto, percusivo y con carácter.

**No es un sintetizador generalista.** Cada decisión responde a una sola
pregunta: *¿esto ayuda a que alguien saque un stab que suene a club en menos de
30 segundos?*

Desarrollado por **Narfbach**.

---

## Estado

**Fase 1 — esqueleto.** Plugin que compila, carga y suena: oscilador sierra
PolyBLEP polifónico con ADSR exponencial, APVTS versionado, motor de voces
propio con robo por edad y fade.

Roadmap completo en [DECISIONS.md](DECISIONS.md).

| Fase | Qué | Estado |
|---|---|---|
| 1 | Esqueleto: CMake, APVTS, sierra polifónica, tests, pluginval | en curso |
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
> comando exacto. Hay dos formas de que ese comando falle en silencio:
>
> - El verbo del instalador tiene que ser **`modify`**, no `install`. Con
>   `install`, si Build Tools ya está instalado, aborta con *"ya se ha
>   instalado"*, no agrega nada, y winget igual reporta éxito.
> - En PowerShell el comando tiene que empezar con **`&`**. Sin él, la ruta
>   entre comillas se interpreta como cadena y tira
>   `ParserError: Token inesperado "modify"`.

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
