#pragma once
#include "Module.h"
#include "API/Phoenix_Keys.h"
#include "API/Phoenix_Types.h"

namespace DirectX { class Keyboard; class Mouse; class GamePad; }

class ModuleInput : public Module {
public:
    ModuleInput(HWND hWnd);

    bool update() override;

    // ----- Keyboard -----
    bool isKeyDown    (Phoenix::Key k) const;
    bool isKeyPressed (Phoenix::Key k) const;
    bool isKeyReleased(Phoenix::Key k) const;

    // ----- Mouse -----
    bool isMouseDown    (Phoenix::MouseButton btn) const;
    bool isMousePressed (Phoenix::MouseButton btn) const;
    bool isMouseReleased(Phoenix::MouseButton btn) const;

    Phoenix::Vec2 getMousePosition() const;
    Phoenix::Vec2 getMouseDelta()    const;

    // ----- Gamepad -----
    bool  isGamepadConnected (int player) const;
    bool  isButtonDown       (Phoenix::GamepadButton btn, int player) const;
    bool  isButtonPressed    (Phoenix::GamepadButton btn, int player) const;
    bool  isButtonReleased   (Phoenix::GamepadButton btn, int player) const;
    float getGamepadAxis     (Phoenix::GamepadAxis axis, int player) const;
    void  setVibration       (float leftMotor, float rightMotor, int player);

private:
    std::unique_ptr<Keyboard> keyboard;
    std::unique_ptr<Mouse>    mouse;
    std::unique_ptr<GamePad>  gamePad;

    // Per-frame state trackers (forward-declared via void* to avoid leaking DirectXTK types)
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
