#include "Globals.h"
#include "ComponentSimpleCharacterController.h"
#include "ComponentCharacterMotion.h"
#include "ComponentAnimation.h"
#include "ResourceStateMachine.h"
#include "GameObject.h"
#include <Keyboard.h>
#include <GamePad.h>
#include <algorithm>
#include <cmath>

ComponentSimpleCharacterController::ComponentSimpleCharacterController(GameObject* owner) : Component(owner) {}

void ComponentSimpleCharacterController::ensureInit() {
    if (m_initialized) return;
    m_motion = owner->getComponent<ComponentCharacterMotion>();
    m_anim   = owner->getComponent<ComponentAnimation>();
    m_initialized = true;
}

void ComponentSimpleCharacterController::update(float dt) {
    ensureInit();
    if (!m_motion) return;

    auto kb = DirectX::Keyboard::Get().GetState();
    float moveInput   = 0.f;
    float rotateInput = 0.f;

    if (kb.Up    || kb.W) moveInput   += 1.f;
    if (kb.Down  || kb.S) moveInput   -= 1.f;
    if (kb.Right || kb.D) rotateInput += 1.f;
    if (kb.Left  || kb.A) rotateInput -= 1.f;

    auto pad = DirectX::GamePad::Get().GetState(0);
    if (pad.IsConnected()) {
        if (fabsf(pad.thumbSticks.leftY) > 0.2f) moveInput   = pad.thumbSticks.leftY;
        if (fabsf(pad.thumbSticks.leftX) > 0.2f) rotateInput = pad.thumbSticks.leftX;
    }

    moveInput   = std::max(-1.f, std::min(1.f, moveInput));
    rotateInput = std::max(-1.f, std::min(1.f, rotateInput));

    m_motion->Move(moveInput);
    m_motion->Rotate(rotateInput);

    if (m_anim) {
        bool isMoving = fabsf(moveInput) > 0.01f || fabsf(rotateInput) > 0.01f;
        if (isMoving && !m_wasMoving)
            m_anim->SendTrigger(HashString(std::string("move")));
        else if (!isMoving && m_wasMoving)
            m_anim->SendTrigger(HashString(std::string("stop")));
        m_wasMoving = isMoving;
    }
}

void ComponentSimpleCharacterController::onEditor() {
    ImGui::TextDisabled("Arrow keys / WASD + left gamepad stick.");
    ImGui::TextDisabled("Requires ComponentCharacterMotion on same object.");
    ensureInit();
    if (m_motion)
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.f), "Motion: found");
    else
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.f), "Motion: NOT FOUND");
    if (m_anim)
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.f), "Animation: found");
    else
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.4f, 1.f), "Animation: not found (optional)");
}

void ComponentSimpleCharacterController::onSave(std::string& outJson) const {
    outJson = "{}";
}

void ComponentSimpleCharacterController::onLoad(const std::string&) {}
