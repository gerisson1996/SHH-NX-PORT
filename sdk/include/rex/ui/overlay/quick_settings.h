/**
 * @file        rex/ui/overlay/quick_settings.h
 *
 * @brief       Fork addition (shh-nx, 2026-10-29): a short menu of a few cvars, for players.
 *
 * The settings menu (settings_overlay.h) lists every cvar by category, which is for developers. This one shows only
 * the settings an app lists, each as its choices in a row (or a slider), with the controller or the mouse; a choice
 * applies at once and "Save" writes the configuration file. The dialog lives in the SDK because the SDK's ImGui is
 * the one its drawer draws with (an app may compile a copy of its own, as Renut's renderer does on PC).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <rex/ui/imgui_drawer.h>

namespace rex::ui {

struct QuickSetting {
  std::string cvar;   // the cvar it sets
  std::string label;  // its name in the menu
  // Its choices, as {value, label}. Empty: a slider from 'min' to 'max'.
  std::vector<std::pair<std::string, std::string>> choices;
  float min = 0.0f;
  float max = 1.0f;
  // Whether a choice can be picked now (for example only in docked mode); null: all of them.
  std::function<bool(const std::string& value)> available;
  std::string note;  // a line under the setting (empty: none)
};

// Opens the menu, or closes it when it is open. UI thread (a key bind's callback).
void ToggleQuickSettings(ImGuiDrawer* imgui_drawer, std::string title, std::vector<QuickSetting> settings,
                         std::filesystem::path config_path);

}  // namespace rex::ui
