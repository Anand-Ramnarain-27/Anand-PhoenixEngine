#include "Globals.h"
#include "HierarchyPanel.h"
#include "EditorColors.h"
#include "ImGuiPass.h"
#include "ModuleEditor.h"
#include "ModuleScene.h"
#include "Application.h"
#include "GameObject.h"
#include "ComponentCamera.h"
#include "ComponentMesh.h"
#include "ComponentLights.h"
#include "ComponentAnimation.h"
#include "ComponentRigidbody.h"
#include <algorithm>
#include "ComponentFactory.h"
#include "EditorSelection.h"
#include "PrefabManager.h"
#include "SceneManager.h"
#include "ComponentScript.h"
#include "HotReloadManager.h"

static GameObject* findPrefabRoot(GameObject* go) {
    GameObject* cur = go;
    while (cur) {
        if (PrefabManager::isPrefabInstance(cur)) return cur;
        cur = cur->getParent();
    }
    return nullptr;
}

// Returns a tag string + color for a GameObject based on its components.
static const char* goTag(GameObject* go, ImVec4& outColor) {
    if (go->getComponent<ComponentDirectionalLight>() || go->getComponent<ComponentPointLight>() || go->getComponent<ComponentSpotLight>()) {
        outColor = EditorColors::Warn; return "LIGHT";
    }
    if (go->getComponent<ComponentCamera>())    { outColor = EditorColors::Inf;  return "CAM"; }
    if (go->getComponent<ComponentAnimation>()) { outColor = EditorColors::Acc;  return "SKIN"; }
    if (go->getComponent<ComponentMesh>())      { outColor = EditorColors::Ok;   return "MESH"; }
    if (go->getComponent<ComponentRigidbody>()) { outColor = EditorColors::Crit; return "PHYS"; }
    outColor = {}; return nullptr;
}

void HierarchyPanel::drawContent() {
    ModuleScene* scene = m_editor->getActiveModuleScene();
    if (!scene) return;
    EditorSelection& sel = m_editor->getSelection();
    bool prefabMode = m_editor->getSceneManager() && m_editor->getSceneManager()->isEditingPrefab();

    if (prefabMode) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 p2 = { p.x + ImGui::GetContentRegionAvail().x, p.y + 26.f };
        ImGui::GetWindowDrawList()->AddRectFilled(p, p2, IM_COL32(30, 90, 30, 200));
        ImGui::GetWindowDrawList()->AddRect(p, p2, IM_COL32(50, 200, 50, 180));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 1.f, 0.35f, 1.f));
        ImGui::Text("  Prefab: %s", m_editor->getSceneManager()->getPrefabEditName().c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing(); ImGui::Separator();
    }
    else {
        // Search bar
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 6.f, 4.f });
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##hier_search", "\xF0\x9F\x94\x8D  Search scene...", m_search, sizeof(m_search));
        ImGui::PopStyleVar();
        ImGui::PushStyleColor(ImGuiCol_Separator, EditorColors::Line);
        ImGui::Separator();
        ImGui::PopStyleColor();

        // Inline [+] button in header region using foreground draw list overlay
        // (drawn by the panel header itself in EditorPanel::draw via ImGui window title)
    }

    drawNode(scene->getRoot(), prefabMode);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered()) ImGui::OpenPopup("##HierBlank");
    blankContextMenu();
}

void HierarchyPanel::drawNode(GameObject* go, bool prefabMode, bool isRoot) {
    if (!go) return;

    // Search filter: skip nodes whose name doesn't contain the search string.
    // Always show selected node regardless of filter.
    EditorSelection& sel = m_editor->getSelection();
    if (m_search[0] != '\0' && go != sel.object) {
        std::string name = go->getName();
        std::string srch = m_search;
        // Case-insensitive
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        std::transform(srch.begin(), srch.end(), srch.begin(), ::tolower);
        if (name.find(srch) == std::string::npos) {
            // Still recurse into children (they might match)
            for (auto* c : go->getChildren()) drawNode(c, prefabMode, false);
            return;
        }
    }

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_FramePadding;
    if (go == sel.object) flags |= ImGuiTreeNodeFlags_Selected;
    if (go->getChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (prefabMode && isRoot) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    if (sel.renaming == go) {
        ImGui::SetKeyboardFocusHere();
        bool done = ImGui::InputText("##rename", sel.renameBuffer, sizeof(sel.renameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (done || ImGui::IsItemDeactivated()) { go->setName(sel.renameBuffer); sel.renaming = nullptr; }
        for (auto* c : go->getChildren()) drawNode(c, prefabMode, false);
        return;
    }

    bool isPrefabRoot = prefabMode && isRoot;
    bool isPrefabInst = !isPrefabRoot && PrefabManager::isPrefabInstance(go);
    if (isPrefabRoot)  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.20f, 1.f));
    else if (isPrefabInst) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.0f, 1.f));

    const char* label = (isPrefabRoot && m_editor->getSceneManager())
        ? m_editor->getSceneManager()->getPrefabEditName().c_str()
        : go->getName().c_str();

    // Override header colors for selection: accent-dim bg + 2px left accent border
    bool isSelected = (go == sel.object);
    if (isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Header,        EditorColors::AccDim);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, { EditorColors::AccDim.x, EditorColors::AccDim.y, EditorColors::AccDim.z, 0.28f });
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  { EditorColors::AccDim.x, EditorColors::AccDim.y, EditorColors::AccDim.z, 0.45f });
    }

    bool nodeOpen = ImGui::TreeNodeEx((void*)(uintptr_t)go->getUID(), flags, "%s", label);

    // 2px left accent border for selected row
    if (isSelected) {
        ImVec2 rMin = ImGui::GetItemRectMin();
        ImVec2 rMax = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(
            { rMin.x, rMin.y }, { rMin.x + 2.f, rMax.y },
            ImGui::ColorConvertFloat4ToU32(EditorColors::Acc));
        ImGui::PopStyleColor(3);
    }

    if (isPrefabRoot || isPrefabInst) ImGui::PopStyleColor();

    // ---- Tag badge (right-aligned, before end of row) ----
    {
        ImVec4 tagColor;
        const char* tag = goTag(go, tagColor);
        if (tag) {
            ImGui::PushFont(g_fontMono);
            ImVec2 ts = ImGui::CalcTextSize(tag);
            float badgeW = ts.x + 8.f;
            ImVec2 rMin  = ImGui::GetItemRectMin();
            ImVec2 rMax  = ImGui::GetItemRectMax();
            float bx     = rMax.x - badgeW - 4.f;
            float by     = rMin.y + (rMax.y - rMin.y - ts.y) * 0.5f;
            ImGui::GetWindowDrawList()->AddRectFilled(
                { bx, by - 2.f }, { bx + badgeW, by + ts.y + 2.f },
                ImGui::ColorConvertFloat4ToU32({ tagColor.x, tagColor.y, tagColor.z, 0.18f }), 3.f);
            ImGui::GetWindowDrawList()->AddRect(
                { bx, by - 2.f }, { bx + badgeW, by + ts.y + 2.f },
                ImGui::ColorConvertFloat4ToU32({ tagColor.x, tagColor.y, tagColor.z, 0.5f }), 3.f);
            ImGui::GetWindowDrawList()->AddText(
                ImGui::GetFont(), ImGui::GetFontSize() * 0.85f,
                { bx + 4.f, by }, ImGui::ColorConvertFloat4ToU32(tagColor), tag);
            ImGui::PopFont();
        }
    }

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
