/**
 * @file    gpu/d3d/hooks/shh_draw.cpp
 * @brief   shh-nx (2026-10-29): the game's draws and indices, handed to the renderer (stage 3).
 *
 * Silent Hill's Direct3D has N&B's layout and two draw entry points, each called only by the engine's own draw
 * functions (docs/d3d-censo.md): DrawVertices 82908408 and DrawIndexedVertices 829087F8. Their arguments are D3D's;
 * the renderer reads the rest of the state from the device as it is at the call.
 *
 * The state setters are not hooked: the game's own Set* calls write the device fields at N&B's offsets, which is what
 * the draws' device snapshot holds (d3d.h). Two pieces are here because the device alone does not have them: the index
 * buffer, which the game passes as an argument, and the vertex fetch constants, which Silent Hill builds when it
 * flushes a draw's state into the ring (below).
 *
 * 82908128 is not hooked on purpose. The census lists it as SetRenderTarget/SetDepthStencilSurface, but the calls it
 * receives pass a surface in r4 and 0 in r5, and hooking it as (device, index, surface) wrote a surface address into
 * the render target slot: every draw then went to a broken 1x1 target and the window stayed black. Without the hook the
 * game's own writes to the device are what the draws read, and the title renders.
 */
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/types.h>

#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/backend/queued.h"
#include "renut_engine/gpu/backend/render_thread.h"
#include "renut_engine/gpu/backend/video.h"
#include "renut_engine/gpu/d3d/d3d.h"
#include "renut_engine/gpu/d3d/d3d_native.h"
#include "renut_engine/gpu/guest/command_buffers.h"
#include "renut_engine/gpu/guest/guest_memory.h"
#include "renut_engine/gpu/log.h"
#include "renut_engine/gpu/render/draw.h"
#include "renut_engine/gpu/render/surfaces.h"
#include "renut_engine/shh_compat/fast_boot.h"

using namespace renut::gpu;

namespace {

// shh-nx: a rebuild of each stream's vertex fetch constant from its source and stride, now off by default.
//
// It was added when the constants looked empty, but that reading was of +0x6F0, the slot of stream 17: the constants
// run in reverse (N&B's VertexFetch[18] at +0x6F0, stream s at +0x6F0 + 8 * (17 - s), so stream 0 at +0x778).
// Silent Hill's own SetStreamSource (82907520: device, stream, vertex buffer, offset, stride) writes the constant
// there at once, with the stream offset added to the address and taken from the size, and tags the buffer's fence.
// The rebuild passes offset 0, so every draw from a batch past the start of a shared vertex buffer read the first
// batch's vertices: the copyright text drew only its first line and a half, the later batches over it.
REXCVAR_DEFINE_BOOL(shh_renut_rebuild_vertex_fetch, false, "Silent Hill/Renut",
                    "Diagnostic: rebuild the vertex fetch constants from the streams before each draw (stream offset "
                    "0), instead of using the ones Silent Hill's SetStreamSource writes");

void FillVertexFetchConstants(u32 device) {
  static const bool on = REXCVAR_GET(shh_renut_rebuild_vertex_fetch);
  if (!on)
    return;
  native::Device d = native::DeviceAt(device);
  if (!d)
    return;
  for (u32 stream = 0; stream < d3d::kMaxStreams; ++stream) {
    const u32 source = d.W(d3d::kDeviceOffsetStreamSources + stream * 4);
    if (!source)
      continue;
    const u32 stride4 = d.B(d3d::kDeviceOffsetStreamStrides + stream);
    if (!stride4)
      continue;
    native::SetStreamSource(d, stream, source, 0, stride4 * 4);
  }
}

REXCVAR_DEFINE_INT32(shh_renut_diag_small_draws, 0, "Silent Hill/Renut",
                     "Diagnostic: this many seconds after the first draw, log every draw of 8 vertices or fewer "
                     "(the 2D layer); 0 = off");

void SmallDrawLog(u32 device, const char *kind, u32 primitive, u32 count) {
  static const int32_t at = REXCVAR_GET(shh_renut_diag_small_draws);
  if (at <= 0 || count > 8)
    return;
  static const auto start = std::chrono::steady_clock::now();
  static int32_t logged = 0;
  if (std::chrono::steady_clock::now() - start < std::chrono::seconds(at))
    return;
  if (logged++ >= 40)
    return;
  native::Device d = native::DeviceAt(device);
  if (!d)
    return;
  char textures[64] = {};
  char streams[96] = {};
  for (u32 i = 0, n = 0; i < 8 && n + 16 < sizeof(textures); ++i) {
    const u32 t = d.W(d3d::kDeviceOffsetTextures + i * 4);
    if (t)
      n += fmt::format_to_n(textures + n, sizeof(textures) - 1 - n, " t{}={:08X}", i, t).size;
  }
  for (u32 i = 0, n = 0; i < 4 && n + 24 < sizeof(streams); ++i) {
    const u32 src = d.W(d3d::kDeviceOffsetStreamSources + i * 4);
    n += fmt::format_to_n(streams + n, sizeof(streams) - 1 - n, " s{}={:08X}/{}", i, src,
                          u32(d.B(d3d::kDeviceOffsetStreamStrides + i)) * 4).size;
  }
  RGPU_INFO("[smalldraw] {} prim {} count {}: vs {:08X} ps {:08X} decl {:08X} targets {:08X},{:08X} depth {:08X}{}{}",
            kind, primitive, count, d.W(d3d::kDeviceOffsetVertexShader), d.W(d3d::kDeviceOffsetPixelShader),
            d.W(d3d::kDeviceOffsetVertexDeclaration), d.W(d3d::kDeviceOffsetRenderTargets),
            d.W(d3d::kDeviceOffsetRenderTargets + 4), d.W(d3d::kDeviceOffsetDepthStencil), textures, streams);
  // The texture fetch the renderer reads for slot 0: the resource's own (BaseTexture::Format, +0x1C) and the device's
  // (Constants::TextureFetch[0], +0x480).
  static constexpr u32 kDeviceTextureFetch = 0x480;
  const u32 t0 = d.W(d3d::kDeviceOffsetTextures);
  fmt::memory_buffer line;
  fmt::format_to(std::back_inserter(line), "[texfetch] slot 0");
  for (u32 i = 0; i < 6; ++i)
    fmt::format_to(std::back_inserter(line), " dev{:08X}", d.W(kDeviceTextureFetch + i * 4));
  if (t0) {
    const auto *res = GuestPtr<const be_u32>(t0 + offsetof(d3d::BaseTexture, Format));
    for (u32 i = 0; res && i < 6; ++i)
      fmt::format_to(std::back_inserter(line), " res{:08X}", u32(res[i]));
  }
  RGPU_INFO("{}", fmt::to_string(line));
  // The vertex declaration's own words (the game's D3DVERTEXELEMENT9 list, 12 bytes an element).
  const u32 decl_va = d.W(d3d::kDeviceOffsetVertexDeclaration);
  if (decl_va) {
    const auto *elem = GuestPtr<const be_u32>(decl_va);
    if (elem) {
      fmt::memory_buffer decl_line;
      fmt::format_to(std::back_inserter(decl_line), "[decl] {:08X}:", decl_va);
      for (u32 i = 0; i < 24; ++i)
        fmt::format_to(std::back_inserter(decl_line), " {:08X}", u32(elem[i]));
      RGPU_INFO("{}", fmt::to_string(decl_line));
    }
  }
}

void QueueOrRecord(u32 device, DrawCall &call) {
  if (IsRecordingCommandBuffer(device))
    RecordDraw(device, call, 0, 0);
  else
    QueueDraw(device, call, 0, 0);
}

// void D3DDevice_DrawVertices(D3DDevice*, D3DPRIMITIVETYPE, UINT StartVertex, UINT VertexCount)
void D3DDevice_DrawVertices_hook(u32 device, u32 primitive, u32 start_vertex, u32 vertex_count) {
  FillVertexFetchConstants(device);
  DrawCall call;
  call.primitive = primitive;
  call.start_vertex = start_vertex;
  call.count = vertex_count;
  SmallDrawLog(device, "vertices", primitive, vertex_count);
  QueueOrRecord(device, call);
}

// void D3DDevice_DrawIndexedVertices(D3DDevice*, D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT StartIndex,
//     UINT IndexCount)
void D3DDevice_DrawIndexedVertices_hook(u32 device, u32 primitive, u32 base_vertex, u32 start_index,
                                        u32 index_count) {
  FillVertexFetchConstants(device);
  DrawCall call;
  call.primitive = primitive;
  call.indexed = true;
  call.base_vertex = i32(base_vertex);
  call.start_index = start_index;
  call.count = index_count;
  SmallDrawLog(device, "indices", primitive, index_count);
  QueueOrRecord(device, call);
}

// void D3DDevice_SetIndices(D3DDevice*, D3DIndexBuffer*)
void D3DDevice_SetIndices_hook(u32 device, u32 index_buffer) {
  native::SetIndices(native::DeviceAt(device), index_buffer);
}

constexpr u32 kE_OUTOFMEMORY = 0x8007000Eu;
constexpr u32 kRenderStateHiZWriteEnable = 97;

bool Active() {
  auto &s = State();
  return s.host_ready && !s.shutting_down.load(std::memory_order_acquire);
}

bool ReadRect(u32 va, ClearRect &rect) {
  const auto *p = GuestPtr<const be_u32>(va);
  if (!p)
    return false;
  rect = {i32(u32(p[0])), i32(u32(p[1])), i32(u32(p[2])), i32(u32(p[3]))};
  return true;
}

bool ReadVector4(u32 va, float out[4]) {
  const auto *p = GuestPtr<const be_u32>(va);
  if (!p)
    return false;
  for (int i = 0; i < 4; ++i) {
    const u32 bits = p[i];
    std::memcpy(&out[i], &bits, 4);
  }
  return true;
}

void Clear(u32 device, u32 flags, const ClearRect *rect, const float color[4], float z,
           u32 stencil, bool whole_target) {
  const ClearRect *host_rect = whole_target ? nullptr : rect;
  if (IsRecordingCommandBuffer(device))
    RecordClear(device, flags, host_rect, color, z, stencil);
  else if (Active())
    QueueClear(device, flags, host_rect, color, z, stencil);
  const native::Rect native_rect = rect ? native::Rect{rect->x1, rect->y1, rect->x2, rect->y2}
                                        : native::Rect{};
  native::Clear(native::DeviceAt(device), flags, rect ? &native_rect : nullptr);
}

void Resolve(u32 device, u32 flags, const ClearRect *rect, u32 dest_texture, i32 dest_x,
             i32 dest_y, u32 dest_level, u32 dest_slice, u32 color_va, float clear_z,
             u32 clear_stencil, u32 parameters) {
  float color[4] = {0, 0, 0, 0};
  const bool has_color = color_va && ReadVector4(color_va, color);
  if (IsRecordingCommandBuffer(device)) {
    RecordResolve(device, flags, rect, dest_texture, dest_x, dest_y, dest_level, dest_slice,
                  has_color ? color : nullptr, clear_z, clear_stencil);
  } else if (Active()) {
    QueueResolve(device, flags, rect, dest_texture, dest_x, dest_y, dest_level, dest_slice,
                 has_color ? color : nullptr, clear_z, clear_stencil);
  }
  native::Resolve(native::DeviceAt(device), flags, dest_texture, clear_z, clear_stencil,
                  parameters);
}

void D3DDevice_BeginTiling(u32 device, u32 flags, u32 count, u32 rects_va, u32 color_va,
                           float clear_z, u32 clear_stencil) {
  native::Device d = native::DeviceAt(device);
  if (!d)
    return;
  std::vector<ClearRect> tiles;
  for (u32 i = 0; i < count; ++i) {
    ClearRect rect;
    if (!ReadRect(rects_va + i * 16, rect))
      break;
    tiles.push_back(rect);
  }
  count = u32(tiles.size());
  u32 width = 0, height = 0;
  for (const ClearRect &rect : tiles) {
    width = std::max(width, u32(rect.x2));
    height = std::max(height, u32(rect.y2));
  }
  shh::fast_boot::End();  // the title screen's 3D scene is the game's first tiled frame
  static u32 s_tiling_logs = 0;
  if (s_tiling_logs < 10) {
    ++s_tiling_logs;
    RGPU_INFO("[tiling] BeginTiling: {} tiles, extent {}x{}", count, width, height);
  }
  if (!IsRecordingCommandBuffer(device)) {
    FlushDeferredVertices(device);
    render_thread::Queue([tiles, width, height] {
      SetTilingExtentLocked(width, height);
      SetTilesLocked(tiles.data(), u32(tiles.size()));
    });
  }

  d.SetW(native::off::TileCount, count);
  u32 extent_x = 0, extent_y = 0;
  for (u32 i = 0; i < count; ++i) {
    const ClearRect &rect = tiles[i];
    extent_x = std::max(extent_x, u32(rect.x2));
    extent_y = std::max(extent_y, u32(rect.y2));
    d.SetW(native::off::TileRects + i * 16, u32(rect.x1));
    d.SetW(native::off::TileRects + i * 16 + 4, u32(rect.y1));
    d.SetW(native::off::TileRects + i * 16 + 8, u32(rect.x2));
    d.SetW(native::off::TileRects + i * 16 + 12, u32(rect.y2));
    d.SetW(native::off::TileOrigins + i * 8, u32(rect.x1) & ~31u);
    d.SetW(native::off::TileOrigins + i * 8 + 4, u32(rect.y1) & ~31u);
  }
  d.SetW(native::off::TilingExtent, extent_x);
  d.SetW(native::off::TilingExtent + 4, extent_y);
  d.SetW(0x3384, 0);

  if (!(d.B(native::off::Flags3) & 0x20)) {
    // The largest tile, from its origin: its clear covers a tile of EDRAM.
    u32 tile_w = 0, tile_h = 0;
    for (u32 i = 0; i < count; ++i) {
      tile_w = std::max(tile_w, u32(tiles[i].x2) - d.W(native::off::TileOrigins + i * 8));
      tile_h = std::max(tile_h, u32(tiles[i].y2) - d.W(native::off::TileOrigins + i * 8 + 4));
    }
    // The viewport (x, y, width, height, min z, max z) and scissor rect, for after the clear.
    float viewport[6];
    for (u32 i = 0; i < 6; ++i)
      viewport[i] = d.F(0x3168 + i * 4);
    i32 scissor[4];
    for (u32 i = 0; i < 4; ++i)
      scissor[i] = i32(d.W(native::off::ScissorRect + i * 4));
    native::SetViewport(d, 0.0f, 0.0f, 65535.0f, 65535.0f, 0.0f, 1.0f);
    native::SetScissorRect(d, 0, 0, 0xFFFF, 0xFFFF);
    const ClearRect rect{0, 0, i32(tile_w), i32(tile_h)};
    float color[4] = {0, 0, 0, 0};
    if (color_va)
      ReadVector4(color_va, color);
    Clear(device, 0x3F, &rect, color, 0.0f, 0, true);
    native::SetViewport(d, viewport[0], viewport[1], viewport[2], viewport[3], viewport[4],
                        viewport[5]);
    native::SetScissorRect(d, scissor[0], scissor[1], scissor[2], scissor[3]);
  }
  if (const auto *color = color_va ? GuestPtr<const u8>(color_va) : nullptr)
    std::memcpy(d.bytes + 0x3390, color, 16);
  else
    std::memset(d.bytes + 0x3390, 0, 16);
  d.SetF(0x33A0, 0.0f);
  d.SetW(0x33A4, 0);

  const u32 target = d.W(native::off::RenderTargets);
  d.SetW(0x31B4, target ? target : d.W(native::off::DepthStencil));
  for (u32 i = 0; i < 5; ++i)  // the 4 targets, then depth
    d.SetW(native::off::TilingSurfaces + i * 4, d.W(native::off::RenderTargets + i * 4));
  d.SetW(0x31B0, 2);
  d.SetB(native::off::Flags, d.B(native::off::Flags) | 0x20);
  d.SetW(native::off::PredicationSet, 0);
  native::UpdateTilePredicated(d);
  d.SetW(native::off::RunPredicationState, 0);
  d.SetW(native::off::Predication, ~0u);
  d.SetW(native::off::TilePredication, 0);
  d.SetW(native::off::RunPredication, 0);
  // The tiles' commands go into a ring segment of their own from a KickOff.
  native::KickOff(device);
  native::SetViewport(d, 0.0f, 0.0f, 65535.0f, 65535.0f, 0.0f, 1.0f);
  native::ResetAllState(d);
}

u32 D3DDevice_EndTiling(u32 device, u32 resolve_flags, u32 rects_va, u32 dest_texture,
                        u32 color_va, float clear_z, u32 clear_stencil, u32 parameters) {
  native::Device d = native::DeviceAt(device);
  if (!d)
    return 0;
  const u32 count = d.W(native::off::TileCount);
  if (dest_texture) {
    for (u32 i = 0; i < count; ++i) {
      const u32 tile = native::off::TileRects + i * 16;
      ClearRect rect{i32(d.W(tile)), i32(d.W(tile + 4)), i32(d.W(tile + 8)), i32(d.W(tile + 12))};
      // A tile at the surface's edge resolves to a multiple of 8.
      const u32 surface = d.W(0x31B4);
      const auto *s = surface ? GuestPtr<const be_u32>(surface) : nullptr;
      const u32 size = s ? u32(s[9]) : 0;
      if (u32(rect.x2) == (size >> 18) + 1)
        rect.x2 = i32((u32(rect.x2) + 7) & ~7u);
      if (u32(rect.y2) == native::Extract(size, 3, 15) + 1)
        rect.y2 = i32((u32(rect.y2) + 7) & ~7u);
      native::SetPredication(d, 3u << (i * 2));
      native::SetRenderState(d, kRenderStateHiZWriteEnable,
                             (d.B(native::off::Flags3) & 0x20) ? 0 : 2);
      const i32 dest_x = i32(d.W(tile)), dest_y = i32(d.W(tile + 4));
      if (i + 1 < count) {
        Resolve(device, 0x300, &rect, dest_texture, dest_x, dest_y, 0, 0, device + 0x3390,
                d.F(0x33A0), d.W(0x33A4), 0);
      } else {
        Resolve(device, 0x300, &rect, dest_texture, dest_x, dest_y, 0, 0, color_va,
                clear_z, clear_stencil, parameters);
      }
    }
  }
  native::SetPredication(d, 0);
  native::SetRenderState(d, kRenderStateHiZWriteEnable, 2);
  if (count != 1) {
    // Around the WorkerDpc callback.
    native::SetPredication(d, 0x80000000u);
    native::SetPredication(d, 0);
  }
  native::KickOff(device);

  d.SetB(native::off::Flags3, d.B(native::off::Flags3) & 0xCF);
  d.SetB(native::off::Flags, d.B(native::off::Flags) & 0xDF);
  d.SetW(0x31B4, 0);
  d.SetW(native::off::PredicationSet, 0);
  native::UpdateTilePredicated(d);
  d.SetW(native::off::RunPredicationState, 0);
  d.SetW(native::off::Predication, ~0u);
  d.SetW(native::off::TilePredication, 0);
  d.SetW(native::off::RunPredication, 0);
  // The segment's fence, unless the device's command memory ran out.
  const bool out_of_memory = (d.B(native::off::Flags1) & 0x20) != 0;
  if (out_of_memory) {
    d.SetB(native::off::Flags1, d.B(native::off::Flags1) & 0xDF);
    native::RewindRing(d);
  } else {
    native::SetFence(device);
  }
  native::SetViewport(d, 0.0f, 0.0f, 65535.0f, 65535.0f, 0.0f, 1.0f);

  if (!IsRecordingCommandBuffer(device)) {
    render_thread::Queue([] {
      SetTilingExtentLocked(0, 0);
      SetTilesLocked(nullptr, 0);
    });
  }
  if (out_of_memory) {
    native::ResetAllState(d);
    return kE_OUTOFMEMORY;
  }
  return 0;
}

}  // namespace

BANJO_RENUT_HOOK(sub_82908408, D3DDevice_DrawVertices_hook);         // DrawVertices
BANJO_RENUT_HOOK(sub_829087F8, D3DDevice_DrawIndexedVertices_hook);  // DrawIndexedVertices
BANJO_RENUT_HOOK(sub_82907640, D3DDevice_SetIndices_hook);           // SetIndices

// void D3DDevice_BeginTiling(D3DDevice*, DWORD Flags, UINT Count, const D3DRECT *pTileRects,
//     const D3DVECTOR4 *pClearColor, float ClearZ, DWORD ClearStencil) [sub_82910B70]
BANJO_RENUT_HOOK_RAW(sub_82910B70) {
  const u32 device = ctx.r3.u32;
  const u32 flags = ctx.r4.u32;
  const u32 count = ctx.r5.u32;
  const u32 rects_va = ctx.r6.u32;
  const u32 color_va = ctx.r7.u32;
  const float clear_z = float(ctx.f1.f64);
  const u32 clear_stencil = ctx.r9.u32;
  D3DDevice_BeginTiling(device, flags, count, rects_va, color_va, clear_z, clear_stencil);
}

// HRESULT D3DDevice_EndTiling(D3DDevice*, DWORD ResolveFlags, const D3DRECT *pResolveRects,
//     D3DBaseTexture *pDestTexture, const D3DVECTOR4 *pClearColor, float ClearZ, DWORD ClearStencil,
//     const D3DRESOLVE_PARAMETERS *pParameters) [sub_82911000]
BANJO_RENUT_HOOK_RAW(sub_82911000) {
  const u32 device = ctx.r3.u32;
  const u32 resolve_flags = ctx.r4.u32;
  const u32 rects_va = ctx.r5.u32;
  const u32 dest_texture = ctx.r6.u32;
  const u32 color_va = ctx.r7.u32;
  const float clear_z = float(ctx.f1.f64);
  const u32 clear_stencil = ctx.r9.u32;
  const u32 parameters = ctx.r10.u32;
  ctx.r3.u64 = D3DDevice_EndTiling(device, resolve_flags, rects_va, dest_texture, color_va,
                                  clear_z, clear_stencil, parameters);
}
