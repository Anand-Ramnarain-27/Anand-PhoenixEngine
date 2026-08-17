#pragma once
#include "API/Phoenix_Keys.h"
#include "API/Phoenix_Types.h"

namespace Phoenix {

struct Input {
    // ----- Keyboard -----
    static bool IsKeyDown    (Key k);  
    static bool IsKeyPressed (Key k);   
    static bool IsKeyReleased(Key k);  

    // ----- Mouse -----
    static bool IsMouseDown    (MouseButton btn);
    static bool IsMousePressed (MouseButton btn);
    static bool IsMouseReleased(MouseButton btn);

    static Vec2 GetMousePosition();     
    static Vec2 GetMouseDelta();     

    static float GetAxis(const char* name);

    // ----- Gamepad (player index 0-3) -----
    static bool  IsGamepadConnected (int player = 0);
    static bool  IsButtonDown       (GamepadButton btn, int player = 0);
    static bool  IsButtonPressed    (GamepadButton btn, int player = 0);
    static bool  IsButtonReleased   (GamepadButton btn, int player = 0);
    static float GetGamepadAxis     (GamepadAxis axis, int player = 0); 

    static void  SetVibration       (float leftMotor, float rightMotor, int player = 0);
};

} // namespace Phoenix
