/**
 * sa2-nx (2026-10-20): a function the input system calls at every controller state read (InputSystem::GetState),
 * before the read: Renut's late input delays the frame's first read there and its latency probe notes the read
 * (rex::latency::BeforeInputRead / NoteInputRead, app/src/renut_engine/sa2_compat/frame_latency.cpp; banjo-nx calls
 * them from its SDK directly). None by default.
 */
#pragma once

#include <cstdint>

namespace rex::input {

using ReadHook = void (*)();
void SetReadHook(ReadHook hook);

// shh-nx (2026-10-29): buttons the app holds down for user 0 on top of the controllers (XINPUT_GAMEPAD bits; 0 = none).
// Silent Hill's fast boot presses A through the logos with it.
void SetInjectedButtons(uint16_t buttons);

// shh-nx (2026-10-29): called (on the thread that reads input) when player 1 presses LB+RB+Start, L+R+Plus on the
// Switch: the app's menu. While LB and RB are held the game does not see Start. Null: no combination.
void SetMenuComboCallback(void (*callback)());

}  // namespace rex::input
