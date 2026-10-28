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
| 1 | Project, code generator, first C++ translation of `default.xex` | done (not compiled yet) |
| 2 | Game runs on PC with ReXGlue's own graphics | - |
| 3 | Switch build (NRO), memory, threads, audio | - |
| 4 | Shader library and native Vulkan renderer | - |
| 5 | Browser installer | - |

### Notes from phase 1

- Game: Title ID `4B4E07E5`, "Build v18.51" (Code Changelist #437812), XEX file time 2008-08-14.
- The first codegen failed on two branch targets with no function; they are declared in `app/overrides.toml`.
  The second run wrote 152 C++ files (about 163 MB) in about 2 minutes.
- The codegen still warns `Unresolved conditional branch to 0x8265CC48 from 0x8265CC54`. Check it in phase 2.
- `tools/huecos.py` has not been run yet. Run it and add `huecos.toml` to the manifest when the game starts hitting
  "Call to invalid or unregistered function" at run time.
- `sdk/` comes from nfsmw-nx and has some NFSMW tuning, for example XMA audio and file reads. Check it in phase 2
  before trusting it for this game.

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
