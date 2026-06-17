#include "Globals.h"
#include "Application.h"
#include "ModuleInput.h"

using namespace DirectX;
using namespace Phoenix;

ModuleInput::ModuleInput(HWND hWnd){
    keyboard = std::make_unique<Keyboard>();
    mouse    = std::make_unique<Mouse>();
    gamePad  = std::make_unique<GamePad>();
    mouse->SetWindow(hWnd);
}

void ModuleInput::update(){
    mousePrev = mouseCurr;
    mouseCurr = mouse->GetState();
    kbTracker.Update(keyboard->GetState());
    mouseTracker.Update(mouseCurr);
    for (int i = 0; i < kMaxPlayers; ++i){
        padState[i] = gamePad->GetState(i, GamePad::DEAD_ZONE_CIRCULAR);
        padTracker[i].Update(padState[i]);
    }
}

// ----- Keyboard -----

bool ModuleInput::isKeyDown(Key k) const {
    return keyboard->GetState().IsKeyDown(static_cast<Keyboard::Keys>(k));
}

bool ModuleInput::isKeyPressed(Key k) const {
    return kbTracker.IsKeyPressed(static_cast<Keyboard::Keys>(k));
}

bool ModuleInput::isKeyReleased(Key k) const {
    return kbTracker.IsKeyReleased(static_cast<Keyboard::Keys>(k));
}

// ----- Mouse -----

bool ModuleInput::isMouseDown(MouseButton btn) const {
    switch (btn){
    case MouseButton::Left:   return mouseCurr.leftButton;
    case MouseButton::Right:  return mouseCurr.rightButton;
    case MouseButton::Middle: return mouseCurr.middleButton;
    }
    return false;
}

bool ModuleInput::isMousePressed(MouseButton btn) const {
    using BS = Mouse::ButtonStateTracker;
    switch (btn){
    case MouseButton::Left:   return mouseTracker.leftButton   == BS::PRESSED;
    case MouseButton::Right:  return mouseTracker.rightButton  == BS::PRESSED;
    case MouseButton::Middle: return mouseTracker.middleButton == BS::PRESSED;
    }
    return false;
}

bool ModuleInput::isMouseReleased(MouseButton btn) const {
    using BS = Mouse::ButtonStateTracker;
    switch (btn){
    case MouseButton::Left:   return mouseTracker.leftButton   == BS::RELEASED;
    case MouseButton::Right:  return mouseTracker.rightButton  == BS::RELEASED;
    case MouseButton::Middle: return mouseTracker.middleButton == BS::RELEASED;
    }
    return false;
}

Vec2 ModuleInput::getMousePosition() const {
    return Vec2{ static_cast<float>(mouseCurr.x), static_cast<float>(mouseCurr.y) };
}

Vec2 ModuleInput::getMouseDelta() const {
    return Vec2{ static_cast<float>(mouseCurr.x - mousePrev.x),
                 static_cast<float>(mouseCurr.y - mousePrev.y) };
}

// ----- Gamepad -----

static int clampPlayer(int p, int max){ return (p < 0 || p >= max) ? 0 : p; }

bool ModuleInput::isGamepadConnected(int player) const {
    return padState[clampPlayer(player, kMaxPlayers)].IsConnected();
}

bool ModuleInput::isButtonDown(GamepadButton btn, int player) const {
    const auto& s = padState[clampPlayer(player, kMaxPlayers)];
    switch (btn){
    case GamepadButton::A:             return s.buttons.a;
    case GamepadButton::B:             return s.buttons.b;
    case GamepadButton::X:             return s.buttons.x;
    case GamepadButton::Y:             return s.buttons.y;
    case GamepadButton::LeftShoulder:  return s.buttons.leftShoulder;
    case GamepadButton::RightShoulder: return s.buttons.rightShoulder;
    case GamepadButton::LeftStick:     return s.buttons.leftStick;
    case GamepadButton::RightStick:    return s.buttons.rightStick;
    case GamepadButton::DPadUp:        return s.dpad.up;
    case GamepadButton::DPadDown:      return s.dpad.down;
    case GamepadButton::DPadLeft:      return s.dpad.left;
    case GamepadButton::DPadRight:     return s.dpad.right;
    case GamepadButton::Start:         return s.buttons.start;
    case GamepadButton::Back:          return s.buttons.back;
    }
    return false;
}

bool ModuleInput::isButtonPressed(GamepadButton btn, int player) const {
    const auto& t = padTracker[clampPlayer(player, kMaxPlayers)];
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
    const auto& t = padTracker[clampPlayer(player, kMaxPlayers)];
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
    const auto& s = padState[clampPlayer(player, kMaxPlayers)];
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
    gamePad->SetVibration(clampPlayer(player, kMaxPlayers), leftMotor, rightMotor);
}
