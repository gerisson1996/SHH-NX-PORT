/**
 * @file        ui/overlay/boot_screen.cpp
 *
 * @brief       Fork addition (shh-nx, 2026-10-29). See boot_screen.h.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <rex/ui/overlay/boot_screen.h>

#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace rex::ui {

namespace {

class BootScreenDialog final : public ImGuiDialog {
 public:
  BootScreenDialog(ImGuiDrawer* imgui_drawer, std::string title, std::function<bool()> done)
      : ImGuiDialog(imgui_drawer),
        title_(std::move(title)),
        done_(std::move(done)),
        start_(std::chrono::steady_clock::now()) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    if (done_ && done_()) {
      REXLOG_INFO("[boot screen] the game's first frame after {:.1f} s", t);
      Close();
      return;
    }
    const ImVec2 display = io.DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(display);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    // No focus, navigation or input: the controller stays the game's (rex_app.cpp's GameReceivesGamepad).
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##boot_screen", nullptr, flags)) {
      // Sized for the display: ImGui's font is 13 px, about 3x at 720 lines.
      const float scale = std::max(1.0f, display.y / 240.0f);
      ImDrawList* draw = ImGui::GetWindowDrawList();
      ImFont* font = ImGui::GetFont();

      const float title_size = ImGui::GetFontSize() * scale;
      const ImVec2 title_extent = font->CalcTextSizeA(title_size, FLT_MAX, 0.0f, title_.c_str());
      const float cy = display.y * 0.5f;
      draw->AddText(font, title_size, ImVec2((display.x - title_extent.x) * 0.5f, cy - title_extent.y * 1.4f),
                    IM_COL32(205, 200, 190, 255), title_.c_str());

      // "Carregando" with one to three dots, one more every half second.
      static const char* const kLoading[] = {"Carregando.", "Carregando..", "Carregando..."};
      const char* loading = kLoading[int(t * 2.0) % 3];
      const float text_size = title_size * 0.5f;
      const ImVec2 base = font->CalcTextSizeA(text_size, FLT_MAX, 0.0f, "Carregando...");
      draw->AddText(font, text_size, ImVec2((display.x - base.x) * 0.5f, cy + title_extent.y * 0.2f),
                    IM_COL32(150, 145, 140, 255), loading);

      // A thin bar with a segment that goes back and forth: no progress is known, only that it is working.
      const float bar_w = std::min(display.x * 0.4f, title_extent.x);
      const float bar_h = std::max(2.0f, display.y / 360.0f);
      const ImVec2 bar0((display.x - bar_w) * 0.5f, cy + title_extent.y * 0.2f + base.y * 1.8f);
      draw->AddRectFilled(bar0, ImVec2(bar0.x + bar_w, bar0.y + bar_h), IM_COL32(40, 38, 36, 255));
      const float seg = bar_w * 0.25f;
      const float phase = float(0.5 - 0.5 * std::cos(t * 2.0));  // 0..1..0, about every 3 s
      const float x = bar0.x + (bar_w - seg) * phase;
      draw->AddRectFilled(ImVec2(x, bar0.y), ImVec2(x + seg, bar0.y + bar_h), IM_COL32(170, 40, 30, 255));
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
  }

 private:
  std::string title_;
  std::function<bool()> done_;
  std::chrono::steady_clock::time_point start_;
};

}  // namespace

void ShowBootScreen(ImGuiDrawer* imgui_drawer, std::string title, std::function<bool()> done) {
  if (!imgui_drawer)
    return;
  new BootScreenDialog(imgui_drawer, std::move(title), std::move(done));  // owned by the drawer until Close
}

}  // namespace rex::ui
