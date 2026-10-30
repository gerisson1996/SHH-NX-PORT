/**
 * @file        ui/overlay/quick_settings.cpp
 *
 * @brief       Fork addition (shh-nx, 2026-10-29). See quick_settings.h.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <rex/ui/overlay/quick_settings.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>

#include <imgui.h>

#include <chrono>
#include <cstdlib>

namespace rex::ui {

namespace {

class QuickSettingsDialog;
QuickSettingsDialog* g_open = nullptr;

class QuickSettingsDialog final : public ImGuiDialog {
 public:
  QuickSettingsDialog(ImGuiDrawer* imgui_drawer, std::string title, std::vector<QuickSetting> settings,
                      std::filesystem::path config_path)
      : ImGuiDialog(imgui_drawer),
        title_(std::move(title)),
        settings_(std::move(settings)),
        config_path_(std::move(config_path)) {
    g_open = this;
  }
  ~QuickSettingsDialog() override {
    if (g_open == this)
      g_open = nullptr;
  }

 protected:
  void OnDraw(ImGuiIO& io) override {
    const ImVec2 size(560.0f, 0.0f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    ImGui::SetNextWindowBgAlpha(0.9f);
    // The controller lands in the menu as it opens.
    if (!focused_) {
      ImGui::SetNextWindowFocus();
      focused_ = true;
    }
    bool open = true;
    if (ImGui::Begin((title_ + "##quick").c_str(), &open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
      for (const QuickSetting& s : settings_) {
        ImGui::PushID(s.cvar.c_str());
        ImGui::TextUnformatted(s.label.c_str());
        const std::string value = rex::cvar::GetFlagByName(s.cvar);
        if (s.choices.empty()) {
          float v = float(std::atof(value.c_str()));
          ImGui::SetNextItemWidth(-1.0f);
          if (ImGui::SliderFloat("##v", &v, s.min, s.max, "%.2f"))
            Set(s.cvar, std::to_string(v));
        } else {
          bool first = true;
          for (const auto& [choice, label] : s.choices) {
            if (!first)
              ImGui::SameLine();
            first = false;
            const bool can = !s.available || s.available(choice);
            ImGui::BeginDisabled(!can);
            if (ImGui::RadioButton(label.c_str(), value == choice) && value != choice)
              Set(s.cvar, choice);
            ImGui::EndDisabled();
          }
        }
        if (!s.note.empty())
          ImGui::TextDisabled("%s", s.note.c_str());
        ImGui::Spacing();
        ImGui::PopID();
      }
      ImGui::Separator();
      if (ImGui::Button("Salvar")) {
        rex::cvar::SaveConfig(config_path_);
        saved_ = true;
      }
      ImGui::SameLine();
      if (ImGui::Button("Fechar"))
        open = false;
      if (saved_) {
        ImGui::SameLine();
        ImGui::TextDisabled("salvo em %s", config_path_.filename().string().c_str());
      }
    }
    ImGui::End();
    if (!open)
      Close();
  }

 private:
  void Set(const std::string& name, const std::string& value) {
    if (rex::cvar::SetFlagByName(name, value))
      REXLOG_INFO("menu: {} = {}", name, value);
    saved_ = false;
  }

  std::string title_;
  std::vector<QuickSetting> settings_;
  std::filesystem::path config_path_;
  bool focused_ = false;
  bool saved_ = false;
};

}  // namespace

void ToggleQuickSettings(ImGuiDrawer* imgui_drawer, std::string title, std::vector<QuickSetting> settings,
                         std::filesystem::path config_path) {
  static auto last_toggle = std::chrono::steady_clock::time_point{};
  const auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_toggle).count() < 300) {
    return;
  }
  last_toggle = now;

  if (g_open) {
    delete g_open;  // its destructor takes it out of the drawer and clears g_open
    return;
  }
  if (!imgui_drawer)
    return;
  new QuickSettingsDialog(imgui_drawer, std::move(title), std::move(settings), std::move(config_path));
}

}  // namespace rex::ui
