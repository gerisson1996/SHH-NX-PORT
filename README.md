# Silent Hill: Homecoming, Nintendo Switch port (shh-nx)

Unofficial fan project, not affiliated with Konami. The goal is a native Switch port of the Xbox 360 version, made
the same way as [nfsmw-nx](https://github.com/StevensND/nfsmw-nx): the PowerPC code of `default.xex` is statically
recompiled to C++ with [ReXGlue](https://github.com/rexglue/rexglue-sdk) and built for the Switch with devkitA64 and
libnx. It is not an emulator.

Nothing from the game is in this repository. You need your own Xbox 360 copy of the game.

## Status

Phase 1: translating `default.xex` to C++ on the PC. The game does not run yet.

| Phase | What | State |
|---|---|---|
| 1 | Project, code generator, first C++ translation of `default.xex` | done |
| 2 | Game runs on PC with ReXGlue's own graphics | title screen reached; gameplay not tested |
| 3 | Switch build (NRO), memory, threads, audio | NRO builds; not tested on the console yet |
| 4 | Shader library and native Vulkan renderer | - |
| 5 | Browser installer | - |

### Notes from phase 1

- Game: Title ID `4B4E07E5`, "Build v18.51" (Code Changelist #437812), XEX file time 2008-08-14.
- The first codegen failed on two branch targets with no function; they are declared in `app/overrides.toml`.
  The second run wrote 152 C++ files (about 163 MB) in about 2 minutes.
- `sdk/` comes from nfsmw-nx and has some NFSMW tuning, for example XMA audio and file reads. Check it before
  trusting it for this game.

### Notes from phase 2

- The first PC run stopped at once on `Call to invalid or unregistered function at guest address 0x8268C720`: a
  code gap the analysis left without a function. `tools/huecos.py --min 8` found 908 gaps. Declaring them all split
  some real functions (356 branch warnings in the codegen log) and some were data or import thunks.
  `tools/huecos_conflictos.py` removes those from `app/huecos.toml` and lists them in `tools/huecos_excluir.txt`.
  After two passes, 757 gaps remain and the codegen gives no warnings.
- Graphics need the `xenos` GPU plugin: `app/CMakeLists.txt` puts it next to the .exe and `app/src/shh_app.h`
  sets `gpu_plugin=xenos` by default. Without it the game runs with a black window.
- With that, the game plays the intro movies (Bink) and reaches "Press START" at about 30 FPS on the PC.
- Open: one draw with tessellation fails in the backend (`PM4_DRAW_INDX ... tess_mode=1`), and the game cannot
  write `d:\streaming.log` because the game folder is mounted read-only.

## Building for the Switch

Needs devkitPro and the static NVK driver: a `portlibs/switch` folder with `lib/libvulkan.a`, built from
[mesa-switch](https://github.com/danfromtico/mesa-switch) at commit `1a8c1a66d6f` with nfsmw-nx's
`mesa/mesa-switch-nfsmw.patch` (`tools/build_mesa.sh` does it on Windows with MSYS2 at `C:\msys64`).

```bat
tools\build_switch.bat C:\path\to\nvk_sdk
```

The result is `out\switch\shh.nro`. On the SD card:

```
sdmc:/switch/shh-nx/
    shh.nro
    game_root/      (the game files: default.xex, the .PAK files, audio/, movies/, ...)
```

Start it from the Homebrew Menu in title takeover mode (hold R while starting a game). The album does not give
enough memory. Graphics still go through ReXGlue's GPU emulation, which was too slow for nfsmw-nx on the Switch.

## Running on the PC

```bat
tools\build_pc.bat
out\pc\shh.exe --game_data_root=assets\game_root
```

## Layout

| Folder | What it has |
|---|---|
| `app/` | The game's manifest (`shh_manifest.toml`), hand-written overrides, and later the port's own code |
| `sdk/` | ReXGlue SDK with the Switch layer and generator changes from nfsmw-nx |
| `tools/` | Build and analysis scripts |
| `assets/game_root/` | Your game files (git-ignored). Here it is a junction to the folder with the extracted disc |

## Building the code generator and translating the game

Needs Visual Studio 2022 Build Tools, LLVM clang 18+, Python 3.10+ and Git.

```bat
python tools\fetch_thirdparty.py
tools\build_host.bat
```

Put the game files in `assets\game_root` (`default.xex` and the `.PAK` files), then:

```sh
REXGLUE=sdk/out/win-amd64/rexglue.exe tools/codegen.sh app
```

The C++ goes to `app/generated/default` and the log to `app/codegen.log`.

## License

GPL-3.0 for the main source, inherited from nfsmw-nx. The SDK and third-party parts keep their own licenses; see
`THIRD_PARTY_NOTICES.md`.
