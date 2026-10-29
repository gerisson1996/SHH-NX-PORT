// shh-nx (2026-10-29, docs/plano-renut.md 5b): fast boot. The Konami and Double Helix logos take some 20 s with the
// legal screen on every platform, and a press of A skips each logo (the legal screen keeps its time). So A is pressed
// for the player, every half second, up to the title screen (its 3D scene is the game's first tiled frame,
// D3DDevice_BeginTiling). A faster guest clock (rex::chrono's time scalar) was tried first: the logos came later.
#include "renut_engine/shh_compat/fast_boot.h"

#include <atomic>
#include <chrono>
#include <cmath>

#include <rex/cvar.h>
#include <rex/input/read_hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(shh_fast_boot, true, "SHH", "Skip the boot's logos, up to the title screen (presses A for you)");
REXCVAR_DEFINE_DOUBLE(shh_fast_boot_limit, 40.0, "SHH",
                      "Host seconds after which fast boot stops pressing even if the title screen never came");

namespace shh::fast_boot {
namespace {

constexpr uint16_t kButtonA = 0x1000;  // XINPUT_GAMEPAD_A
constexpr double kPeriod = 0.5;        // seconds between presses
constexpr double kHold = 0.15;         // seconds each press is held

std::atomic<bool> g_active{false};
std::chrono::steady_clock::time_point g_started;

double Elapsed() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_started).count();
}

}  // namespace

void Start() {
  if (!REXCVAR_GET(shh_fast_boot))
    return;
  g_started = std::chrono::steady_clock::now();
  g_active.store(true, std::memory_order_release);
  REXLOG_INFO("[fast boot] pressing A through the logos until the title screen (shh_fast_boot)");
}

void End() {
  if (!g_active.exchange(false, std::memory_order_acq_rel))
    return;
  rex::input::SetInjectedButtons(0);
  REXLOG_INFO("[fast boot] title screen after {:.1f} s", Elapsed());
}

void Tick() {
  if (!g_active.load(std::memory_order_acquire))
    return;
  const double t = Elapsed();
  if (t > REXCVAR_GET(shh_fast_boot_limit)) {
    REXLOG_WARN("[fast boot] no title screen after shh_fast_boot_limit: stopped pressing");
    End();
    return;
  }
  rex::input::SetInjectedButtons(std::fmod(t, kPeriod) < kHold ? kButtonA : 0);
}

}  // namespace shh::fast_boot
