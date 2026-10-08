#pragma once
// Keyboard, mouse and gamepad state for scripts.

#include "API/Phoenix_Keys.h"
#include "API/Phoenix_Types.h"

namespace Phoenix {

/// Polled input for scripts, updated once per frame.
struct Input {
    // Keyboard. Pressed / Released are true only on the frame the state changed.
    static bool IsKeyDown    (Key k);
    static bool IsKeyPressed (Key k);
    static bool IsKeyReleased(Key k);

    // Mouse. Positions are in client-area pixels.
    static bool IsMouseDown    (MouseButton btn);
    static bool IsMousePressed (MouseButton btn);
    static bool IsMouseReleased(MouseButton btn);

    static Vec2 GetMousePosition();
    static Vec2 GetMouseDelta();

    /// "Horizontal" (A / D or Left / Right) or "Vertical" (S / W or Down / Up) as -1..1; the left stick when no
    /// key is held. 0 for any other name.
    static float GetAxis(const char* name);

    // Gamepad (DirectXTK GamePad, player 0-3).
    static bool  IsGamepadConnected (int player = 0);
    static bool  IsButtonDown       (GamepadButton btn, int player = 0);
    static bool  IsButtonPressed    (GamepadButton btn, int player = 0);
    static bool  IsButtonReleased   (GamepadButton btn, int player = 0);
    static float GetGamepadAxis     (GamepadAxis axis, int player = 0);

    static void  SetVibration       (float leftMotor, float rightMotor, int player = 0);
};

} // namespace Phoenix
