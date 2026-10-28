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
#include <atomic>
#include <cstring>

#include <rex/hook.h>
#include <rex/types.h>

#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/backend/queued.h"
#include "renut_engine/gpu/d3d/d3d.h"
#include "renut_engine/gpu/guest/guest_memory.h"
#include "renut_engine/gpu/log.h"
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
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1, std::memory_order_relaxed) < 4) {
    const auto *dev = GuestPtr<const be_u32>(device + d3d::kDeviceOffsetRenderTargets);
    const u32 target0 = dev ? u32(dev[0]) : 0;
    const u32 target1 = dev ? u32(dev[1]) : 0;
    const auto *depth = GuestPtr<const be_u32>(device + d3d::kDeviceOffsetDepthStencil);
    const u32 depth_va = depth ? u32(*depth) : 0;
    const auto *desc = target0 ? GuestPtr<const d3d::SurfaceDesc>(target0 + sizeof(d3d::Resource)) : nullptr;
    u32 words[8] = {};
    if (const auto *w = target0 ? GuestPtr<const be_u32>(target0 + 0x18) : nullptr)
      for (int i = 0; i < 8; ++i)
        words[i] = u32(w[i]);
    // shh-nx: Silent Hill's own headers, to find where each keeps its D3DFORMAT (the N&B word at +0x28 is not one).
    // The back buffer, the depth surface and the front buffer are dumped once each, 20 words from the object's start.
    static std::atomic<int> dumped{0};
    if (dumped.fetch_add(1, std::memory_order_relaxed) == 0) {
      for (const u32 va : {target0, depth_va, dest_texture}) {
        u32 h[20] = {};
        if (const auto *w = va ? GuestPtr<const be_u32>(va) : nullptr)
          for (int i = 0; i < 20; ++i)
            h[i] = u32(w[i]);
        RGPU_INFO("Header of {:#010x}: {:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x},"
                  "{:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x}",
                  va, h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], h[8], h[9], h[10], h[11], h[12], h[13], h[14],
                  h[15], h[16], h[17], h[18], h[19]);
      }
    }
    RGPU_INFO("Resolve device={:#010x} flags={:#x} rect={} dest={:#010x} point={},{} level={} slice={} colour={} "
              "targets={:#010x},{:#010x} depth={:#010x} target0 size={:#010x} format={} info={:#010x} "
              "desc={:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x},{:08x}",
              device, flags, has_rect ? "yes" : "no", dest_texture, dest_x, dest_y, dest_level, dest_slice,
              has_color ? "yes" : "no", target0, target1, depth_va, desc ? u32(desc->Size) : 0,
              desc ? u32(desc->Format) : 0, desc ? u32(desc->SurfaceInfo) : 0, words[0], words[1], words[2], words[3],
              words[4], words[5], words[6], words[7]);
  }
  QueueResolve(device, flags, has_rect ? &rect : nullptr, dest_texture, dest_x, dest_y, dest_level, dest_slice,
               has_color ? color : nullptr, float(ctx.f1.f64), clear_stencil);
  __imp__sub_82914EA8(ctx, base);
}
