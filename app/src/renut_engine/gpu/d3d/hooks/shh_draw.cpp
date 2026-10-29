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
void D3DDevice_DrawVertices_hook(u32 device, u32 primitive, u32 start_vertex, u32 vertex_count) {
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
