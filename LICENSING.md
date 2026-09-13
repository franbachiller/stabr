# LICENSING — STABR

Estado: **uso personal hoy, distribución comercial de código cerrado en el futuro.**
Todo lo que sigue está escrito con ese destino en mente.

Última verificación contra las fuentes: **2026-09-09**, sobre el tag `JUCE 8.0.15`.

> Esto no es asesoramiento legal. Es un resumen técnico de lo que dicen las
> licencias, hecho para tomar decisiones de arquitectura. Antes de vender,
> leé vos mismo el EULA de JUCE.

---

## 1. JUCE

JUCE 8 es **dual: AGPLv3 o licencia comercial JUCE.** (Ojo: es AGPLv3, no
GPLv3 — JUCE cambió de GPL a AGPL en la versión 8. La AGPL agrega la cláusula
de "uso en red", que para un plugin de audio no cambia mucho en la práctica,
pero es una licencia distinta.)

Verificado en `LICENSE.md` del tag 8.0.15:

> The JUCE Framework modules are dual-licensed under the AGPLv3 and the
> commercial JUCE licence.

### Tiers comerciales (juce.com/legal/juce-8-licence/)

| Tier | Tope de facturación anual | Costo |
|---|---|---|
| **Starter** | hasta USD 20.000 | **gratis** (perpetua) |
| Indie | hasta USD 300.000 | USD 40/mes (mín. 1 mes) o USD 800 perpetua |
| Pro | sin límite | USD 175/mes (mín. 12 meses) o USD 3.500 perpetua |
| Educational | sin límite | gratis, pero **prohibido cualquier uso comercial** |

### Qué significa para STABR

**El tier Starter permite distribución comercial de código cerrado.** Podés
vender STABR sin pagar licencia hasta los USD 20.000 anuales de facturación.
No hace falta liberar el código ni adoptar AGPL.

**Decisión: STABR se desarrolla bajo el tier Starter con código propietario.**
El código de este repositorio NO es AGPL. Cuando la facturación se acerque al
tope, se pasa a Indie.

### Consecuencia práctica y no negociable

**No se puede incorporar a STABR ningún código de terceros con licencia
GPL/AGPL/copyleft.** Traer una sola función GPL contamina todo el binario y
obliga a liberar el plugin entero. Licencias permisivas (MIT, BSD, ISC,
Apache 2.0, zlib, dominio público) sí se pueden usar, documentando la
atribución en este archivo.

### Splash screen

En el CMake está `JUCE_DISPLAY_SPLASH_SCREEN=0`. El texto del EULA que se
consultó no menciona requisito de splash screen ni de atribución "Made with
JUCE" para ningún tier. **A verificar en el EULA completo antes de la primera
distribución pública.** Si resultara obligatorio en Starter, se cambia a `1`.

---

## 2. SDK VST3 (Steinberg)

**Cambió a mejor y esto simplifica todo.** El SDK VST3 que viene embebido en
JUCE 8.0.15 está bajo **licencia MIT**.

Verificado en
`modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt`:

```
MIT License
Copyright (c) 2025, Steinberg Media Technologies GmbH
```

### Qué significa

- **No hay que firmar el acuerdo propietario de Steinberg.**
- **No hay que registrarse como desarrollador VST3 ante Steinberg.**
- **No hay incompatibilidad de licencias.** Históricamente el SDK VST3 era
  dual GPLv3 / propietario-Steinberg, y combinarlo con JUCE bajo copyleft era
  un problema real. Con MIT eso desapareció.
- Obligación única: mantener el aviso de copyright de Steinberg en las
  distribuciones. JUCE ya lo incluye en el árbol del SDK.

### Marca registrada

"VST" sigue siendo **marca registrada de Steinberg**. La licencia MIT cubre el
código, no la marca. Al promocionar STABR hay que usar el logo VST según los
lineamientos de marca de Steinberg e incluir el aviso de trademark en el
material de marketing. Esto es independiente de la licencia del código.

---

## 3. Audio Units (Apple)

- El AudioUnitSDK que embebe JUCE está bajo **Apache 2.0** (permisiva, compatible).
- Construir AU requiere **macOS + Xcode**. No hay forma de compilarlo ni
  testearlo en Windows.
- Para distribuir en macOS hace falta una **cuenta de Apple Developer
  (USD 99/año)** para firmar y notarizar. Sin notarización, Gatekeeper bloquea
  el plugin en las máquinas de los usuarios.

**Estado actual del proyecto: AU está fuera de alcance para la v1.** No hay
acceso a una mac, así que ese código nunca se compila y está sin verificar. El
target sigue declarado en el CMake para no tener que rehacerlo el día que
aparezca una máquina o un runner de CI con macOS, pero **no se promete como
entregable**. Ver [DECISIONS.md](DECISIONS.md) D-018, que lista los caminos
para revertirlo y lo que cuesta cada uno.

Si algún día se distribuye en macOS, esta sección aplica y hay que sumar la
cuenta de Apple Developer al presupuesto.

---

## 4. Dependencias de terceros

Hoy STABR no usa ninguna dependencia externa fuera de JUCE. Cualquier
incorporación futura se anota acá con su licencia, y **debe ser permisiva**.

| Componente | Licencia | Estado |
|---|---|---|
| JUCE 8.0.15 | Comercial (tier Starter) | En uso |
| VST3 SDK (vía JUCE) | MIT | En uso |
| AudioUnitSDK (vía JUCE) | Apache 2.0 | Solo macOS |

### Tipografías

**Fase 8 (SP-92): tres tipografías embebidas, las tres bajo SIL OFL 1.1.**
Descargadas el 2026-09-13 con autorización del usuario.

| Fuente | Uso | Origen | Licencia |
|---|---|---|---|
| Michroma Regular | marca, modelo, pad STAB, VU | github.com/google/fonts (`ofl/michroma`) | OFL 1.1 |
| Archivo Narrow SemiBold (600) y Bold (700) | serigrafía, teclas, pantallas chicas | fonts.gstatic.com, instancias estáticas de Google Fonts | OFL 1.1 |
| Reenie Beanie Regular | disquetes y cinta escritos a mano | github.com/google/fonts (`ofl/reeniebeanie`) | OFL 1.1 |

Archivo Narrow viene en el repositorio de Google solo como fuente variable, y
JUCE no elige pesos dentro de una fuente variable: por eso se usan las dos
instancias estáticas que sirve Google Fonts, las mismas que usa la maqueta.

La OFL permite embeberlas y distribuirlas dentro de software comercial con dos
condiciones que se cumplen así:

- **Incluir la licencia:** los textos están en `Resources/Fonts/OFL-*.txt` y el
  instalador los copia a `{app}\Licenses`.
- **No vender las fuentes solas:** van compiladas dentro del binario del plugin
  (`juce_add_binary_data`), nunca como archivos sueltos.

No se modificaron, así que no aplica la cláusula de nombre reservado.

Si más adelante se quiere una letra propia (por identidad, o para que la
interfaz se vea igual en otros sistemas), tiene que tener licencia que permita
**embeber en software distribuido**. Las de Google Fonts bajo **SIL OFL 1.1**
sirven; muchas comerciales cobran licencia de embedding aparte. La licencia
elegida se documenta acá.

---

## 5. Checklist antes de la primera venta

- [ ] Confirmar el tier de JUCE según facturación proyectada
- [ ] Leer el EULA completo de JUCE 8 y confirmar el tema del splash screen
- [ ] Revisar los lineamientos de marca VST de Steinberg para el material de marketing
- [ ] (Si se decide sacar macOS) mac o CI con runners macOS, cuenta Apple Developer y notarización
- [ ] Firma de código en Windows (certificado EV o el SmartScreen espanta usuarios)
- [x] Tipografías de la GUI: Michroma, Archivo Narrow y Reenie Beanie embebidas, OFL 1.1, licencias en el instalador (Fase 8)
- [ ] Confirmar que no entró ningún código GPL/AGPL al árbol
