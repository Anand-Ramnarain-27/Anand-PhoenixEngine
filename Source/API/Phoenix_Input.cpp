#include "Globals.h"
#include "API/Phoenix_Input.h"
#include "ModuleInput.h"
#include "Application.h"

namespace Phoenix {

bool Input::IsKeyDown(Key k){
    return app->getInput()->isKeyDown(k);
}

bool Input::IsKeyPressed(Key k){
    return app->getInput()->isKeyPressed(k);
}

bool Input::IsKeyReleased(Key k){
    return app->getInput()->isKeyReleased(k);
}

bool Input::IsMouseDown(MouseButton btn){
    return app->getInput()->isMouseDown(btn);
}

bool Input::IsMousePressed(MouseButton btn){
    return app->getInput()->isMousePressed(btn);
}

bool Input::IsMouseReleased(MouseButton btn){
    return app->getInput()->isMouseReleased(btn);
}

Vec2 Input::GetMousePosition(){
    return app->getInput()->getMousePosition();
}

Vec2 Input::GetMouseDelta(){
    return app->getInput()->getMouseDelta();
}

float Input::GetAxis(const char* name){
    if (!name) return 0.f;
    if (strcmp(name, "Horizontal") == 0){
        float v = 0.f;
        if (IsKeyDown(Key::A) || IsKeyDown(Key::Left))  v -= 1.f;
        if (IsKeyDown(Key::D) || IsKeyDown(Key::Right)) v += 1.f;
        // Fall back to left stick if no key held
        if (v == 0.f) v = GetGamepadAxis(GamepadAxis::LeftStickX);
        return v;
    }
    if (strcmp(name, "Vertical") == 0){
        float v = 0.f;
        if (IsKeyDown(Key::S) || IsKeyDown(Key::Down)) v -= 1.f;
        if (IsKeyDown(Key::W) || IsKeyDown(Key::Up))   v += 1.f;
        if (v == 0.f) v = GetGamepadAxis(GamepadAxis::LeftStickY);
        return v;
    }
    return 0.f;
}

bool Input::IsGamepadConnected(int player){
    return app->getInput()->isGamepadConnected(player);
}

bool Input::IsButtonDown(GamepadButton btn, int player){
    return app->getInput()->isButtonDown(btn, player);
}

bool Input::IsButtonPressed(GamepadButton btn, int player){
    return app->getInput()->isButtonPressed(btn, player);
}

bool Input::IsButtonReleased(GamepadButton btn, int player){
    return app->getInput()->isButtonReleased(btn, player);
}

float Input::GetGamepadAxis(GamepadAxis axis, int player){
    return app->getInput()->getGamepadAxis(axis, player);
}

void Input::SetVibration(float leftMotor, float rightMotor, int player){
    app->getInput()->setVibration(leftMotor, rightMotor, player);
}

} // namespace Phoenix
