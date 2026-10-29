# Plan: Renut on plume/deko3d for Silent Hill: Homecoming

Why: on the console, the emulated Xenos GPU draws the game wrong and runs the title movie at about 6 fps. The GPU is
busy only about 600 ms a second, and render-target transfers take half of that. Every working ReXGlue Switch port of
a 3D game draws through Renut's Direct3D-level renderer on deko3d instead: sa2-nx, mk9-nx, usf4-nx, mgr-nx-deko and
naruto-nx. Reference: `C:\dev\sa2-nx-ref` (sa2-nx 32a81a8), `docs/deko3d-renderer-guide.md`.

What Silent Hill has in its favour: its Direct3D (XDK 2.0.6690) has N&B's device layout (`d3d-censo.md`), and Renut was
written for N&B. Renut's N&B offsets (given in brackets in sa2-nx's sources) apply as they are.

## Stages (each verified before the next)

1. **The sa2-nx SDK.** Replace `sdk/` with sa2-nx's fork (`patches/rexglue-sdk-v0.10.0.patch` against upstream
   v0.10.0: the Horizon layer, the no-NVK build, the input hook, the codegen changes). Rebuild `rexglue`, re-run the
   codegen, and re-check the gap rounds (`huecos.py`, `huecos_conflictos.py`). Gate: the PC build boots to the title
   with the emulated GPU, as now.
   **Done 2026-10-28.** sa2-nx's `sdk/` copied in. Its `thirdparty/` overlay was completed with the files of the old
   tree (the same v0.10.0 release), and the old SDK is kept at `C:\dev\shh-nx-sdk-xenos-backup`. The codegen
   (`REX_MAX_JUMP_TABLE_ENTRIES=1024`, image dumped to `out/default_image.bin`) gives no warnings, so the gap rounds
   still hold. `shh_register.cpp` (-O1) now skips the precompiled header. The PC build reaches the title movie as
   before.
2. **Renut on the PC (plume, Vulkan back end),** with the emulated GPU still built in as the reference (a boot key
   chooses the renderer, as sa2-nx's `sa2_renderer`):
   - vendor `app/src/renut_engine`, `app/thirdparty/plume`, `app/thirdparty/reblue-XenosRecomp`, `app/cmake/*renut*`;
   - point every `BANJO_RENUT_HOOK` at SHH's address (`d3d-censo.md`) and put N&B's offsets back where SA2's shifted
     ones are;
   - creation through SHH's type-2 (no hardware) path, then the title device's setup without the GPU;
   - teach `tools/llamadas_directas.py` the hooks (sa2-nx's `direct_calls.py` reads `BANJO_RENUT_HOOK*`).
   Gate: device created, ring sink, fences, Swap presenting the clear colour.
   **Started 2026-10-28.** What is in the tree:
   - the vendored trees above, from `C:\dev\sa2-nx-ref` (sa2-nx 32a81a8): `renut_engine` (112 files),
     `plume` (875), `reblue-XenosRecomp` (1481), `imgui`, `uam`, and `app/cmake` (banjo's, sa2's, the two scripts).
     The reference's `thirdparty/dxc-bin` had only headers, so DXC 1.9.0.5512 from `C:\VulkanSDK\1.4.363.0\Bin`
     (`dxc.exe`, `dxcompiler.dll`) is placed in `bin/x64/`; the host shaders need nothing else. That folder is ignored
     by `reblue-XenosRecomp`'s own `.gitignore` (`[Bb]in/`), so it is not in the repository: a fresh clone has to copy
     the same pair there (any DXC 1.9 or newer).
   - `app/cmake/shh_renut_gpu.cmake` (from sa2's), included by `app/CMakeLists.txt` behind
     `SHH_RENUT_RENDERER` (default ON); the target is `shh_renut_gpu`, an OBJECT library so its hooks replace the
     generated weak `sub_X` definitions. `shh_renderer` (`xenos` default / `renut`) picks the renderer at boot
     (`banjo_renut_select.cpp`, adapted: SA2's key and its no-NVK branch dropped).
   - `app/src/renut_engine/shh_compat` (from sa2_compat: the frame-latency probe, the profiler no-ops, the `banjo_*`
     keys) with `app/src/cpu/shh_enhancements.h` (two masters, both on). `app/src/me/me_platform_compat.h` (thread
     helpers, no-ops on the PC) and `app/src/me/sa2_render_mode.h` (a 720p shim; the vendored call sites keep SA2's
     namespace).
   - stage 2 has no translated shaders yet, so `RENUT_NO_SHADER_CACHE` compiles
     `gpu/shaders/empty_shader_cache.cpp` instead of running XenosRecomp and dumping the game's containers (stage 3).
     The game's screen width is `gpu/guest/shh_screen.cpp` (1280, as sa2-nx's).
   - the renderer's core (backend, render, guest records, overlay) compiles and links. The Direct3D hook files are
     not in yet; `gpu/d3d/shh_stage2_shims.cpp` stands in for the five symbols the core takes from them
     (`native::BlockOnFence` and the four record lookups). Delete it as `d3d/hooks/device.cpp` and `shader.cpp` come
     in.
   - the device hooks, as retargeted (Silent Hill's address): CreateDevice `82910928`, InitializePresentationParameters
     `82919758`, InitializeApiState `829194F0`, SynchronizeToPresentationInterval `82911880`, KickOff `8290FBB8`,
     KickOffSegment `8290FA30`, BeginRingBig `8290FDD8`, AllocateInlineData `8290EE48`, the ring segment allocator
     `8290F8A0`, Swap `82911D68`, BlockOnFence `8290F4A0`, IsFencePending `8290FCF0`, CBlocker::Check `82910448`.
     No hook yet for InsertCallback, RingBufferDeviceAllocate or SetRingBufferParameters: the census has not placed
     them.
   - next, in this order: `shader.cpp`, `resource.cpp` and `hooks/draw.cpp`, then the setters (`state.cpp`).
   - **2026-10-28, later:** `d3d/hooks/device.cpp` is retargeted (N&B's offsets back, Silent Hill's addresses on the
     hooks above). Two things had to change to get there:
     - the record heap could not take 64 MiB of Silent Hill's physical window (0xA0000000-0xBFFFFFFF) once the
       emulated Xenos GPU had its share, so `guest_records.cpp` now takes the largest of 64/32/16/8 MiB that fits and,
       when none does, the same block from the guest's own heap (the renderer only reaches it through a guest VA).
     - with `shh_renderer = renut`, `src/shh_app.h` no longer loads the emulated Xenos GPU at all: the two renderers
       would both draw, and the plugin's share of the physical window was what the renderer ran out of.
     Run of 2026-10-28 19:20 (`shh_renderer=renut`): "Guest D3D device (type 1) created at 0xe2e7fe00", "Render
     thread started", "Render resolution 1280x720", 296 game frames in 10 s (29.5 fps), 1481 kicks and 589 fence
     queries with no stall, 0 draws (the draw and state hooks are stage 3).
     The clear (stage 2's last gate item): `d3d/hooks/shh_clear.cpp` hooks ClearF (8290E528) and the game does call it
     (flags 0x31 = colour 0 + depth + stencil, colour 0,0,0,0, no rect, every frame at the title), but the picture stays
     black even with `shh_renut_clear_test=true`, which clears magenta instead: the Swap presents the *front buffer
     texture*, and without the resource and Resolve hooks the renderer has no record of it, so it presents an empty
     frame.
     `d3d/hooks/shh_resolve.cpp` adds the resolve (82914EA8), and the Swap's front buffer is exactly the resolve's
     destination (`0xe2e89ce0`, both), so the two line up.
     The resolve still drew nothing, and the reason was the offsets: `d3d.h` and `d3d_native.h` still carried sa2-nx's
     *shifted* device layout (render targets 12816, depth 12832, fence 11036, swap count 16968, ...) with N&B's in the
     comments. With N&B's values back (12440/12456, 10908, 16560, and the rest), the device now reads its targets:
     `targets=0xe2e88010,0x00000000 depth=0xe2e88048`, alternating with `0xe2e88080` as the game double-buffers. Four
     `static_assert`s in `queued.cpp` that encode SA2's private layout (its setter marks, `kMarkSnapshotEnd`) are
     commented out with a stage-3 note, since SHH has no setter marks yet.
     **The gate is met (2026-10-28, 19:42).** The last missing link was not in the renderer at all: the app never
     started it. sa2-nx starts it in `OnFinalizePaths` (and forwards window resizes), and `src/shh_app.h` did not, so
     `Video::CreateHostDevice` never ran: no host device, `host_ready` false, `EnsureCommandListLocked` false, and every
     resolve was dropped at its first line while the Swap presented an empty frame. With the same two calls in SHH's
     app: `Renderer: D3D12 on AMD Radeon RX 6600`, `Swapchain: 3 images`, the resolve finds its source
     (`1280x720, format 20`), and with `shh_renut_clear_test=true` (magenta instead of the game's black) the captured
     screen is `(255,0,255)` in the centre and in a corner: Silent Hill's own ClearF reached the screen through Renut's
     Swap. That closes stage 2: device created through Silent Hill's type-2 path, ring sink kicking (1478 kicks per
     10 s), fences answered (587 queries), Swap presenting the game's clear colour, the PC build green in both
     configurations (`xenos` and `renut`).
3. **Shaders and draws on the PC:** dump the game's containers (CreateVertexShader / CreatePixelShader), translate
   them with XenosRecomp, and draw. Compare with the emulated GPU frame by frame through boot, menus, the title
   movie and gameplay.
   **Started 2026-10-28.** What is ready and what is not:
   - the XenosRecomp toolchain now has its import library: `thirdparty/dxc-bin/lib/x64/dxcompiler.lib` (from
     `C:\VulkanSDK\1.4.363.0\Lib`; `bin/x64/` already has `dxc.exe` and `dxcompiler.dll`). Both folders are ignored by
     reblue's own `.gitignore`, so they are local to this tree. There is no `dxil.dll`: the SPIR-V side (what Vulkan
     builds use) is unaffected, and DXIL signing is only needed by D3D12 validation.
   - `RENUT_NO_SHADER_CACHE` is still ON: the dump directory (`app/shaders`) does not exist because Silent Hill's
     shader containers are not located yet (the plan's open point), and `tools/dump_shaders.py` has no SHH counterpart.
     Turning it off with an empty directory makes XenosRecomp produce an empty cache, which is the way to prove the
     toolchain before the dump exists.
   - the Direct3D entry points the draws need, as `docs/d3d-censo.md` places them: SetRenderTarget/SetDepthStencilSurface
     `82908128` (the census lists the two together: whether one function takes both or the depth has an index of its
     own has to be settled before hooking it, or a wrong hook would break the working clear), DrawVertices `82908408`,
     DrawIndexedVertices `829087F8`, SetIndices `82907640`, CreateVertexShader `82909FE0` (and XGRegisterVertexShader
     `82909990`), CreatePixelShader `82909ED0`, CreateVertexDeclaration `82909D60`, the per-draw setter `8290D238`, and
     the state setters at `82905xxx` (not identified one by one yet).
   - the sa2-nx pieces the draws use (setter marks, `src/me/sa2_d3d_api.cpp`) have no SHH counterpart, so the draws'
     device snapshot compares all chunks for now (`sa2_renut_snapshot_deltas_narrow = false`).
   - **The game's shaders are dumped (2026-10-28).** `d3d/hooks/shader.cpp` is in the build with Silent Hill's three
     hooks: XGRegisterVertexShader `82909990`, CreatePixelShader `82909ED0` (the object comes back in r3: the stub that
     read an out parameter captured nothing) and CreateVertexDeclaration `82909D60`. RegisterGuestShader captures every
     container the cache lacks, and one run to the title gives **893 containers: 205 vertex, 688 pixel**, in
     `out/pc/shader_capture/` (~1 MB).
   - **They are in the cache (2026-10-29).** `app/shaders` holds the 893 containers and `RENUT_NO_SHADER_CACHE` is off:
     XenosRecomp (built as an ExternalProject, with the DXC 1.8 trio and `dxcompiler.lib` in `dxc-bin/`) translates
     them at build time. Three things were needed: `tools/renut_early_alpha.py` and `tools/early_alpha.py` copied from
     sa2-nx, the early-alpha pass turned OFF (`BANJO_RENUT_EARLY_ALPHA`: DXC 1.8 and 1.9 both refuse the HLSL it
     writes Ã¢â‚¬â€ "partial explicit stage input location assignment via vk::location(X) unsupported"), and that the PC
     build name a native Windows Python (`C:\msys64\mingw64\bin\python.exe`): devkitPro's MSYS2 python on PATH cannot
     read `C:/dev/...` paths. 31 of the 893 hit the same DXC restriction in the plain translation, so
     `XenosRecomp/main.cpp` now writes the cache and keeps listing them (862 in the cache; a draw whose shader is
     missing is skipped).
   - **The draws are hooked (2026-10-29).** `d3d/hooks/shh_draw.cpp`: DrawVertices `82908408`, DrawIndexedVertices
     `829087F8`, SetIndices `82907640` and SetRenderTarget/SetDepthStencilSurface `82908128` (an index of 4 or more is
     taken as the depth surface, the census lists both at this address). Run of 07:37: **650 draws in 10 s**, the GPU
     busy 0.41 ms a frame over 302 frames, and the window no longer black (12,12,12 at the centre, 7,7,7 near the
     bottom: `out/renut_draws.png`) Ã¢â‚¬â€ the title's dark intro. One pipeline creation fails and is logged by key (VS
     8763F4DD2B71AA32, PS 4D524BFF32A7299D, 1 target).
   - **Compared with the emulated GPU (2026-10-29, 32 s).** `shh_renderer=xenos` shows the title screen (logo, clock,
     "Press START to Begin": `out/cmp_xenos_32s.png`); `renut` shows black with Renut's own overlay over it
     (`out/cmp_renut_32s.png`), and with `shh_renut_clear_test=true` it is black too, so the 650 draws paint over the
     clear rather than the Swap losing it.
   - **The title renders through Renut (2026-10-29).** Two fixes after the comparison: the vertex fetch constants are
     rebuilt from the streams before each draw's snapshot (`shh_draw.cpp`, FillVertexFetchConstants — Silent Hill builds
     them in the ring, not in the device), and — the decisive one — **the 82908128 hook is gone**: the calls it receives
     pass the surface in r4 and 0 in r5, so hooking it as `(device, index, surface)` wrote a surface address into the
     render target slot, every draw went to a broken 1x1 target, and the window stayed black. With the game's own
     writes to the device left alone, `out/renut_title.png` is the clock tower and street of the title screen, 296
     frames in 10 s at 29.5 fps, 650 draws. Still missing: the title's 2D layer (logo, "Press START"), the one pipeline
     that fails to create, and the 31 shaders DXC rejects.
   - **Where the geometry was lost (2026-10-29).** At the first draws: texture slot 0 = `0xe432c7e0` (N&B's `+12544` is
     Silent Hill's offset) and the device's fetch constant for it (`0x480`) holds a real, D3D-built fetch
     (`81004802 079f2052 ...`, the texture header's own form with the physical address), so textures are not the
     problem. Stream 0's source is set (`0xe4362fb0`, stride 24 from the packed `u8`, `+12528`), but the **vertex fetch
     constants are empty** (`0x6F0`: `00000001 00000000` â€” type 1 with address 0). Silent Hill writes the vertex
     buffers into the device but builds their fetch constants when the draw's state is flushed, in the ring, which this
     renderer does not parse. So the draws have no vertices: 650 draws, nothing on screen. Next: give the draws their
     fetch constants from the bound streams (source address, stride and the vertex buffer's header, as D3D's
     SetStreamSource does), or hook Silent Hill's SetStreamSource once its address is known.
     **Correction (2026-10-29, later):** the constants were not empty. `0x6F0` is the slot of stream 17: the array runs
     in reverse (stream s at `0x6F0 + 8 * (17 - s)`, so stream 0 at `0x778`, which is what Renut's `StreamFetchIndex`
     reads). Silent Hill's SetStreamSource is `82907520` (device, stream, vertex buffer, offset, stride). It writes the
     constant there at once, with the offset added to the address and taken from the size, and it tags the buffer's
     fence with the current one. The rebuild (FillVertexFetchConstants) passed offset 0, so every batch past the start
     of a shared vertex buffer drew the first batch's vertices: the copyright text showed a line and a half with holes,
     and the logo and "Press START" never showed. The rebuild is now off (`shh_renut_rebuild_vertex_fetch`, a
     diagnostic). What made the title draw was the removal of the 82908128 hook in the same commit.
   - **The title screen is complete (2026-10-29).** With the game's own vertex fetch constants: the copyright screen
     is identical to the emulated GPU's, and the title shows the logo and "Press START to Begin" over each of its
     camera shots (`out/serie_renut_folha.png`: 20, 24, 28 and 36 s; the black frame seen at 32 s was a cut between
     shots, the log loading `m01_nightmare_76.pak`). About 29.7 fps, no draw dropped for a missing shader.
     Two SA2 literals were also fixed in `render/draw.cpp`: the chain-reuse checks of the render targets (SA2's
     12816-12835) and the texture slots (12920-13023) now use `d3d.h`'s offsets. A draw whose shader is missing from
     the cache is now logged once per shader ("Draws dropped").
     `out/shot_run.ps1` captures the window's own content (PrintWindow) instead of photographing the screen, and takes
     a series with `-Also "20,24,28"`.
     Left: the one pipeline that fails to create (VS 8763F4DD2B71AA32, PS 4D524BFF32A7299D and a depth-only variant),
     `sa2_setter_marks.h`'s `kMarkSnapshotEnd = 13088` (SA2's snapshot size; Silent Hill's is 12712, and the marks are off),
     and gameplay past the title.
4. **deko3d:** the shader chain (HLSL -> SPIR-V -> GLSL -> package, uam on the console), the deko3d back end, and the
   Switch build without NVK. Gate: a picture on the console.
   **The Switch today (2026-10-28).** The console build is still the stage 1 one (the emulated Xenos GPU); it is what
   `out/switch/shh.nro` holds, and that NRO predates the stage 1 SDK swap, so it has to be rebuilt before it is used as
   a reference again. `SHH_RENUT_RENDERER` is OFF for the Switch toolchain on purpose: `cmake/shh_renut_gpu.cmake` is
   the PC's D3D12/Vulkan path. What the console needs, in order:
   - the deko3d back end of plume (banjo-nx's `plume_deko3d*.cpp`, the `PLUME_DEKO3D` path in `thirdparty/plume`) and
     the `uam` that compiles variant shaders on the console (vendored in `app/thirdparty/uam`);
   - devkitPro's deko3d: it is **not installed** on this machine (no `libdeko3d.a` under `C:\devkitPro`), so it has to
     be installed first;
   - the static NVK driver: no `libvulkan.a` exists anywhere on this machine either, although the Mesa tree it is built
     from is at `C:\dev\mesa-switch` (`tools/build_mesa.sh`, and `REXGLUE_SWITCH_NVK_SDK` points at its
     `portlibs/switch`), so `tools\build_mesa.sh` has to run again before `tools\build_switch.bat <nvk_sdk>`;
   - the game shaders' GLSL package for deko3d (`tools/deko3d_shader_package.py` in sa2-nx) and the shader cache built on
     the PC (`tools/renut_switch_shader_cache.py`), both of which first need stage 3's dump of Silent Hill's containers;
   - `tools/build_switch.bat` takes the NVK SDK folder as its argument, and the CMake tree in `out/switch` also needs
     the Renut pieces' Switch settings (plume's `PLUME_PLATFORM_SWITCH`/`PLUME_DEKO3D`, the `-mearly-ra=none` and LTO
     options of sa2-nx's CMakeLists), which are not ported yet.
5. **The console:** render-thread work, pacing, then the browser installer for the shader package.

## Open points

- DrawVerticesUP / BeginVertices look unlinked: confirm at run time.
- InsertCallback, SetRingBufferParameters, RingBufferDeviceAllocate and CBlocker::Check are not identified yet.
- Bink: the title movie is decoded on the CPU into textures, which Renut uploads as it does any texture. The
  emulated-read fix in `sdk/` (hot pages) belongs to the emulated GPU's watching and must be re-checked after stage 1.
- The game's shader containers: where they live (a bundle in the PAKs, or created at load) decides how the installer
  finds them.
