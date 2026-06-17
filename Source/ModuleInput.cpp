#include "Globals.h"
#include "Application.h"
#include "ModuleInput.h"

#include "Keyboard.h"
#include "Mouse.h"
#include "GamePad.h"

using namespace DirectX;
using namespace Phoenix;

static constexpr int kMaxPlayers = 4;

struct ModuleInput::Impl {
    Keyboard::KeyboardStateTracker  kbTracker;
    Mouse::ButtonStateTracker       mouseTracker;
    Mouse::State                    mousePrev{};
    Mouse::State                    mouseCurr{};
    GamePad::ButtonStateTracker     padTracker[kMaxPlayers];
    GamePad::State                  padState  [kMaxPlayers]{};
};

ModuleInput::ModuleInput(HWND hWnd){
    keyboard = std::make_unique<Keyboard>();
    mouse    = std::make_unique<Mouse>();
    gamePad  = std::make_unique<GamePad>();
    mouse->SetWindow(hWnd);
    m_impl   = std::make_unique<Impl>();
}

bool ModuleInput::update(){
    m_impl->mousePrev = m_impl->mouseCurr;
    m_impl->mouseCurr = mouse->GetState();
    m_impl->kbTracker.Update(keyboard->GetState());
    m_impl->mouseTracker.Update(m_impl->mouseCurr);
    for (int i = 0; i < kMaxPlayers; ++i){
        m_impl->padState[i] = gamePad->GetState(i, GamePad::DEAD_ZONE_CIRCULAR);
        m_impl->padTracker[i].Update(m_impl->padState[i]);
    }
    return true;
}

// ----- Keyboard -----

bool ModuleInput::isKeyDown(Key k) const {
    return keyboard->GetState().IsKeyDown(static_cast<Keyboard::Keys>(k));
}

bool ModuleInput::isKeyPressed(Key k) const {
    return m_impl->kbTracker.IsKeyPressed(static_cast<Keyboard::Keys>(k));
}

bool ModuleInput::isKeyReleased(Key k) const {
    return m_impl->kbTracker.IsKeyReleased(static_cast<Keyboard::Keys>(k));
}

// ----- Mouse -----

bool ModuleInput::isMouseDown(MouseButton btn) const {
    switch (btn){
    case MouseButton::Left:   return m_impl->mouseCurr.leftButton;
    case MouseButton::Right:  return m_impl->mouseCurr.rightButton;
    case MouseButton::Middle: return m_impl->mouseCurr.middleButton;
    }
    return false;
}

bool ModuleInput::isMousePressed(MouseButton btn) const {
    switch (btn){
    case MouseButton::Left:   return m_impl->mouseTracker.leftButton   == Mouse::ButtonStateTracker::PRESSED;
    case MouseButton::Right:  return m_impl->mouseTracker.rightButton  == Mouse::ButtonStateTracker::PRESSED;
    case MouseButton::Middle: return m_impl->mouseTracker.middleButton == Mouse::ButtonStateTracker::PRESSED;
    }
    return false;
}

bool ModuleInput::isMouseReleased(MouseButton btn) const {
    switch (btn){
    case MouseButton::Left:   return m_impl->mouseTracker.leftButton   == Mouse::ButtonStateTracker::RELEASED;
    case MouseButton::Right:  return m_impl->mouseTracker.rightButton  == Mouse::ButtonStateTracker::RELEASED;
    case MouseButton::Middle: return m_impl->mouseTracker.middleButton == Mouse::ButtonStateTracker::RELEASED;
    }
    return false;
}

Vec2 ModuleInput::getMousePosition() const {
    return Vec2{ static_cast<float>(m_impl->mouseCurr.x),
                 static_cast<float>(m_impl->mouseCurr.y) };
}

Vec2 ModuleInput::getMouseDelta() const {
    return Vec2{ static_cast<float>(m_impl->mouseCurr.x - m_impl->mousePrev.x),
                 static_cast<float>(m_impl->mouseCurr.y - m_impl->mousePrev.y) };
}

// ----- Gamepad -----

static int clampPlayer(int p){ return (p < 0 || p >= kMaxPlayers) ? 0 : p; }

bool ModuleInput::isGamepadConnected(int player) const {
    return m_impl->padState[clampPlayer(player)].IsConnected();
}

bool ModuleInput::isButtonDown(GamepadButton btn, int player) const {
    const auto& s = m_impl->padState[clampPlayer(player)].buttons;
    switch (btn){
    case GamepadButton::A:             return s.a;
    case GamepadButton::B:             return s.b;
    case GamepadButton::X:             return s.x;
    case GamepadButton::Y:             return s.y;
    case GamepadButton::LeftShoulder:  return s.leftShoulder;
    case GamepadButton::RightShoulder: return s.rightShoulder;
    case GamepadButton::LeftStick:     return s.leftStick;
    case GamepadButton::RightStick:    return s.rightStick;
    case GamepadButton::DPadUp:        return m_impl->padState[clampPlayer(player)].dpad.up;
    case GamepadButton::DPadDown:      return m_impl->padState[clampPlayer(player)].dpad.down;
    case GamepadButton::DPadLeft:      return m_impl->padState[clampPlayer(player)].dpad.left;
    case GamepadButton::DPadRight:     return m_impl->padState[clampPlayer(player)].dpad.right;
    case GamepadButton::Start:         return s.start;
    case GamepadButton::Back:          return s.back;
    }
    return false;
}

bool ModuleInput::isButtonPressed(GamepadButton btn, int player) const {
    const auto& t = m_impl->padTracker[clampPlayer(player)];
    using BS = GamePad::ButtonStateTracker;
    switch (btn){
    case GamepadButton::A:             return t.a             == BS::PRESSED;
    case GamepadButton::B:             return t.b             == BS::PRESSED;
    case GamepadButton::X:             return t.x             == BS::PRESSED;
    case GamepadButton::Y:             return t.y             == BS::PRESSED;
    case GamepadButton::LeftShoulder:  return t.leftShoulder  == BS::PRESSED;
    case GamepadButton::RightShoulder: return t.rightShoulder == BS::PRESSED;
    case GamepadButton::LeftStick:     return t.leftStick     == BS::PRESSED;
    case GamepadButton::RightStick:    return t.rightStick    == BS::PRESSED;
    case GamepadButton::DPadUp:        return t.dpadUp        == BS::PRESSED;
    case GamepadButton::DPadDown:      return t.dpadDown      == BS::PRESSED;
    case GamepadButton::DPadLeft:      return t.dpadLeft      == BS::PRESSED;
    case GamepadButton::DPadRight:     return t.dpadRight     == BS::PRESSED;
    case GamepadButton::Start:         return t.start         == BS::PRESSED;
    case GamepadButton::Back:          return t.back          == BS::PRESSED;
    }
    return false;
}

bool ModuleInput::isButtonReleased(GamepadButton btn, int player) const {
    const auto& t = m_impl->padTracker[clampPlayer(player)];
    using BS = GamePad::ButtonStateTracker;
    switch (btn){
    case GamepadButton::A:             return t.a             == BS::RELEASED;
    case GamepadButton::B:             return t.b             == BS::RELEASED;
    case GamepadButton::X:             return t.x             == BS::RELEASED;
    case GamepadButton::Y:             return t.y             == BS::RELEASED;
    case GamepadButton::LeftShoulder:  return t.leftShoulder  == BS::RELEASED;
    case GamepadButton::RightShoulder: return t.rightShoulder == BS::RELEASED;
    case GamepadButton::LeftStick:     return t.leftStick     == BS::RELEASED;
    case GamepadButton::RightStick:    return t.rightStick    == BS::RELEASED;
    case GamepadButton::DPadUp:        return t.dpadUp        == BS::RELEASED;
    case GamepadButton::DPadDown:      return t.dpadDown      == BS::RELEASED;
    case GamepadButton::DPadLeft:      return t.dpadLeft      == BS::RELEASED;
    case GamepadButton::DPadRight:     return t.dpadRight     == BS::RELEASED;
    case GamepadButton::Start:         return t.start         == BS::RELEASED;
    case GamepadButton::Back:          return t.back          == BS::RELEASED;
    }
    return false;
}

float ModuleInput::getGamepadAxis(GamepadAxis axis, int player) const {
    const auto& s = m_impl->padState[clampPlayer(player)];
    switch (axis){
    case GamepadAxis::LeftStickX:   return s.thumbSticks.leftX;
    case GamepadAxis::LeftStickY:   return s.thumbSticks.leftY;
    case GamepadAxis::RightStickX:  return s.thumbSticks.rightX;
    case GamepadAxis::RightStickY:  return s.thumbSticks.rightY;
    case GamepadAxis::LeftTrigger:  return s.triggers.left;
    case GamepadAxis::RightTrigger: return s.triggers.right;
    }
    return 0.f;
}

void ModuleInput::setVibration(float leftMotor, float rightMotor, int player){
    gamePad->SetVibration(clampPlayer(player), leftMotor, rightMotor);
}
