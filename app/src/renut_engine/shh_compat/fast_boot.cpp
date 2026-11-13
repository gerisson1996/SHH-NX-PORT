// shh-nx (2026-10-29, docs/plano-renut.md 5b): fast boot. The Konami and Double Helix logos take some 20 s with the
// legal screen on every platform, and a press of A skips each logo (the legal screen keeps its time). So A is pressed
// for the player, every half second, up to the title screen (its 3D scene is the game's first tiled frame,
// D3DDevice_BeginTiling). A faster guest clock (rex::chrono's time scalar) was tried first: the logos came later.
#include "renut_engine/shh_compat/fast_boot.h"
#include "renut_engine/gpu/guest/guest_memory.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/input/read_hook.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/ppc/func.h>

#if REX_PLATFORM_SWITCH
#include <switch.h>
#endif

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

// shh-nx (2026-11-11): the Double Helix logo and the legal screen (some 10 s) are the boot's loading screen
// (TUILoadingScreenScript, between its TitleScreenStart and TitleScreenFinished), which A does not skip. Its thread
// (sub_823B4DB0) steps it 1/30 s and then sleeps 33 ms; with the boot's data loaded, the main thread waits in
// sub_825AC688 for as long as sub_825E6460 says it is still up. Fast boot drops that 33 ms sleep while the main thread
// waits: the logos play out in a fraction of the time and end as they always do (cut short instead, the rest of the
// legal screen came back over the first level's loading screen).
REXCVAR_DEFINE_BOOL(shh_fast_boot_skip_legal, true, "SHH",
                    "Fast boot also runs the Double Helix logo and the legal screen at full speed");

// shh-nx (2026-11-12, console log shh_003): with the logos down to 1 s, the Switch boot is the game's first load (from
// the first frame at 3.4 s to the logos at 9 s, CPU work: the SD reads take some 1.5 s of it). The console's own
// loading boost (appletSetCpuBoostMode FastLoad: CPU 1785 MHz instead of 1020, the GPU at its minimum meanwhile, as
// commercial games do on their loading screens) runs from launch to the title screen.
REXCVAR_DEFINE_BOOL(shh_fast_boot_cpu_boost, true, "SHH",
                    "Switch: CPU at 1785 MHz (the system's loading boost) from launch to the title screen");

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

std::atomic<bool> g_cpu_boost{false};

void CpuBoost(bool on) {
#if REX_PLATFORM_SWITCH
  if (on ? !REXCVAR_GET(shh_fast_boot_cpu_boost) : !g_cpu_boost.load(std::memory_order_acquire))
    return;
  const Result rc = appletSetCpuBoostMode(on ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal);
  g_cpu_boost.store(on && R_SUCCEEDED(rc), std::memory_order_release);
  REXLOG_INFO("[fast boot] CPU loading boost {} (appletSetCpuBoostMode 0x{:X})", on ? "on" : "off", u32(rc));
#else
  (void)on;
#endif
}

}  // namespace

void Start() {
  if (!REXCVAR_GET(shh_fast_boot) && REXCVAR_GET(shh_diag_press_a_s) <= 0.0)
    return;
  g_started = std::chrono::steady_clock::now();
  g_active.store(true, std::memory_order_release);
  REXLOG_INFO("[fast boot] pressing A through the logos until the title screen (shh_fast_boot)");
  CpuBoost(true);
}

void End() {
  CpuBoost(false);  // at the title screen, before the diagnostics' own presses go on
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

namespace {
// The main thread is waiting for the boot's loading screen (sub_825E6460 said it is up), during fast boot.
std::atomic<bool> g_loading_wait{false};
}  // namespace

// sub_825E6460: is the LoadingScreenScript still showing? Its one caller is the main thread's wait after a load
// (sub_825AC688, Sleep(50) a turn).
REX_EXTERN(__imp__sub_825E6460);
extern "C" REX_FUNC(sub_825E6460) {
  __imp__sub_825E6460(ctx, base);
  const bool up = ctx.r3.u8 != 0 && shh::fast_boot::g_active.load(std::memory_order_acquire) &&
                  REXCVAR_GET(shh_fast_boot_skip_legal);
  if (g_loading_wait.exchange(up, std::memory_order_acq_rel) != up)
    REXLOG_INFO("[fast boot] {} after {:.1f} s", up ? "logos and legal screen at full speed" : "logos done",
                shh::fast_boot::Elapsed());
}

namespace {
// The loading screen thread's object (sub_823B4DB0's parameter), caught where the thread is made.
std::atomic<uint32_t> g_loading_screen{0};
constexpr uint32_t kLoadingScreenThread = 0x823B4DB0;
constexpr uint32_t kUpdateSlot = 588;  // vtable slot: Update(this, float dt), dt a fixed 1/30 s
}  // namespace

// shh-nx (2026-11-11, console log shh_001): with the 33 ms sleep gone the Switch still took 5.2 s over the logos (the
// PC 3.1 s): each turn also draws and presents, so the turns come at the display's pace. These extra steps a turn,
// with the same 1/30 s, run the timeline that many times faster again; the drawn frames simply show fewer of its states.
REXCVAR_DEFINE_INT32(shh_fast_boot_legal_steps, 4, "SHH",
                     "Fast boot: extra 1/30 s steps of the logos and legal screen for each frame they draw (0: none)");

// sub_8269F970: the engine's CreateThread(..., entry r4, parameter r5, ...).
REX_EXTERN(__imp__sub_8269F970);
extern "C" REX_FUNC(sub_8269F970) {
  if (ctx.r4.u32 == kLoadingScreenThread)
    g_loading_screen.store(ctx.r5.u32, std::memory_order_release);
  __imp__sub_8269F970(ctx, base);
}

REX_EXTERN(__imp__sub_82506710);  // the per-thread "in an update" count, up (the loop's own, around Update and Draw)
REX_EXTERN(__imp__sub_82506760);  // and down

// sub_8269F780: the engine's Sleep(float ms, alertable). The loading screen thread's 33 ms between its steps goes while
// the main thread waits for it in the boot (the step is a fixed 1/30 s, so its timers run that much faster). The thread
// sleeps inside its critical section (sub_8269F428(33)), so the extra steps below run under it as its own do.
REX_EXTERN(__imp__sub_8269F780);
extern "C" REX_FUNC(sub_8269F780) {
  if (ctx.f1.f64 == 33.0 && g_loading_wait.load(std::memory_order_relaxed)) {
    const uint32_t object = g_loading_screen.load(std::memory_order_acquire);
    const int32_t steps = std::clamp(REXCVAR_GET(shh_fast_boot_legal_steps), 0, 16);
    const uint32_t *vtable = object ? renut::gpu::GuestPtr<uint32_t>(object) : nullptr;
    const uint32_t update =
        vtable ? __builtin_bswap32(*renut::gpu::GuestPtr<uint32_t>(__builtin_bswap32(*vtable) + kUpdateSlot)) : 0;
    const float dt = 1.0f / 30.0f;
    if (update) {
      PPCFunc *update_func = rex::runtime::ResolveIndirectFunction(update);
      for (int32_t i = 0; i < steps && update_func && g_loading_wait.load(std::memory_order_relaxed); ++i) {
        __imp__sub_82506710(ctx, base);
        ctx.r3.u64 = object;
        ctx.f1.f64 = dt;
        update_func(ctx, base);
        __imp__sub_82506760(ctx, base);
      }
    }
    ctx.r4.u64 = 1;
    ctx.f1.f64 = 0.0;
  }
  __imp__sub_8269F780(ctx, base);
}
