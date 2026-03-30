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
#include "SceneManager.h"
#include "ComponentScript.h"
#include "HotReloadManager.h"
#include "ComponentAnimation.h"

static GameObject* findPrefabRoot(GameObject* go) {
    GameObject* cur = go;
    while (cur) {
        if (PrefabManager::isPrefabInstance(cur)) return cur;
        cur = cur->getParent();
    }
    return nullptr;
}

void HierarchyPanel::drawContent() {
    ModuleScene* scene = m_editor->getActiveModuleScene();
    if (!scene) return;
    EditorSelection& sel = m_editor->getSelection();

    bool prefabMode = m_editor->getSceneManager() && m_editor->getSceneManager()->isEditingPrefab();

    if (prefabMode) {
        // Coloured header strip
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 p2 = { p.x + ImGui::GetContentRegionAvail().x, p.y + 26.f };
        ImGui::GetWindowDrawList()->AddRectFilled(p, p2, IM_COL32(30, 90, 30, 200));
        ImGui::GetWindowDrawList()->AddRect(p, p2, IM_COL32(50, 200, 50, 180));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 1.f, 0.35f, 1.f));
        ImGui::Text("  Prefab: %s", m_editor->getSceneManager()->getPrefabEditName().c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
    }
    else {
        if (ImGui::Button("+ Empty")) m_editor->createEmptyGameObject();
        ImGui::SameLine();
        if (ImGui::Button("+ Child") && sel.has()) m_editor->createEmptyGameObject("Empty", sel.object);
        ImGui::Separator();
    }

    drawNode(scene->getRoot(), prefabMode);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered()) ImGui::OpenPopup("##HierBlank");
    blankContextMenu();
}

void HierarchyPanel::drawNode(GameObject* go, bool prefabMode, bool isRoot) {
    if (!go) return;
    EditorSelection& sel = m_editor->getSelection();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (go == sel.object) flags |= ImGuiTreeNodeFlags_Selected;
    if (go->getChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (prefabMode && isRoot) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    if (sel.renaming == go) {
        ImGui::SetKeyboardFocusHere();
        bool done = ImGui::InputText("##rename", sel.renameBuffer, sizeof(sel.renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (done || ImGui::IsItemDeactivated()) { go->setName(sel.renameBuffer); sel.renaming = nullptr; }
        for (auto* c : go->getChildren()) drawNode(c, prefabMode, false);
        return;
    }

    bool isPrefabRoot = prefabMode && isRoot;
    bool isPrefabInst = !isPrefabRoot && PrefabManager::isPrefabInstance(go);
    if (isPrefabRoot) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.20f, 1.f));
    else if (isPrefabInst) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.0f, 1.f));

    const char* label = (isPrefabRoot && m_editor->getSceneManager())
        ? m_editor->getSceneManager()->getPrefabEditName().c_str()
        : go->getName().c_str();

    bool nodeOpen = ImGui::TreeNodeEx((void*)(uintptr_t)go->getUID(), flags, "%s", label);

    if (isPrefabRoot || isPrefabInst) ImGui::PopStyleColor();

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
        for (auto* c : go->getChildren()) drawNode(c, prefabMode, false);
        ImGui::TreePop();
    }
}

void HierarchyPanel::itemContextMenu(GameObject* go) {
    EditorSelection& sel = m_editor->getSelection();
    bool prefabMode = m_editor->getSceneManager() && m_editor->getSceneManager()->isEditingPrefab();
    bool isEditRoot = prefabMode && m_editor->getPrefabSession() && go == m_editor->getPrefabSession()->rootObject;

    if (prefabMode) {
        const std::string& pfName = m_editor->getSceneManager()->getPrefabEditName();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.75f, 0.2f, 1.f));
        ImGui::Text("Prefab: %s", pfName.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();

        PrefabEditSession* pfSess = m_editor->getPrefabSession();
     
        const PrefabInstanceData* instData = pfSess ? PrefabManager::getInstanceData(pfSess->rootObject) : nullptr;
        bool hasChanges = instData && !instData->overrides.isEmpty();

        ImGui::BeginDisabled(!hasChanges);
        ImGui::PushStyleColor(ImGuiCol_Text, hasChanges ? EditorColors::Success : EditorColors::Muted);
        if (ImGui::MenuItem("Apply  - Save changes to prefab file")) {
            PrefabManager::applyToPrefab(pfSess->rootObject);
            m_editor->log(("Applied prefab: " + pfName).c_str(), EditorColors::Success);
            m_editor->exitPrefabEdit();
        }
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !hasChanges)
            ImGui::SetTooltip("No changes to apply");

        ImGui::BeginDisabled(!hasChanges);
        if (ImGui::MenuItem("Revert  - Reload from prefab file")) {
            PrefabManager::revertToPrefab(pfSess->rootObject, pfSess->isolatedScene.get());
            m_editor->getSelection().object = pfSess->rootObject;
            m_editor->log(("Reverted prefab: " + pfName).c_str(), EditorColors::Warning);
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !hasChanges)
            ImGui::SetTooltip("No changes to revert");

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Warning);
        if (ImGui::MenuItem("Unlink  - Break prefab connection")) {
            PrefabManager::unlinkInstance(m_editor->getPrefabSession()->rootObject);
            m_editor->log(("Unlinked prefab: " + pfName).c_str(), EditorColors::Warning);
            m_editor->exitPrefabEdit();
        }
        ImGui::PopStyleColor();

        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Danger);
        if (ImGui::MenuItem("Exit Prefab Edit")) m_editor->exitPrefabEdit();
        ImGui::PopStyleColor();
        ImGui::Separator();
    }
    else {
        textMuted("%s", go->getName().c_str());
        ImGui::Separator();
    }

    if (ImGui::MenuItem("Rename")) { sel.renaming = go; strncpy_s(sel.renameBuffer, go->getName().c_str(), sizeof(sel.renameBuffer) - 1); }
    if (!prefabMode) {
        if (ImGui::MenuItem("Duplicate")) m_editor->createEmptyGameObject((go->getName() + " (Copy)").c_str(), go->getParent());
    }
    ImGui::Separator();

    if (ImGui::BeginMenu("Add Component")) {
        auto addIf = [&](const char* label, Component::Type type, bool has) {
            if (ImGui::MenuItem(label) && !has) {
                go->addComponent(ComponentFactory::CreateComponent(type, go));
                GameObject* root = findPrefabRoot(go);
                if (root)
                    if (PrefabInstanceData* inst = PrefabManager::getInstanceDataMutable(root))
                        inst->overrides.addedComponentTypes.push_back((int)type);
            }
            };
        addIf("Camera", Component::Type::Camera, go->getComponent<ComponentCamera>() != nullptr);
        addIf("Mesh", Component::Type::Mesh, go->getComponent<ComponentMesh>() != nullptr);
        addIf("Directional Light", Component::Type::DirectionalLight, go->getComponent<ComponentDirectionalLight>() != nullptr);
        addIf("Point Light", Component::Type::PointLight, go->getComponent<ComponentPointLight>() != nullptr);
        addIf("Spot Light", Component::Type::SpotLight, go->getComponent<ComponentSpotLight>() != nullptr);
        addIf("Animation", Component::Type::Animation, go->getComponent<ComponentAnimation>() != nullptr);
        if (ImGui::BeginMenu("Add Script")) {
            auto* hr = m_editor->getHotReloadManager();

            if (!hr) {
                ImGui::TextDisabled("HotReload not ready");
            }
            else {
                auto names = hr->getRegisteredClassNames();

                if (names.empty()) {
                    ImGui::TextDisabled("No script DLL loaded");
                }
                else {
                    for (const auto& name : names) {
                        if (ImGui::MenuItem(name.c_str())) {

                            auto comp = ComponentFactory::CreateComponent(
                                Component::Type::Script, go);

                            auto* sc = static_cast<ComponentScript*>(comp.get());
                            sc->setScriptClass(name, hr);

                            go->addComponent(std::move(comp));
                        }
                    }
                }
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Create Empty Child")) m_editor->createEmptyGameObject("Empty", go);

    if (!prefabMode) {
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
    }

    ImGui::Separator();
    bool isRoot = prefabMode && isEditRoot;
    ImGui::BeginDisabled(isRoot);
    ImGui::PushStyleColor(ImGuiCol_Text, isRoot ? EditorColors::Muted : EditorColors::Danger);
    if (ImGui::MenuItem("Delete")) m_editor->deleteGameObject(go);
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
}

void HierarchyPanel::blankContextMenu() {
    if (!ImGui::BeginPopup("##HierBlank")) return;
    EditorSelection& sel = m_editor->getSelection();
    if (ImGui::MenuItem("Create Empty")) m_editor->createEmptyGameObject();
    if (ImGui::MenuItem("Create Empty Child") && sel.has()) m_editor->createEmptyGameObject("Empty", sel.object);
    ImGui::EndPopup();
}