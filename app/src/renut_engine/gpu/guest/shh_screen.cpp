/**
 * @file    gpu/guest/shh_screen.cpp
 * @brief   shh-nx (2026-10-28): the game's screen width for Renut's renderer.
 *
 * Renut's guest/widescreen.cpp widens N&B's screen through N&B's own code; Silent Hill: Homecoming has none of that
 * code, so its screen is the console's 1280x720 (the renderer's options read the width, graphics_options.cpp). Same
 * shape as sa2-nx's sa2_screen.cpp.
 */
#include "renut_engine/gpu/guest/widescreen.h"

namespace renut::gpu {

uint32_t GuestScreenWidth() {
  return 1280;
}

} // namespace renut::gpu
