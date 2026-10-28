// banjo-nx: the boot-time choice between the emulated Xenos GPU and Renut's renderer (gpu/banjo_renut_select.h).
//
// shh-nx (2026-10-28): SA2's key (sa2_renderer) and its no-NVK Switch branch were dropped here. SHH starts on the
// emulated GPU exactly as before ("xenos"), and "renut" switches to Renut's Direct3D-level renderer, which has no PM4
// ring and no EDRAM.
#include "renut_engine/gpu/banjo_renut_select.h"

#include <rex/cvar.h>
#include <rex/logging.h>

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
  static const bool active = REXCVAR_GET(shh_renderer) == "renut";
  g_active = active ? 1 : -1;
  return active;
}
}  // namespace detail

}  // namespace banjo::renut_gpu
