/**
 * @file    gpu/d3d/hooks/shh_resolve.cpp
 * @brief   shh-nx (2026-10-28): the game's resolve, handed to the renderer (stage 2).
 *
 * Silent Hill's Direct3D resolves the frame's render targets into the textures it presents (docs/d3d-censo.md,
 * D3DDevice_Resolve 82914EA8). The Swap presents the front buffer, which is one of those destinations: without this
 * hook the renderer has no host texture for it and the Swap presents nothing.
 *
 * The argument mapping is sa2-nx's (d3d/hooks/draw.cpp): ClearZ in f1 (the slot of r7), ClearStencil and pParameters in
 * the caller's argument words at +92 and +100, which the guest function reads there.
 */
#include <cstring>

#include <rex/hook.h>
#include <rex/types.h>

#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/backend/queued.h"
#include "renut_engine/gpu/guest/guest_memory.h"
#include "renut_engine/gpu/render/draw.h"

using namespace renut::gpu;

namespace {

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

u32 LoadU32(u32 va) {
  const auto *p = GuestPtr<const be_u32>(va);
  return p ? u32(*p) : 0;
}

}  // namespace

// void D3DDevice_Resolve(D3DDevice*, DWORD Flags, const D3DRECT *pSourceRect, D3DBaseTexture *pDestTexture,
//     const D3DPOINT *pDestPoint, UINT DestLevel, UINT DestSliceOrFace, const D3DVECTOR4 *pClearColor, float ClearZ,
//     DWORD ClearStencil, const D3DRESOLVE_PARAMETERS *pParameters) [sub_82914EA8]
BANJO_RENUT_HOOK_RAW(sub_82914EA8) {
  const u32 device = ctx.r3.u32, flags = ctx.r4.u32, rect_va = ctx.r5.u32, dest_texture = ctx.r6.u32;
  const u32 point_va = ctx.r7.u32, dest_level = ctx.r8.u32, dest_slice = ctx.r9.u32, color_va = ctx.r10.u32;
  const u32 clear_stencil = LoadU32(ctx.r1.u32 + 92);
  ClearRect rect;
  const bool has_rect = rect_va && ReadRect(rect_va, rect);
  i32 dest_x = 0, dest_y = 0;
  if (const auto *p = point_va ? GuestPtr<const be_u32>(point_va) : nullptr) {
    dest_x = i32(u32(p[0]));
    dest_y = i32(u32(p[1]));
  }
  float color[4] = {0, 0, 0, 0};
  const bool has_color = color_va && ReadVector4(color_va, color);
  QueueResolve(device, flags, has_rect ? &rect : nullptr, dest_texture, dest_x, dest_y, dest_level, dest_slice,
               has_color ? color : nullptr, float(ctx.f1.f64), clear_stencil);
  __imp__sub_82914EA8(ctx, base);
}
