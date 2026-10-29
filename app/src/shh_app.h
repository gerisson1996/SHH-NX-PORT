// shh - Silent Hill: Homecoming, ReXGlue recompiled project
//
// Customize the app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/input/read_hook.h>
#include <rex/rex_app.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/overlay/quick_settings.h>

#include <cstdlib>
#include <filesystem>
#include <system_error>

#if SHH_RENUT_RENDERER
#include "renut_engine/gpu/banjo_renut_select.h"
#include "renut_engine/gpu/overlay/app_integration.h"
#include "renut_engine/shh_compat/fast_boot.h"
#include "me/sa2_render_mode.h"
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
    // shh-nx (2026-10-29, docs/perfil-shh.md SHH-05): the handheld's OLED crushes the game's dark tones; 0.6 at scan-out
    // is what the user found right on the console (1.0, the game's own picture, stays the PC's).
    SetIfNotRequested("shh_output_gamma", "0.6");
    // shh-nx (2026-10-29, docs/perfil-shh.md SHH-07): the scene's 2x MSAA cost the GPU its resolves and twice the
    // depth and colour traffic; single-sample on the Switch (the PC keeps the game's 2x).
    SetIfNotRequested("shh_renut_single_sample", "true");
    // shh-nx (2026-10-29, docs/perfil-shh.md SHH-08): the shadow maps at 2/3 (a menu choice later).
    SetIfNotRequested("shh_shadow_quality", "Medium");
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
#if defined(__APPLE__)
    // iOS: the game files live in the app's Documents folder (Files app or Finder), in the same game_root layout as
    // the Switch's sdmc:/switch/shh-nx/game_root
    if (const char* home = std::getenv("HOME")) {
      const std::filesystem::path documents = std::filesystem::path(home) / "Documents";
      for (const auto& candidate : {documents / "game_root", documents}) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate / "default.xex", ec)) {
          paths.game_data_root = candidate;
          REXLOG_INFO("Game files found at {}", paths.game_data_root.string());
          return;
        }
      }
    }
#endif
  }

  // shh-nx (2026-10-28): Renut's renderer has to be started where SA2 starts it: once the paths are final (the window
  // and its drawer exist by then) and on every window resize. Without this the renderer's GPU device is never created,
  // no command list ever opens, and its resolve and Swap have nowhere to draw (docs/plano-renut.md).
#if SHH_RENUT_RENDERER
  // shh-nx (2026-10-29): the SDK's menus (and the game's, OnCreateDialogs) are drawn by Renut's present, as in sa2-nx;
  // without this drawer the SDK sets up no menus at all.
  std::unique_ptr<rex::ui::ImmediateDrawer> OnCreateImmediateDrawer() override {
    if (banjo::renut_gpu::Active())
      return renut::gpu::CreateOverlayDrawer();
    return rex::ReXApp::OnCreateImmediateDrawer();
  }

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

  // shh-nx (2026-10-29, docs/perfil-shh.md SHH-10): the game's menu (F5; L+R+Plus on the Switch): resolution, shadows,
  // brightness and MSAA, applied at once and saved to the configuration file.
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    rex::ReXApp::OnCreateDialogs(drawer);
    rex::ui::RegisterBind("bind_shh_menu", "F5", "Silent Hill menu", [this] { ToggleMenu(); });
    // LB+RB+Start (L+R+Plus on the Switch), read where every controller's input passes (input_system.cpp): from the
    // thread that reads input, so the menu opens on the UI thread.
    menu_app_ = this;
    rex::input::SetMenuComboCallback([] {
      if (ShhApp* app = menu_app_)
        app->app_context().CallInUIThread([app] { app->ToggleMenu(); });
    });
    // A measurement aid: the menu open from the start (PC screenshots, shot_run.ps1).
    if (rex::cvar::GetFlagByName("shh_diag_open_menu") == "true")
      ToggleMenu();
  }

 private:
  void ToggleMenu() {
    REXLOG_INFO("[menu] toggled");
    rex::ui::ToggleQuickSettings(imgui_drawer(), "Silent Hill: Homecoming", MenuSettings(), config_path());
  }
  static inline ShhApp* menu_app_ = nullptr;

  static std::vector<rex::ui::QuickSetting> MenuSettings() {
    std::vector<rex::ui::QuickSetting> settings;
#if SHH_RENUT_RENDERER
    rex::ui::QuickSetting resolution;
    resolution.cvar = "shh_resolution";
    resolution.label = "Resolucao";
    resolution.choices = {{"480p", "480p"}, {"600p", "600p"}, {"720p", "720p"}, {"1080p", "1080p"}};
#if REX_PLATFORM_SWITCH
    // 1080p only docked, where the GPU runs faster; in handheld mode the renderer draws it at 720p.
    resolution.available = [](const std::string& value) { return value != "1080p" || sa2::render::Docked(); };
    resolution.note = "1080p so no modo dock";
#endif
    settings.push_back(std::move(resolution));

    rex::ui::QuickSetting shadows;
    shadows.cvar = "shh_shadow_quality";
    shadows.label = "Sombras";
    shadows.choices = {{"Medium", "Media"}, {"High", "Alta"}};
    settings.push_back(std::move(shadows));

    rex::ui::QuickSetting gamma;
    gamma.cvar = "shh_output_gamma";
    gamma.label = "Gama da tela (menor = mais claro)";
    gamma.min = 0.3f;
    gamma.max = 1.2f;
    gamma.note = "0.6 no Switch, 1.0 e a imagem original";
    settings.push_back(std::move(gamma));

    rex::ui::QuickSetting msaa;
    msaa.cvar = "shh_renut_single_sample";
    msaa.label = "Anti-aliasing (MSAA 2x)";
    msaa.choices = {{"true", "Desligado"}, {"false", "Ligado"}};
    msaa.note = "vale ao reiniciar o jogo";
    settings.push_back(std::move(msaa));
#endif
    return settings;
  }

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
