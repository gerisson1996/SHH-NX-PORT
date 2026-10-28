// shh-nx (2026-10-28): the render resolution per console mode, as Renut's renderer asks SA2's renderer for it
// (sa2-nx's src/me/sa2_render_mode.h). SHH has no resolution feature yet: the game's own 1280x720 is what it draws,
// so the PC is never "docked" and the height is always 720. The namespace keeps SA2's name so the vendored renderer's
// call sites stay untouched; the Switch build replaces this with the console's mode when stage 4 gets there.
#pragma once

#include <cstdint>

namespace sa2::render {

// The current mode: the PC is the game's own 720p, so never docked.
inline bool Docked() { return false; }

// The render height set for a mode: the game's 720.
inline int32_t RenderHeightOf(bool /*docked*/) { return 720; }

// Ring thread, after each presented swap: nothing to do (SA2's PC test toggles live here).
inline void OnPresented(uint64_t /*presented*/) {}

}  // namespace sa2::render
