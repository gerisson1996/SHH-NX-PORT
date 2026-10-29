// shh-nx (2026-10-29, docs/plano-renut.md 5b): fast boot, A pressed through the boot's logos up to the title screen
// (the game's first tiled frame).
#pragma once

namespace shh::fast_boot {

// Before the game's module starts (shh_fast_boot).
void Start();
// The title screen is up (the first BeginTiling): no more presses.
void End();
// Every Swap: the presses, and the stop after shh_fast_boot_limit host seconds whatever happens.
void Tick();

}  // namespace shh::fast_boot
