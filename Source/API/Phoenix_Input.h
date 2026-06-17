#pragma once
#include "API/Phoenix_Keys.h"
#include "API/Phoenix_Types.h"

namespace Phoenix {

struct Input {
    // ----- Keyboard -----
    static bool IsKeyDown    (Key k);   // held this frame
    static bool IsKeyPressed (Key k);   // went down this frame only
    static bool IsKeyReleased(Key k);   // went up this frame only

    // ----- Mouse -----
    static bool IsMouseDown    (MouseButton btn);
    static bool IsMousePressed (MouseButton btn);
    static bool IsMouseReleased(MouseButton btn);

    static Vec2 GetMousePosition();     // screen-space pixel coords
    static Vec2 GetMouseDelta();        // pixels moved since last frame

    // ----- Axes (WASD / Arrow keys, -1..1) -----
    // "Horizontal"  A/Left = -1,  D/Right = +1
    // "Vertical"    S/Down = -1,  W/Up    = +1
    static float GetAxis(const char* name);

    // ----- Gamepad (player index 0-3) -----
    static bool  IsGamepadConnected (int player = 0);
    static bool  IsButtonDown       (GamepadButton btn, int player = 0);
    static bool  IsButtonPressed    (GamepadButton btn, int player = 0);
    static bool  IsButtonReleased   (GamepadButton btn, int player = 0);
    static float GetGamepadAxis     (GamepadAxis axis, int player = 0);  // -1..1 sticks, 0..1 triggers

    // Vibration — leftMotor and rightMotor are 0..1
    static void  SetVibration       (float leftMotor, float rightMotor, int player = 0);
};

} // namespace Phoenix
