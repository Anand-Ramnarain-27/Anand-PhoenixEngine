#pragma once
// Keyboard, mouse and gamepad state, sampled once per frame (DirectXTK input), with pressed/released edges.

#include "Module.h"
#include "API/Phoenix_Keys.h"
#include "API/Phoenix_Types.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "GamePad.h"

/// Polled input for the engine and for Phoenix::Input. "Pressed" / "released" are edges since the previous
/// update(); "down" is the current state. Gamepad queries take a player index 0..3 (anything else reads player 0).
class ModuleInput : public Module {
public:
    ModuleInput(HWND hWnd);

    void update() override;

    /// Drops pending pressed/released edges and the mouse delta, e.g. at the end of a scene transition: anything
    /// held down through it reads as held, never as a fresh press.
    void resetState();

    bool isKeyDown    (Phoenix::Key k) const;
    bool isKeyPressed (Phoenix::Key k) const;
    bool isKeyReleased(Phoenix::Key k) const;

    bool isMouseDown    (Phoenix::MouseButton btn) const;
    bool isMousePressed (Phoenix::MouseButton btn) const;
    bool isMouseReleased(Phoenix::MouseButton btn) const;

    /// Client-area pixels.
    Phoenix::Vec2 getMousePosition() const;
    /// Pixels moved since the last update.
    Phoenix::Vec2 getMouseDelta()    const;
    /// Mouse wheel movement since the last update, in notches; + = away from the user.
    float getMouseWheelDelta() const { return float(m_mouseCurr.scrollWheelValue - m_mousePrev.scrollWheelValue) / float(WHEEL_DELTA); }

    bool  isGamepadConnected (int player) const;
    bool  isButtonDown       (Phoenix::GamepadButton btn, int player) const;
    bool  isButtonPressed    (Phoenix::GamepadButton btn, int player) const;
    bool  isButtonReleased   (Phoenix::GamepadButton btn, int player) const;
    /// Sticks -1..1 (circular dead zone applied), triggers 0..1.
    float getGamepadAxis     (Phoenix::GamepadAxis axis, int player) const;
    /// Motor speeds 0..1.
    void  setVibration       (float leftMotor, float rightMotor, int player);

private:
    static constexpr int kMaxPlayers = 4;

    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    std::unique_ptr<DirectX::Mouse>    m_mouse;
    std::unique_ptr<DirectX::GamePad>  m_gamePad;

    DirectX::Keyboard::KeyboardStateTracker m_kbTracker;
    DirectX::Mouse::ButtonStateTracker      m_mouseTracker;
    DirectX::Mouse::State                   m_mousePrev{};
    DirectX::Mouse::State                   m_mouseCurr{};

    DirectX::GamePad::ButtonStateTracker m_padTracker[kMaxPlayers];
    DirectX::GamePad::State              m_padState  [kMaxPlayers]{};
};
