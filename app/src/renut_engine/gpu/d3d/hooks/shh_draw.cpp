/**
 * @file    gpu/d3d/hooks/shh_draw.cpp
 * @brief   shh-nx (2026-10-28): the game's draws, indices and render targets, handed to the renderer (stage 3).
 *
 * Silent Hill's Direct3D has N&B's layout and two draw entry points, each called only by the engine's own draw
 * functions (docs/d3d-censo.md): DrawVertices 82908408 and DrawIndexedVertices 829087F8. Their arguments are D3D's;
 * the renderer reads the rest of the state from the device as it is at the call.
 *
 * The state setters are not hooked: the game's own Set* calls write the device fields at N&B's offsets, which is what
 * the draws' device snapshot holds (d3d.h). Only the pieces the renderer cannot read from the snapshot are here: the
 * index buffer (the game passes it as an argument) and the render target (Renut keeps its own host surface for it).
 */
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

void QueueOrRecord(u32 device, DrawCall &call) {
  if (IsRecordingCommandBuffer(device))
    RecordDraw(device, call, 0, 0);
  else
    QueueDraw(device, call, 0, 0);
}

// void D3DDevice_DrawVertices(D3DDevice*, D3DPRIMITIVETYPE, UINT StartVertex, UINT VertexCount)
// shh-nx (2026-10-29): temporary, stage 3: the first draws' texture bindings. Silent Hill writes its textures into
// the device at N&B's +12544; if these are all zero while the draw is textured, the offset is not Silent Hill's.
void LogTextureSlots(u32 device) {
  static int logged = 0;
  if (logged++ >= 6)
    return;
  const auto *slots = GuestPtr<const be_u32>(device + d3d::kDeviceOffsetTextures);
  if (!slots)
    return;
  RGPU_INFO("[shh] draw texture slots: {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x}", u32(slots[0]),
            u32(slots[1]), u32(slots[2]), u32(slots[3]), u32(slots[4]), u32(slots[5]), u32(slots[6]), u32(slots[7]));
  // shh-nx: the texture's own fetch constant (BaseTexture::Format, 24 bytes) against the device's slot 0. Silent Hill's
  // SetTexture copies one into the other, so if the device's copy is elsewhere, the renderer reads garbage: the scan
  // says where the same six words are in the device.
  const u32 texture_va = u32(slots[0]);
  const auto *header_fetch = texture_va ? GuestPtr<const be_u32>(texture_va + 0x1C) : nullptr;
  if (!header_fetch)
    return;
  u32 fetch[6];
  for (int i = 0; i < 6; ++i)
    fetch[i] = u32(header_fetch[i]);
  const auto *device_words = GuestPtr<const be_u32>(device);
  u32 found = 0;
  constexpr u32 kDeviceOffsetTextureFetch = 0x480;  // d3d_native.h off::TextureFetch, until SHH's is known
  for (u32 at = 0; at + 24 <= 24064 && device_words && !found; at += 4) {
    bool same = true;
    for (int i = 0; i < 6 && same; ++i)
      same = u32(device_words[at / 4 + i]) == fetch[i];
    if (same)
      found = at;
  }
  u32 device_fetch[6] = {};
  for (int i = 0; i < 6; ++i)
    device_fetch[i] = device_words ? u32(device_words[(kDeviceOffsetTextureFetch + i * 4) / 4]) : 0;
  {
    const auto *vfc = GuestPtr<const be_u32>(device + 0x6F0);
    const auto *srcs = GuestPtr<const be_u32>(device + d3d::kDeviceOffsetStreamSources);
    const auto *strides = GuestPtr<const be_u32>(device + d3d::kDeviceOffsetStreamStrides);
    if (vfc && srcs && strides)
      RGPU_INFO("[shh] stream 0 src {:08x} stride {} vfc {:08x} {:08x} {:08x} {:08x} {:08x} {:08x}", u32(srcs[0]),
                u32(strides[0]), u32(vfc[0]), u32(vfc[1]), u32(vfc[2]), u32(vfc[3]), u32(vfc[4]), u32(vfc[5]));
  }
  RGPU_INFO("[shh] device fetch at {:#x}: {:08x} {:08x} {:08x} {:08x} {:08x} {:08x}",
            kDeviceOffsetTextureFetch, device_fetch[0], device_fetch[1], device_fetch[2], device_fetch[3],
            device_fetch[4], device_fetch[5]);
  RGPU_INFO("[shh] texture {} fetch {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} -> device offset {:#x}", texture_va,
            fetch[0], fetch[1], fetch[2], fetch[3], fetch[4], fetch[5], found);
}

void D3DDevice_DrawVertices_hook(u32 device, u32 primitive, u32 start_vertex, u32 vertex_count) {
  LogTextureSlots(device);
  DrawCall call;
  call.primitive = primitive;
  call.start_vertex = start_vertex;
  call.count = vertex_count;
  QueueOrRecord(device, call);
}

// void D3DDevice_DrawIndexedVertices(D3DDevice*, D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT StartIndex,
//     UINT IndexCount)
void D3DDevice_DrawIndexedVertices_hook(u32 device, u32 primitive, u32 base_vertex, u32 start_index,
                                        u32 index_count) {
  DrawCall call;
  call.primitive = primitive;
  call.indexed = true;
  call.base_vertex = i32(base_vertex);
  call.start_index = start_index;
  call.count = index_count;
  QueueOrRecord(device, call);
}

// void D3DDevice_SetIndices(D3DDevice*, D3DIndexBuffer*)
void D3DDevice_SetIndices_hook(u32 device, u32 index_buffer) {
  native::SetIndices(native::DeviceAt(device), index_buffer);
}

// void D3DDevice_SetRenderTarget(D3DDevice*, UINT Index, D3DSurface*): the census lists this address for the render
// target and the depth/stencil surface together, so an index of 4 or more is taken as the depth surface.
void D3DDevice_SetRenderTarget_hook(u32 device, u32 index, u32 surface) {
  native::Device d = native::DeviceAt(device);
  if (!d)
    return;
  if (index < d3d::kMaxRenderTargets)
    native::SetRenderTarget(d, index, surface);
  else
    native::SetDepthStencilSurface(d, surface);
}

}  // namespace

BANJO_RENUT_HOOK(sub_82908408, D3DDevice_DrawVertices_hook);         // DrawVertices
BANJO_RENUT_HOOK(sub_829087F8, D3DDevice_DrawIndexedVertices_hook);  // DrawIndexedVertices
BANJO_RENUT_HOOK(sub_82907640, D3DDevice_SetIndices_hook);           // SetIndices
BANJO_RENUT_HOOK(sub_82908128, D3DDevice_SetRenderTarget_hook);      // SetRenderTarget / SetDepthStencilSurface
