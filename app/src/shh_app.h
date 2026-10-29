// shh - Silent Hill: Homecoming, ReXGlue recompiled project
//
// Customize the app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/rex_app.h>

#include <filesystem>
#include <system_error>

#if SHH_RENUT_RENDERER
#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/overlay/app_integration.h"
#include "renut_engine/shh_compat/fast_boot.h"
#endif

class ShhApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<ShhApp>(new ShhApp(ctx, "shh", PPCImageConfig));
  }

  // Settings the game needs, so that a plain "shh.exe" starts like the long command line.
  // Only the ones nobody set are touched: the command line still wins.
  void OnPostInitLogging() override {
    // Without a GPU plugin the game runs but the runtime discards its graphics calls
    // ("no GPU emulation loaded") and the window stays black. It has to be set here:
    // SetupPresentation reads it right after.
    //
    // shh-nx (2026-10-28): not with Renut's renderer: its own hooks take the game's Direct3D and the emulated Xenos
    // GPU must not allocate its share of the guest's physical window (the renderer's blocks come out of the guest's
    // own heap then, and both would be drawing).
    bool renut = false;
#if SHH_RENUT_RENDERER
    renut = banjo::renut_gpu::Active();
    if (renut) {
      // shh-nx (2026-10-29): SHH recompiled code does not mark store cards. Synchronous exact_streams
      // forces CRC-64 hashing on the render thread (1.5 GB / 10s), starving Core 1.
      // Offloading verification to the background thread keeps the render thread under 16 ms for 30 fps.
      SetIfNotRequested("banjo_renut_exact_streams", "false");
    }
#endif
    if (!renut) {
      SetIfNotRequested("gpu_plugin", "xenos");
    }
#if !REX_PLATFORM_SWITCH
    // Keyboard and mouse in addition to the gamepad.
    SetIfNotRequested("mnk_mode", "true");
#else
    // shh-nx (2026-10-29): no asset pack. Silent Hill's data is 4848 MB, over FAT32's 4 GB file limit, so the SDK's
    // first-boot pack build reads it all for 1 min 45 s on every boot and then gives up ("asset pack not built").
    SetIfNotRequested("asset_pack_build", "false");
#endif
  }

  // Without --game_data_root (a double click on shh.exe), look for the game files ourselves:
  // a "game_root" folder next to the exe (the layout of a package), then the project's
  // assets/game_root (the exe built in out/pc). The first one with default.xex wins.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (!paths.game_data_root.empty()) {
      return;
    }
    const auto exe_dir = rex::filesystem::GetExecutableFolder();
    for (const auto& candidate : {exe_dir / "game_root", exe_dir / ".." / ".." / "assets" / "game_root"}) {
      std::error_code ec;
      if (std::filesystem::is_regular_file(candidate / "default.xex", ec)) {
        paths.game_data_root = std::filesystem::weakly_canonical(candidate, ec);
        if (ec) {
          paths.game_data_root = candidate;
        }
        REXLOG_INFO("Game files found at {}", paths.game_data_root.string());
        return;
      }
    }
  }

  // shh-nx (2026-10-28): Renut's renderer has to be started where SA2 starts it: once the paths are final (the window
  // and its drawer exist by then) and on every window resize. Without this the renderer's GPU device is never created,
  // no command list ever opens, and its resolve and Swap have nowhere to draw (docs/plano-renut.md).
#if SHH_RENUT_RENDERER
  void OnWindowPixelSizeChanged(uint32_t pixel_width, uint32_t pixel_height) override {
    rex::ReXApp::OnWindowPixelSizeChanged(pixel_width, pixel_height);
    if (banjo::renut_gpu::Active())
      renut::gpu::OnWindowResized();
  }

  // shh-nx (2026-10-29, docs/plano-renut.md 5b): fast boot, A pressed through the logos up to the title screen (its
  // first BeginTiling, a Renut hook: hence only with Renut's renderer).
  void OnPreLaunchModule() override {
    rex::ReXApp::OnPreLaunchModule();
    if (banjo::renut_gpu::Active())
      shh::fast_boot::Start();
  }

  std::optional<rex::PathConfig> OnFinalizePaths(const rex::PathConfig& defaults,
                                                 std::function<void(rex::PathConfig)> resume) override {
    if (banjo::renut_gpu::Active())
      renut::gpu::StartRenderer(app_context(), [this] { return imgui_drawer(); });
    return rex::ReXApp::OnFinalizePaths(defaults, std::move(resume));
  }
#endif

 private:
  static void SetIfNotRequested(const char* name, const char* value) {
    if (rex::cvar::GetFlagInfo(name) == nullptr) {
      REXLOG_DEBUG("Setting '{}' not registered yet; left alone.", name);
      return;
    }
    if (rex::cvar::HasNonDefaultValue(name)) {
      return;  // set explicitly: do not override it.
    }
    if (rex::cvar::SetFlagByName(name, value)) {
      REXLOG_DEBUG("Default setting: {} = {}", name, value);
    }
  }
};
