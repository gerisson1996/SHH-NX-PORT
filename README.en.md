# SILENT HILL HOMECOMING NX

🇧🇷 [Português](README.md) · 🇺🇸 **English** · 🇪🇸 [Español](README.es.md)

**Silent Hill: Homecoming** (Xbox 360) on the **Nintendo Switch**, as homebrew (`.nro`), through **static
recompilation**. The PowerPC code of `default.xex` is translated to C++ with
[ReXGlue](https://github.com/rexglue/rexglue-sdk) and compiled for ARM64. It is not emulation: the game runs
natively, with the Xbox 360's Direct3D 9 rewritten on top of deko3d, the Switch's native graphics API.

| | |
|---|---|
| **Version** | V1.0 |
| **State** | Playable: intro, menus, gameplay and audio; 30 fps in the menus and most of the game |
| **Download** | `shh.nro` and `SHH-NX-V1.0.zip` in [Releases](https://github.com/gerisson1996/SHH-NX-PORT/releases/latest) |
| **Next** | 30 fps in the heaviest areas at 720p too; Android and iOS |

### ☕ Support the project

**We need your help!** The tools I use to make these ports are **paid**, and every donation helps keep them and
keep the project going. Want another game ported or an improvement? **Send your request with your donation**
(write it in the Ko-fi, PayPal or Pix message).

If the port was useful to you, please consider supporting:

- **Ko-fi:** [ko-fi.com/gerissoncassiano](https://ko-fi.com/gerissoncassiano)
- **PayPal:** [Donate with PayPal](https://www.paypal.com/donate/?hosted_button_id=B3YTKT29F5ZLG)
- **Pix:** `pocketneogames@gmail.com`

> [!IMPORTANT]
> This repository **does not contain the game's files** (the `.xex`, the `.PAK` files, videos and sounds) nor the C++
> code generated from it. You need your own original copy of Silent Hill: Homecoming for Xbox 360.

---

## ⚡ Stable 30 fps

At **720p** with the console's stock clocks, most of the game already runs at 30 fps, but the heaviest areas drop to
23–27 fps. To make it **stable**:

- **600p resolution:** in the port menu (**L + R + Plus**), set the resolution to **600p**. The game is stable there,
  with an image only slightly softer.
- **Or raise the GPU clock:** to keep 720p at a stable 30 fps, raise the GPU clock (for example with sys-clk). With
  the GPU above the stock handheld clock (460.8 MHz) the game holds a stable 30 fps.

---

## What works

- The game runs natively on the Switch: opening videos, menus, gameplay and audio.
- Native deko3d renderer, **720p** handheld (480p, 600p and 720p in the port menu; up to 1080p docked), with the
  game's shaders compiled on the console and kept in `deko3d_shaders.bin` (the release already brings this cache for
  the first areas).
- **30 fps** in the menus and most areas at the console's stock clocks (CPU 1020 MHz, GPU 460.8 MHz handheld);
  stable at 600p or with the GPU above its stock clock.
- CPU optimizations in the code generator and GPU optimizations in the game's shaders (lights, shadows, vertices and
  screen copies), without changing the image.
- Port menu (**L + R + Plus**).

## What is missing

- **Stable 30 fps at 720p at the stock clock** in the heaviest areas.
- Known bugs:
  - in the menu, the cursor sometimes skips two entries when holding down;
  - button icons show up huge for an instant when opening the menu;
  - the mirror's reflection draws the scene again;
  - dark rounded blotches on some lights.
- Testing the whole game from start to finish.
- Android and iOS.

---

## Installing on the Switch

```
sdmc:/switch/shh-nx/shh.nro
sdmc:/switch/shh-nx/deko3d_shaders.bin   <- shader cache (in the zip)
sdmc:/switch/shh-nx/game_root/           <- your disc's files: default.xex, the .PAK files, audio/, movies/...
```

- Needs Atmosphère and hbmenu.
- Open it in **title mode**: hold **R** while launching an installed game, then pick Silent Hill Homecoming in
  hbmenu. The album does not give enough memory.
- Areas not in the cache compile their shaders on the first visit (a few seconds); after that they are saved.
- Logs go to `sdmc:/switch/shh-nx/logs/`.

---

## How it works

| Folder | What it is |
|---|---|
| `app/` | The port: code generator configuration (`overrides.toml`, `huecos.toml`), the application (`src/shh_app.h`) and the Renut renderer (`src/renut_engine`), which replaces the Xbox 360's Direct3D 9 with deko3d. |
| `sdk/` | The [ReXGlue](https://github.com/rexglue/rexglue-sdk) recompilation SDK, with the Switch layer from [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) (memory, files on the SD, threads, audio). |
| `app/thirdparty/` | plume (graphics abstraction), XenosRecomp (Xenos shaders to GLSL), uam (deko3d shader compiler) and Dear ImGui. |
| `tools/` | Build scripts, `.xex` analysis and the deko3d shader package (optimizations and cache). |
| `docs/` | Technical notes: [the `.xex` translation](docs/notas-traducao.md) and [the performance profile](docs/perfil-shh.md). |

---

## Building

### Requirements

- **Windows** with **Visual Studio 2022 Build Tools** ("Desktop development with C++" and "C++ Clang tools") and
  **LLVM clang 18+**.
- **devkitPro** with `switch-dev`, libnx and deko3d.
- **Python 3.10+** and **Git**.
- The repository in a path **without accents or spaces** (e.g. `C:\dev\shh-nx`).

### Steps

1. Fetch the SDK's libraries and build the code generator:

   ```bat
   python tools\fetch_thirdparty.py
   tools\build_host.bat
   ```

2. Put your disc's files in `assets\game_root` (`default.xex` and the `.PAK` files) and translate the game, which
   writes `app/generated/default`:

   ```sh
   REXGLUE=sdk/out/win-amd64/rexglue.exe tools/codegen.sh app
   ```

3. Build the `.nro` (deko3d):

   ```bat
   tools\build_switch.bat C:\path\to\nvk_sdk nonvk
   ```

   The result is `out\switch-deko\shh.nro`.

---

## Credits

- **ReXGlue SDK**: Tom Clay, with parts from [Xenia](https://xenia.jp) (BSD-3-Clause)
- **nfsmw-nx**: StevensND, the Switch layer and the project's base
- **Renut renderer**: from the sa2-nx and banjo-nx ports
- **XenosRecomp**: hedge-dev (MIT)
- **deko3d and uam**: fincs (devkitPro)
- **Switch port**: RECOMP-GUTO (gerisson1996)
- **Libraries**: libnx/devkitPro, plume, glslang, SPIRV-Tools, FFmpeg, Dear ImGui, toml++, fmt, spdlog; see
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

GPL-3.0 for the main source; the SDK and the libraries keep their own licenses.

Silent Hill is a trademark of Konami Digital Entertainment. This project is not affiliated with it.
