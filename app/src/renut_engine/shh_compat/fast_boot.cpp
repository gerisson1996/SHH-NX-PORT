// shh-nx (2026-10-29, docs/plano-renut.md 5b): fast boot. The Konami and Double Helix logos take some 20 s with the
// legal screen on every platform, and a press of A skips each logo (the legal screen keeps its time). So A is pressed
// for the player, every half second, up to the title screen (its 3D scene is the game's first tiled frame,
// D3DDevice_BeginTiling). A faster guest clock (rex::chrono's time scalar) was tried first: the logos came later.
#include "renut_engine/shh_compat/fast_boot.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

#include <rex/cvar.h>
#include <rex/input/read_hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(shh_fast_boot, true, "SHH", "Skip the boot's logos, up to the title screen (presses A for you)");
// shh-nx (2026-10-29): read by name in shh_app.h (RexSwitchApmOfficialClocks), defined here with the other SHH switches.
REXCVAR_DEFINE_BOOL(shh_official_clocks, true, "SHH",
                    "Switch: Nintendo's official clock profiles, docked GPU 768 MHz and handheld GPU 460.8 MHz "
                    "(false = leave the clocks to the system)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_DOUBLE(shh_fast_boot_limit, 40.0, "SHH",
                      "Host seconds after which fast boot stops pressing even if the title screen never came");
// shh-nx (2026-10-29, docs/perfil-shh.md): a measurement aid. Keeps pressing A past the title screen, through the menus
// and the opening, for this many host seconds, so the PC reaches gameplay with no one at the controller.
REXCVAR_DEFINE_DOUBLE(shh_diag_press_a_s, 0.0, "SHH/Diagnostics",
                      "Keep pressing A for this many host seconds after launch, past the title screen (0: off)");

REXCVAR_DEFINE_BOOL(shh_diag_open_menu, false, "SHH/Diagnostics",
                    "Open the game's menu (F5) at start, for screenshots");

namespace shh::fast_boot {
namespace {

constexpr uint16_t kButtonA = 0x1000;  // XINPUT_GAMEPAD_A
constexpr double kPeriod = 0.25;       // seconds between presses (was 0.5; faster logo skipping)
constexpr double kHold = 0.10;         // seconds each press is held (was 0.15)

std::atomic<bool> g_active{false};
std::chrono::steady_clock::time_point g_started;

double Elapsed() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_started).count();
}

}  // namespace

void Start() {
  if (!REXCVAR_GET(shh_fast_boot) && REXCVAR_GET(shh_diag_press_a_s) <= 0.0)
    return;
  g_started = std::chrono::steady_clock::now();
  g_active.store(true, std::memory_order_release);
  REXLOG_INFO("[fast boot] pressing A through the logos until the title screen (shh_fast_boot)");
}

void End() {
  if (REXCVAR_GET(shh_diag_press_a_s) > 0.0 && Elapsed() < REXCVAR_GET(shh_diag_press_a_s))
    return;
  if (!g_active.exchange(false, std::memory_order_acq_rel))
    return;
  rex::input::SetInjectedButtons(0);
  REXLOG_INFO("[fast boot] title screen after {:.1f} s", Elapsed());
}

void Tick() {
  if (!g_active.load(std::memory_order_acquire))
    return;
  const double t = Elapsed();
  if (t > std::max(REXCVAR_GET(shh_fast_boot_limit), REXCVAR_GET(shh_diag_press_a_s))) {
    REXLOG_WARN("[fast boot] no title screen after shh_fast_boot_limit: stopped pressing");
    End();
    return;
  }
  rex::input::SetInjectedButtons(std::fmod(t, kPeriod) < kHold ? kButtonA : 0);
}

}  // namespace shh::fast_boot
