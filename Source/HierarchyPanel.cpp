#include "Globals.h"
#include "HierarchyPanel.h"
#include "ModuleEditor.h"
#include "ModuleScene.h"
#include "Application.h"
#include "GameObject.h"
#include "ComponentCamera.h"
#include "ComponentMesh.h"
#include "ComponentLights.h"
#include "ComponentFactory.h"
#include "EditorSelection.h"
#include "PrefabManager.h"

void HierarchyPanel::drawContent() {
    ModuleScene* scene = m_editor->getActiveModuleScene();
    if (!scene) return;
    EditorSelection& sel = m_editor->getSelection();
    if (ImGui::Button("+ Empty")) m_editor->createEmptyGameObject();
    ImGui::SameLine();
    if (ImGui::Button("+ Child") && sel.has()) m_editor->createEmptyGameObject("Empty", sel.object);
    ImGui::Separator();
    drawNode(scene->getRoot());
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered()) ImGui::OpenPopup("##HierBlank");
    blankContextMenu();
}

void HierarchyPanel::drawNode(GameObject* go) {
    if (!go) return;
    EditorSelection& sel = m_editor->getSelection();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (go == sel.object) flags |= ImGuiTreeNodeFlags_Selected;
    if (go->getChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    if (sel.renaming == go) {
        ImGui::SetKeyboardFocusHere();
        bool done = ImGui::InputText("##rename", sel.renameBuffer, sizeof(sel.renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (done || ImGui::IsItemDeactivated()) { go->setName(sel.renameBuffer); sel.renaming = nullptr; }
        for (auto* c : go->getChildren()) drawNode(c);
        return;
    }

    bool nodeOpen = ImGui::TreeNodeEx((void*)(uintptr_t)go->getUID(), flags, go->getName().c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) sel.object = go;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) sel.object = go;
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) { sel.renaming = go; strncpy_s(sel.renameBuffer, go->getName().c_str(), sizeof(sel.renameBuffer) - 1); }
    if (ImGui::BeginPopupContextItem()) { itemContextMenu(go); ImGui::EndPopup(); }

    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("GO_PTR", &go, sizeof(GameObject*));
        ImGui::Text("Move: %s", go->getName().c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("GO_PTR"))
            if (auto* dragged = *(GameObject**)p->Data; dragged && dragged != go) dragged->setParent(go);
        ImGui::EndDragDropTarget();
    }

    if (nodeOpen) {
        for (auto* c : go->getChildren()) drawNode(c);
        ImGui::TreePop();
    }
}

void HierarchyPanel::itemContextMenu(GameObject* go) {
    EditorSelection& sel = m_editor->getSelection();
    textMuted("%s", go->getName().c_str());
    ImGui::Separator();
    if (ImGui::MenuItem("Rename")) { sel.renaming = go; strncpy_s(sel.renameBuffer, go->getName().c_str(), sizeof(sel.renameBuffer) - 1); }
    if (ImGui::MenuItem("Duplicate")) m_editor->createEmptyGameObject((go->getName() + " (Copy)").c_str(), go->getParent());
    ImGui::Separator();

    if (ImGui::BeginMenu("Add Component")) {
        auto addIf = [&](const char* label, Component::Type type, bool has) {
            if (ImGui::MenuItem(label) && !has) go->addComponent(ComponentFactory::CreateComponent(type, go));
            };
        addIf("Camera", Component::Type::Camera, go->getComponent<ComponentCamera>() != nullptr);
        addIf("Mesh", Component::Type::Mesh, go->getComponent<ComponentMesh>() != nullptr);
        addIf("Directional Light", Component::Type::DirectionalLight, go->getComponent<ComponentDirectionalLight>() != nullptr);
        addIf("Point Light", Component::Type::PointLight, go->getComponent<ComponentPointLight>() != nullptr);
        addIf("Spot Light", Component::Type::SpotLight, go->getComponent<ComponentSpotLight>() != nullptr);
        ImGui::EndMenu();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Create Empty Child")) m_editor->createEmptyGameObject("Empty", go);
    ImGui::Separator();

    if (ImGui::BeginMenu("Prefab")) {
        static char pfBuf[128] = "";
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputTextWithHint("##pfn", go->getName().c_str(), pfBuf, sizeof(pfBuf));
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            std::string name = strlen(pfBuf) > 0 ? pfBuf : go->getName();
            if (PrefabManager::createPrefab(go, name)) {
                PrefabInstanceData d; d.prefabName = name;
                PrefabManager::linkInstance(go, d);
                m_editor->log(("Prefab saved: " + name).c_str(), EditorColors::Success);
            }
            memset(pfBuf, 0, sizeof(pfBuf));
        }
        if (PrefabManager::isPrefabInstance(go)) {
            ImGui::Separator();
            if (ImGui::MenuItem("Apply to Prefab")) { PrefabManager::applyToPrefab(go); m_editor->log("Applied to prefab.", EditorColors::Success); }
            if (ImGui::MenuItem("Revert to Prefab")) { PrefabManager::revertToPrefab(go, m_editor->getActiveModuleScene()); m_editor->log("Reverted from prefab.", EditorColors::Warning); }
            ImGui::Separator();
            if (ImGui::MenuItem("Unpack (Unlink)")) { PrefabManager::unlinkInstance(go); m_editor->log("Prefab unlinked.", EditorColors::Warning); }
        }
        ImGui::EndMenu();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Danger);
    if (ImGui::MenuItem("Delete")) m_editor->deleteGameObject(go);
    ImGui::PopStyleColor();
}

void HierarchyPanel::blankContextMenu() {
    if (!ImGui::BeginPopup("##HierBlank")) return;
    EditorSelection& sel = m_editor->getSelection();
    if (ImGui::MenuItem("Create Empty")) m_editor->createEmptyGameObject();
    if (ImGui::MenuItem("Create Empty Child") && sel.has()) m_editor->createEmptyGameObject("Empty", sel.object);
    ImGui::EndPopup();
}