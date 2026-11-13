// shh-nx (2026-10-29): the console's mode for Renut's renderer (sa2_render_mode.h), as sa2-nx reads it.
#include "me/sa2_render_mode.h"

#include <rex/platform.h>
#include <rex/ui/switch_saltynx.h>

#if REX_PLATFORM_SWITCH
#include <switch.h>
#endif

namespace sa2::render {

bool OledScreen() {
#if REX_PLATFORM_SWITCH
  // The model is read once (set:sys); 4 = SetSysProductModel_Aula, the OLED model.
  static const bool oled = [] {
    bool is_oled = false;
    if (R_SUCCEEDED(setsysInitialize())) {
      SetSysProductModel model = SetSysProductModel_Invalid;
      if (R_SUCCEEDED(setsysGetProductModel(&model)))
        is_oled = model == SetSysProductModel_Aula;
      setsysExit();
    }
    return is_oled;
  }();
  return oled && !Docked();
#else
  return false;
#endif
}

bool Docked() {
#if REX_PLATFORM_SWITCH
  // libnx caches the operation mode; the SDK's applet loop (windowed_app_context_switch.cpp) processes the
  // OperationModeChanged message that updates it. Reverse-NX's "fake" mode is obeyed, as the window does.
  return rex::ui::switch_saltynx::BaseMode(appletGetOperationMode() == AppletOperationMode_Console);
#else
  return false;
#endif
}

}  // namespace sa2::render
