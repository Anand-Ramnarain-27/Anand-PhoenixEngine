#pragma once
#include "Module.h"
#include "API/Phoenix_Keys.h"
#include "API/Phoenix_Types.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "GamePad.h"

class ModuleInput : public Module {
public:
    ModuleInput(HWND hWnd);

    void update() override;

    // Drops pending pressed/released edges and the mouse delta, e.g. at the end of a scene transition: anything
    // held down through it reads as held, never as a fresh press.
    void resetState();

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
    // Mouse wheel movement since the last update, in notches (120 units each); + = away from the user.
    float getMouseWheelDelta() const { return float(mouseCurr.scrollWheelValue - mousePrev.scrollWheelValue) / 120.f; }

    // ----- Gamepad -----
    bool  isGamepadConnected (int player) const;
    bool  isButtonDown       (Phoenix::GamepadButton btn, int player) const;
    bool  isButtonPressed    (Phoenix::GamepadButton btn, int player) const;
    bool  isButtonReleased   (Phoenix::GamepadButton btn, int player) const;
    float getGamepadAxis     (Phoenix::GamepadAxis axis, int player) const;
    void  setVibration       (float leftMotor, float rightMotor, int player);

private:
    static constexpr int kMaxPlayers = 4;

    std::unique_ptr<DirectX::Keyboard> keyboard;
    std::unique_ptr<DirectX::Mouse>    mouse;
    std::unique_ptr<DirectX::GamePad>  gamePad;

    DirectX::Keyboard::KeyboardStateTracker kbTracker;
    DirectX::Mouse::ButtonStateTracker      mouseTracker;
    DirectX::Mouse::State                   mousePrev{};
    DirectX::Mouse::State                   mouseCurr{};

    DirectX::GamePad::ButtonStateTracker padTracker[kMaxPlayers];
    DirectX::GamePad::State              padState  [kMaxPlayers]{};
};
