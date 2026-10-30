# SILENT HILL HOMECOMING NX

🇧🇷 [Português](README.md) · 🇺🇸 [English](README.en.md) · 🇪🇸 **Español**

**Silent Hill: Homecoming** (Xbox 360) en **Nintendo Switch**, como homebrew (`.nro`), mediante **recompilación
estática**. El código PowerPC de `default.xex` se traduce a C++ con [ReXGlue](https://github.com/rexglue/rexglue-sdk)
y se compila para ARM64. No es emulación: el juego corre nativo, con el Direct3D 9 de Xbox 360 reescrito sobre deko3d,
la API gráfica nativa de Switch.

| | |
|---|---|
| **Versión** | V1.0 |
| **Estado** | Jugable: intro, menús, gameplay y audio; 30 fps en los menús y en la mayor parte del juego |
| **Descarga** | `shh.nro` y `SHH-NX-V1.0.zip` en [Releases](https://github.com/gerisson1996/SHH-NX-PORT/releases/latest) |
| **Próximo** | 30 fps también en las áreas más pesadas a 720p; Android e iOS |

### ☕ Apoya el proyecto

**¡Necesitamos tu ayuda!** Las herramientas que uso para hacer estos ports son **de pago**, y cada donación ayuda a
mantenerlas y a seguir con el proyecto. ¿Quieres ver otro juego portado o una mejora? **Haz tu pedido junto con tu
donación** (escríbelo en el mensaje de Ko-fi, PayPal o Pix).

Si el port te resultó útil, considera apoyar:

- **Ko-fi:** [ko-fi.com/gerissoncassiano](https://ko-fi.com/gerissoncassiano)
- **PayPal:** [Donar con PayPal](https://www.paypal.com/donate/?hosted_button_id=B3YTKT29F5ZLG)
- **Pix:** `pocketneogames@gmail.com`

> [!IMPORTANT]
> Este repositorio **no contiene los archivos del juego** (el `.xex`, los `.PAK`, vídeos y sonidos) ni el código C++
> generado a partir de él. Necesitas tu propia copia original de Silent Hill: Homecoming para Xbox 360.

---

## ⚡ 30 fps estables

A **720p** con los relojes originales de la consola, la mayor parte del juego ya corre a 30 fps, pero las áreas más
pesadas bajan a 23–27 fps. Para que quede **estable**:

- **Resolución 600p:** en el menú del port (**L + R + Plus**), cambia la resolución a **600p**. Así el juego queda
  estable, con una imagen apenas un poco más suave.
- **O sube el reloj de la GPU:** para mantener 720p a 30 fps estables, sube el reloj de la GPU (por ejemplo con
  sys-clk). Con la GPU por encima del reloj original del portátil (460,8 MHz) el juego queda a 30 fps estables.

---

## Qué funciona

- El juego corre nativo en Switch: vídeos de apertura, menús, gameplay y audio.
- Renderizador nativo en deko3d, **720p** en portátil (480p, 600p y 720p en el menú del port; hasta 1080p en el
  dock), con los shaders del juego compilados en la consola y guardados en `deko3d_shaders.bin` (la release ya trae
  esta caché lista para las primeras áreas).
- **30 fps** en los menús y en la mayoría de las áreas con los relojes originales (CPU 1020 MHz, GPU 460,8 MHz en
  portátil); estables a 600p o con la GPU por encima de su reloj original.
- Optimizaciones de CPU en el generador de código y de GPU en los shaders del juego (luces, sombras, vértices y
  copias de pantalla), sin cambiar la imagen.
- Menú del port (**L + R + Plus**).

## Qué falta

- **30 fps estables a 720p con el reloj original** en las áreas más pesadas.
- Bugs conocidos:
  - en el menú, el cursor a veces salta dos opciones al mantener abajo;
  - los iconos de botón aparecen gigantes un instante al abrir el menú;
  - el reflejo del espejo dibuja la escena otra vez;
  - manchas oscuras redondeadas en algunas luces.
- Probar el juego entero de principio a fin.
- Android e iOS.

---

## Instalar en Switch

```
sdmc:/switch/shh-nx/shh.nro
sdmc:/switch/shh-nx/deko3d_shaders.bin   <- caché de shaders (viene en el zip)
sdmc:/switch/shh-nx/game_root/           <- los archivos de tu disco: default.xex, los .PAK, audio/, movies/...
```

- Necesita Atmosphère y hbmenu.
- Ábrelo en **modo título**: mantén **R** al abrir un juego instalado y elige Silent Hill Homecoming en el hbmenu.
  Desde el álbum no hay memoria suficiente.
- Las áreas que no están en la caché compilan sus shaders en la primera visita (unos segundos); después quedan
  guardados.
- Los logs quedan en `sdmc:/switch/shh-nx/logs/`.

---

## Cómo funciona

| Carpeta | Qué es |
|---|---|
| `app/` | El port: configuración del generador de código (`overrides.toml`, `huecos.toml`), la aplicación (`src/shh_app.h`) y el renderizador Renut (`src/renut_engine`), que reemplaza el Direct3D 9 de Xbox 360 por deko3d. |
| `sdk/` | El SDK de recompilación [ReXGlue](https://github.com/rexglue/rexglue-sdk), con la capa de Switch de [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) (memoria, archivos en la SD, hilos, audio). |
| `app/thirdparty/` | plume (abstracción gráfica), XenosRecomp (shaders de Xenos a GLSL), uam (compilador de shaders de deko3d) y Dear ImGui. |
| `tools/` | Scripts de compilación, de análisis del `.xex` y del paquete de shaders de deko3d (optimizaciones y caché). |
| `docs/` | Notas técnicas: [la traducción del `.xex`](docs/notas-traducao.md) y [el perfil de rendimiento](docs/perfil-shh.md). |

---

## Compilar

### Requisitos

- **Windows** con **Visual Studio 2022 Build Tools** ("Desarrollo para el escritorio con C++" y "Herramientas de
  Clang para C++") y **LLVM clang 18+**.
- **devkitPro** con `switch-dev`, libnx y deko3d.
- **Python 3.10+** y **Git**.
- El repositorio en una ruta **sin acentos ni espacios** (p. ej. `C:\dev\shh-nx`).

### Pasos

1. Descargar las bibliotecas del SDK y compilar el generador de código:

   ```bat
   python tools\fetch_thirdparty.py
   tools\build_host.bat
   ```

2. Poner los archivos de tu disco en `assets\game_root` (`default.xex` y los `.PAK`) y traducir el juego, lo que
   genera `app/generated/default`:

   ```sh
   REXGLUE=sdk/out/win-amd64/rexglue.exe tools/codegen.sh app
   ```

3. Compilar el `.nro` (deko3d):

   ```bat
   tools\build_switch.bat C:\ruta\al\nvk_sdk nonvk
   ```

   El resultado queda en `out\switch-deko\shh.nro`.

---

## Créditos

- **ReXGlue SDK**: Tom Clay, con partes de [Xenia](https://xenia.jp) (BSD-3-Clause)
- **nfsmw-nx**: StevensND, la capa de Switch y la base del proyecto
- **Renderizador Renut**: de los ports sa2-nx y banjo-nx
- **XenosRecomp**: hedge-dev (MIT)
- **deko3d y uam**: fincs (devkitPro)
- **Port para Switch**: RECOMP-GUTO (gerisson1996)
- **Bibliotecas**: libnx/devkitPro, plume, glslang, SPIRV-Tools, FFmpeg, Dear ImGui, toml++, fmt, spdlog; ver
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

Licencia GPL-3.0 para el código principal; el SDK y las bibliotecas mantienen sus propias licencias.

Silent Hill es una marca de Konami Digital Entertainment. Este proyecto no tiene relación con ella.
