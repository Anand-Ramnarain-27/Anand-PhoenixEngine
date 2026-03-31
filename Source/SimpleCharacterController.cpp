#include "Globals.h"
#include "SimpleCharacterController.h"
#include "ComponentAnimation.h"
#include "ComponentMotion.h"
#include "GameObject.h"
#include <imgui.h>

SimpleCharacterController::SimpleCharacterController(GameObject* owner)
    : Component(owner) {
}

void SimpleCharacterController::update(float deltaTime) {
    if (!owner) return;
    auto* motion = owner->getComponent<ComponentMotion>();
    auto* anim = owner->getComponent<ComponentAnimation>();

    float linearDir = 0.0f;
    float angularDir = 0.0f;

    if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow))
        linearDir = 1.0f;
    if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow))
        linearDir = -1.0f;
    if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))
        angularDir = -1.0f;
    if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow))
        angularDir = 1.0f;

    bool isMoving = (linearDir != 0.0f || angularDir != 0.0f);

    if (motion) {
        motion->move(linearDir);
        motion->rotate(angularDir);
    }

    if (anim) {
        if (isMoving && !m_wasMoving)
            anim->sendTrigger(triggerMove);
        else if (!isMoving && m_wasMoving)
            anim->sendTrigger(triggerStop);
    }
    m_wasMoving = isMoving;
}

void SimpleCharacterController::onEditor() {
    ImGui::Text("Simple Character Controller");
    ImGui::Separator();
    ImGui::Text("WASD / Arrow keys to move");
    static char moveBuf[64], stopBuf[64];
    strncpy_s(moveBuf, triggerMove.c_str(), sizeof(moveBuf) - 1);
    strncpy_s(stopBuf, triggerStop.c_str(), sizeof(stopBuf) - 1);
    if (ImGui::InputText("Move trigger", moveBuf, sizeof(moveBuf)))
        triggerMove = moveBuf;
    if (ImGui::InputText("Stop trigger", stopBuf, sizeof(stopBuf)))
        triggerStop = stopBuf;
    ImGui::Text("Moving: %s", m_wasMoving ? "yes" : "no");
}
