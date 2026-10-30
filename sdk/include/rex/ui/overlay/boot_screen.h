/**
 * @file        rex/ui/overlay/boot_screen.h
 *
 * @brief       Fork addition (shh-nx, 2026-10-29): a loading screen for the wait before the game's first frame.
 *
 * Between the launch and the game's first present the screen stays black for some seconds (on the Switch, while the
 * game loads its first files and shaders). This draws the title and "Carregando" with a moving bar over black, as an
 * ImGui window that takes no focus and no input (the controller stays the game's), until 'done' says the game has
 * presented; then it closes itself. In the SDK for the same reason as quick_settings.h: the SDK's ImGui is the one
 * its drawer draws with.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once

#include <functional>
#include <string>

#include <rex/ui/imgui_drawer.h>

namespace rex::ui {

// Opens the loading screen. 'done' is asked every frame (UI thread); true closes it. UI thread.
void ShowBootScreen(ImGuiDrawer* imgui_drawer, std::string title, std::function<bool()> done);

}  // namespace rex::ui
