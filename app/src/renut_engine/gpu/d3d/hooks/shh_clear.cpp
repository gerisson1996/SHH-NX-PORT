/**
 * @file    gpu/d3d/hooks/shh_clear.cpp
 * @brief   shh-nx (2026-10-28): the game's clear, handed to the renderer (stage 2).
 *
 * Silent Hill's Direct3D keeps Clear (8290E650, eleven game callers) and ClearF (8290E528) apart: Clear walks its
 * argument list and calls ClearF once per rectangle, and the game calls ClearF directly too, so ClearF is the hook
 * (docs/d3d-censo.md). The host work is queued from the device as it is at the call, and the original then does its own
 * device work: its packets go into the ring sink and its inline data into the scratch (device.cpp).
 *
 * Adapted from sa2-nx's d3d/hooks/draw.cpp, which carries the same ClearF hook among N&B's draws and resolves; those
 * still wait for SHH's addresses (docs/plano-renut.md, stage 3).
 */
#include <cstring>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/types.h>

#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/backend/queued.h"
#include "renut_engine/gpu/guest/guest_memory.h"
#include "renut_engine/gpu/render/draw.h"

using namespace renut::gpu;

// shh-nx (2026-10-28): a test key for stage 2 and 3: the game's clear (black at the title, and every frame) is
// replaced by magenta, so a screenshot tells whether the clear reaches the screen or the renderer is only presenting
// an empty frame. Nothing a build ships with it on.
REXCVAR_DEFINE_BOOL(shh_renut_clear_test, false, "SHH/Renderer",
                    "Renut's renderer: clear with magenta instead of the game's colour, to check that the game's clear "
                    "reaches the screen")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

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

}  // namespace

// void D3DDevice_ClearF(D3DDevice*, DWORD Flags, const D3DRECT *pRect, const D3DVECTOR4 *pColor, float Z,
//     DWORD Stencil) [sub_8290E528]: Z in f1 (r7's slot), Stencil in r8.
BANJO_RENUT_HOOK_RAW(sub_8290E528) {
  const u32 device = ctx.r3.u32, flags = ctx.r4.u32, rect_va = ctx.r5.u32, color_va = ctx.r6.u32;
  ClearRect rect;
  const bool has_rect = rect_va && ReadRect(rect_va, rect);
  float color[4] = {0, 0, 0, 0};
  if (color_va)
    ReadVector4(color_va, color);
  if (REXCVAR_GET(shh_renut_clear_test)) {
    color[0] = 1.0f;
    color[1] = 0.0f;
    color[2] = 1.0f;
    color[3] = 1.0f;
  }
  // Only the flags' targets that the call names are cleared; the renderer reads the rest from the device.
  QueueClear(device, flags, has_rect ? &rect : nullptr, color, float(ctx.f1.f64), ctx.r8.u32);
  __imp__sub_8290E528(ctx, base);
}
