# Silent Hill: Homecoming's Direct3D: census for Renut

Goal: replace the emulated Xenos GPU (slow and wrong on the Switch, see the console logs of 2026-10-28) with Renut's
Direct3D-level renderer on plume/deko3d, as sa2-nx, mk9-nx, mgr-nx-deko and naruto-rise-of-a-ninja-nx do. Reference
clones: `C:\dev\sa2-nx-ref` (sa2-nx 32a81a8: `docs/deko3d-renderer-guide.md`, `docs/renut-renderer.md`,
`app/src/renut_engine`). Naruto's notes (`docs/renut-retarget.md`, `docs/d3d-census.md`) are the closest case: XDK 6274
and no reference image.

- **XDK:** 2.0.6690 (import libraries in the XEX header). N&B is 7645, SA2 21076, Naruto 6274.
- **No reference image** (N&B or SA2) is available, so `d3d_match.py` cannot be used. Each function is named by its
  evidence from `tools/d3d_censo.py` (call graph, kernel imports, GPU MMIO, device offsets).
- **The Direct3D library** sits at about 0x82904000-0x82924000. 0x82926xxx and later is the CRT (register save/restore
  helpers and so on).

Status: hypothesis until a Renut hook on it works.

## Device layout: N&B's

Renut was written for N&B, and sa2-nx's `renut_engine/gpu/d3d/d3d.h` and `d3d_native.h` give N&B's value of every
offset in brackets. Every one checked so far is the same in Silent Hill:

| Field | SHH | N&B | Where SHH shows it |
|---|---|---|---|
| device size | 24064 (0x5E00) | 0x5E00 | Direct3D_CreateDevice allocates 24064 aligned to 128 |
| flags, no-hardware bit 0x80 | +10940 | 0x2ABC | CreateDevice sets 0x80 for device type 2 |
| flags 1 (immediate fences 0x02, out of memory 0x20) | +10941 | 0x2ABD | KickOff, AllocateInlineData |
| completed-fence pointer / current fence | +10896 / +10908 | 0x2A90 / 0x2A9C | KickOff, BlockOnFence, IsFencePending |
| in-KickOff word | +13232 | 13232 | KickOff, BlockOnFence |
| critical sections | +14944 / +14972 | 14944 / 14972 | hardware init |
| words set to -1 | +21560 / +21564 | 21560 / 21564 | hardware init |
| index buffer | +12436 | 0x3094 | 829087F8, 82907640 |
| render targets / depth | +12440.. / +12456 | 0x3098 / 0x30A8 | 82908128, Resolve |
| swap count | +16560 | 0x40B0 | Swap |
| side buffer | +16728 | 16728 | 8290EC20, 8290F7A0 |
| behaviour flags | +23932 | ? | CreateDevice: `oris 0x0C00` unless flags 0x100 / 0x3F000000, as SA2's |
| VdGlobalDevice import slot | 0x82000570 | - | `lis r11,-32256; lwz r11,1392(r11)` |
| ring pointer / limit | +0x30 / +0x34 | 0x30 / 0x34 | BeginRingBig |

So Renut's N&B values (the brackets) apply, not SA2's shifted ones. Each remaining offset still has to be checked
in SHH's own code before it is used.

Shader objects: vertex shader container at +0x368 (872), as N&B's; pixel shader container at +0x28 and physical part
at +0x18 (CreatePixelShader).

## Functions

| SHH | Function (hypothesis) | Evidence |
|---|---|---|
| 82910928 | Direct3D_CreateDevice | called by the game (82511250); calls the hardware init 82919C88; +10940 |
| 82919C88 | hardware device initialisation | RtlInitializeCriticalSection, VdIsHSIOTrainingSucceeded, calls the ring init 8290FF10 and 82919B98 (VdInitializeEngines, VdSetGraphicsInterruptCallback) |
| 82919F70 | device destruction | VdShutdownEngines, VdSetGraphicsInterruptCallback |
| 8290FF10 | ring initialisation | VdInitializeRingBuffer, VdEnableRingBufferRPtrWriteBack, MmGetPhysicalAddress; +10896 |
| 82911D68 | D3DDevice_Swap | the only caller of VdSwap and VdGetSystemCommandBuffer; VdPersistDisplay; calls BlockOnFence and KickOff. The game's render thread spun in it on the console |
| 8290FBB8 | CDevice::KickOff | 64 callers in Direct3D; +10940/+10941 flags, +10896, +10908, +13232 |
| 8290F3C8 | the kick's submission | spin lock; calls 8290EF30 |
| 8290EF30 | KickOff (internal) | GPU MMIO (CP_RB_WPTR) |
| 8290F4A0 | CDevice::BlockOnFence | +10896 / +10908; calls KickOff and the wait 82910448 |
| 82910448 | the fence wait loop (CBlocker::Check?) | db16cyc spin with a 5000-tick timeout, calls 829193E8 |
| 82910350 / 82910380 | CBlocker begin / end (?) | called with 82910448 by the same six functions |
| 8290FA30 | segment allocator (?) | critical section +14928; ring fields +13376..13540 |
| 8290F8A0 | ring wrap / BeginRingBig (?) | ring fields +14912..14940 |
| 82905xxx | state setters (SetRenderState and similar) | many game callers, each writes one register packet word (+10548, +10460, ...) |
| 82908408 | DrawVertices (?) | twin of 829087F8 without the index buffer; the same two game callers (82452318, 82518748); calls the ring (8290F7A0), KickOff and the state flushes 82916A60..82917F20 |
| 829087F8 | DrawIndexedVertices (?) | as 82908408 plus the index buffer +12436 |
| 82907640 | SetIndices (?) | index buffer +12436; 4 game callers |
| 82908128 | SetRenderTarget / SetDepthStencilSurface (?) | 35 game callers; render targets +12440/+12444, depth +12456, flags |
| 8290D238 | a per-draw setter (SetTexture? / SetStreamSource?) (?) | 92 game callers; +13524 |
| 82919EA0 | the no-hardware device setup (device type 2) | calls 8290EC20 (the side buffer as the ring), 829194F0, 82923310 |
| 829194F0 | InitializeApiState (?) | called by both setups; state setters, render targets |
| 82923310 | XenonGpu_Initialize (?) | called by both setups; register packets +10564.. |
| 82919758 | InitializePresentationParameters (?) | hardware init only, (device, D3DPRESENT_PARAMETERS*); VdQueryVideoMode |
| 8290FCF0 | **D3DDevice_IsFencePending(fence)** | through VdGlobalDevice; KickOff when the fence is the current one; compares with +10896 / +10908 |
| 8290F760 | D3DDevice_BlockOnFence(fence) | `BlockOnFence(VdGlobalDevice, fence, 6, 0, 0)` (tail call) |
| 8290FDD8 | **CDevice::BeginRingBig(device, count)** | ring +0x30 + count*4 against the limit +0x34, KickOff, then the segment allocator |
| 8290F8A0 | the ring segment allocator (device, dwords) | called by BeginRingBig, Swap and 8290FA30 |
| 8290FA30 | CDevice::KickOffSegment (?) | called only by KickOff; calls the segment allocator |
| 8290EE48 | **CDevice::AllocateInlineData(device, dwords, alignment)** | out-of-memory bit 0x20 of +10941; allocates from +14932 against +14936 |
| 82909FE0 / 82909990 | **D3DDevice_CreateVertexShader / XGRegisterVertexShader** | the game calls the first; it allocates, copies, and calls the second, which uses the container at +0x368 |
| 82909ED0 | **D3DDevice_CreatePixelShader** (XGRegisterPixelShader inlined) | container copied to +0x28, physical part at +0x18, header `0x00100007` |
| 82909D60 | **D3DDevice_CreateVertexDeclaration** | counts 12-byte D3DVERTEXELEMENT9 to Stream 0xFF, allocates 56 + 12n, fills with 82909C70 |
| 8290E650 | D3DDevice_Clear | 11 game callers; calls ClearF |
| 8290E528 | **D3DDevice_ClearF** | the clear chain 8290DEB0 -> 8290D948 -> 8290D6A8 -> 8290D3B8; depth +12456, targets +12440.. |
| 82910B70 / 82911000 | BeginTiling / EndTiling (?) | one game caller each; tile rects +13376; Begin clears, End resolves (82914EA8) |
| 82907DC8 | the render target / window commit (Naruto's 82128270 "window scissor writer") (?) | 31 game calls through the stub 82908400; +10460 packets, flags, targets +12440..12456 |
| 8290A8E0 | D3DResource_Release | 6 callers, recursive; calls Destroy |
| 8290A1B0 | **D3DResource_Destroy** | only caller Release; BlockOnFence on the resource's fence, then frees (827D04E0) |
| 8290AA28 / 8290AB38 -> 8290A3A8 | a resource Lock (?) | waits with BlockOnFence through VdGlobalDevice |
| 82911B08 / 82911B88 | the two present paths of Swap (?) | behaviour flags bit 0x04 (+23932); 82911880 or a wait packet (6212 = 0x1844) and 82911A60 |
| 82911880 | SynchronizeToPresentationInterval (?) | called by both present paths; +10900, +10941, +13596 |
| 82914EA8 | D3DDevice_Resolve (?) | 915 instructions; called by 82911000, 8291AAA0 and the game (82519880); depth +12456, 0x2100/0x2200 packets |

Still to find: InsertCallback, SetRingBufferParameters, RingBufferDeviceAllocate (8290EC48 or 82915CF8 /
82915F68, which AllocateInlineData and the segment allocator call), CBlocker::Check (82910448?).

**Not found, probably not linked:** DrawVerticesUP and BeginVertices. No Direct3D function writes user vertices into
the ring, and the game reaches its draws only through two functions (82452318, 82518748). To be confirmed at run time:
hook the candidates and count calls.

The draws have only two game callers each, the engine's own draw functions, which makes them easy to trace.
