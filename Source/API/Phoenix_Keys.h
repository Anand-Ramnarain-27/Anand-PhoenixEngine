#pragma once

namespace Phoenix {

// Maps to DirectXTK Keyboard::Keys / Windows VK_* values.
enum class Key : int {
    None      = 0,
    Back      = 0x08,
    Tab       = 0x09,
    Enter     = 0x0D,
    Escape    = 0x1B,
    Space     = 0x20,
    Left      = 0x25,
    Up        = 0x26,
    Right     = 0x27,
    Down      = 0x28,
    D0 = 0x30, D1, D2, D3, D4, D5, D6, D7, D8, D9,
    A  = 0x41, B, C, D, E, F, G, H, I, J, K, L, M,
    N,         O, P, Q, R, S, T, U, V, W, X, Y, Z,
    F1  = 0x70, F2,  F3,  F4,  F5,  F6,
    F7,         F8,  F9,  F10, F11, F12,
    LeftShift   = 0xA0,
    RightShift  = 0xA1,
    LeftControl = 0xA2,
    RightControl= 0xA3,
    LeftAlt     = 0xA4,
    RightAlt    = 0xA5,
};

enum class MouseButton {
    Left   = 0,
    Right  = 1,
    Middle = 2,
};

enum class GamepadButton {
    A,
    B,
    X,
    Y,
    LeftShoulder,
    RightShoulder,
    LeftStick,
    RightStick,
    DPadUp,
    DPadDown,
    DPadLeft,
    DPadRight,
    Start,
    Back,
};

enum class GamepadAxis {
    LeftStickX,
    LeftStickY,
    RightStickX,
    RightStickY,
    LeftTrigger,
    RightTrigger,
};

} // namespace Phoenix
