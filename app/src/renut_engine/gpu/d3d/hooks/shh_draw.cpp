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

// shh-nx: Silent Hill keeps its streams' sources and strides in the device but builds the vertex fetch constants only
// when it flushes a draw's state, in the ring, which this renderer does not parse; without them the draws had no
// vertices at all (the constants read type 1 with address 0). Rebuild each stream's constant from its source, stride
// and the vertex buffer's header, exactly as D3D's SetStreamSource does (d3d_native.h), right before the draw's device
// snapshot is taken.
void FillVertexFetchConstants(u32 device) {
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
