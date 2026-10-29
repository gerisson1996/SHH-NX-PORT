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
#include <chrono>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/types.h>

#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/backend/queued.h"
#include "renut_engine/gpu/d3d/d3d.h"
#include "renut_engine/gpu/d3d/d3d_native.h"
#include "renut_engine/gpu/guest/command_buffers.h"
#include "renut_engine/gpu/guest/guest_memory.h"
#include "renut_engine/gpu/log.h"
#include "renut_engine/gpu/render/draw.h"

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

}  // namespace

BANJO_RENUT_HOOK(sub_82908408, D3DDevice_DrawVertices_hook);         // DrawVertices
BANJO_RENUT_HOOK(sub_829087F8, D3DDevice_DrawIndexedVertices_hook);  // DrawIndexedVertices
BANJO_RENUT_HOOK(sub_82907640, D3DDevice_SetIndices_hook);           // SetIndices
