// banjo-nx: the boot-time choice between the emulated Xenos GPU and Renut's renderer (gpu/banjo_renut_select.h).
//
// shh-nx (2026-10-28): SHH's key (shh_renderer). The PC starts on the emulated GPU exactly as before ("xenos"), and
// "renut" switches to Renut's Direct3D-level renderer, which has no PM4 ring and no EDRAM. A Switch build without the
// Vulkan driver (REXGLUE_SWITCH_NO_NVK, stage 4) has no other renderer: Renut on deko3d (sa2-nx's branch, 2026-10-29).
#include "renut_engine/gpu/banjo_renut_select.h"

#include <rex/cvar.h>
#include <rex/logging.h>

#if defined(PLUME_DEKO3D)
#include "renut_engine/gpu/banjo_renut_deko3d.h"  // sa2-nx 1.0.3: MissingGameShaders
#endif

REXCVAR_DEFINE_STRING(shh_renderer, "xenos", "SHH",
                      "Renderer, chosen at boot. xenos = ReXGlue's emulated Xenos GPU (PM4 ring and EDRAM). renut = "
                      "Renut's Direct3D-level renderer (github.com/masterspike52/reNut, through banjo-nx): the game's "
                      "Direct3D draws natively through plume (deko3d on the Switch), with no PM4 ring and no EDRAM; its "
                      "own renut_* keys apply")
    .allowed({"xenos", "renut"})
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace banjo::renut_gpu {

namespace detail {
int8_t g_active = 0;
bool LatchActive() {
#if SA2_SWITCH_NO_NVK
  // sa2-nx (2026-10-20): a Switch build without the Vulkan driver (REXGLUE_SWITCH_NO_NVK) has no other renderer.
  static const bool active = [] {
#if defined(PLUME_DEKO3D)
    if (const size_t missing = deko3d::MissingGameShaders()) {
      REXLOG_ERROR("[deko3d] {} of the game's shaders are in no GLSL package; their draws are skipped", missing);
    }
#endif
    return true;
  }();
#else
  static const bool active = REXCVAR_GET(shh_renderer) == "renut";
#endif
  g_active = active ? 1 : -1;
  return active;
}
}  // namespace detail

}  // namespace banjo::renut_gpu
