#include "Globals.h"
#include "InspectorPanel.h"
#include "EditorColors.h"
#include "ImGuiPass.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleCamera.h"
#include "SceneGraph.h"
#include "ModuleAssets.h"
#include "ModuleResources.h"
#include "ResourceMesh.h"
#include "Mesh.h"
#include "ResourceAnimation.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentCamera.h"
#include "ComponentMesh.h"
#include "ComponentLights.h"
#include "ComponentAnimation.h"
#include "ComponentDecal.h"
#include "ComponentBillboard.h"
#include "ComponentFactory.h"
#include "PrefabManager.h"
#include "TextureImporter.h"
#include "Material.h"
#include "Model.h"
#include "ModuleD3D12.h"
#include "MeshEntry.h"
#include "ResourceMaterial.h"
#include "EditorSelection.h"
#include "PrefabEditSession.h"
#include "ComponentScript.h"
#include "ComponentCharacterMotion.h"
#include "ComponentSimpleCharacterController.h"
#include "ComponentRigidbody.h"
#include "ComponentBounds.h"
#include "ComponentParticleSystem.h"
#include "ComponentTrail.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static constexpr float kDeg2Rad = 0.0174532925f;
static constexpr float kRad2Deg = 57.2957795f;

static GameObject* findPrefabRoot(GameObject* go){
    GameObject* cur = go;
    while (cur){
        if (PrefabManager::isPrefabInstance(cur)) return cur;
        cur = cur->getParent();
    }
    return nullptr;
}

void InspectorPanel::drawContent(){
    EditorSelection& sel = m_editor->getSelection();
    bool prefabMode = m_editor->getSceneManager() && m_editor->getSceneManager()->isEditingPrefab();

    if (prefabMode){
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 p2 = ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 28.f);
        ImGui::GetWindowDrawList()->AddRectFilled(p, p2, IM_COL32(25, 80, 25, 210));
        ImGui::GetWindowDrawList()->AddRect(p, p2, IM_COL32(50, 190, 50, 180));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 1.f, 0.35f, 1.f));
        ImGui::Text("  Prefab: %s", m_editor->getSceneManager()->getPrefabEditName().c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
    }

    if (!sel.has()){ textMuted("No GameObject selected."); return; }
    GameObject* go = sel.object;

    PrefabEditSession* session = m_editor->getPrefabSession();
    bool isEditRoot = prefabMode && session && go == session->rootObject;
    bool isInPrefabEdit = prefabMode && session && session->rootObject;

    {
        bool active = go->isActive();
        if (ImGui::Checkbox("##active", &active)) go->setActive(active);
        ImGui::SameLine(0, 6);

        char nameBuf[256];
        strncpy_s(nameBuf, go->getName().c_str(), sizeof(nameBuf) - 1);
        if (isEditRoot) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.28f, 0.18f, 0.04f, 1.f));
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputText("##goname", nameBuf, sizeof(nameBuf))) go->setName(nameBuf);
        if (isEditRoot) ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::Text("UID: %u", go->getUID());
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    if (isInPrefabEdit){
        if (isEditRoot){
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.20f, 1.f));
            ImGui::Text("Prefab root: %s", session->prefabName.c_str());
            ImGui::PopStyleColor();
        }

        const float bw = (ImGui::GetContentRegionAvail().x - 8.f) / 3.f;
        const PrefabInstanceData* instData = PrefabManager::getInstanceData(session->rootObject);
        bool hasChanges = instData && !instData->overrides.isEmpty();

        ImGui::BeginDisabled(!hasChanges);
        ImGui::PushStyleColor(ImGuiCol_Button, hasChanges ? ImVec4(0.14f, 0.42f, 0.14f, 1.f) : ImVec4(0.15f, 0.15f, 0.15f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.58f, 0.20f, 1.f));
        if (ImGui::Button("Apply", ImVec2(bw, 0)) && session->rootObject){
            PrefabManager::applyToPrefab(session->rootObject);
            m_editor->log(("Applied prefab: " + session->prefabName).c_str(), EditorColors::Success);
            m_editor->exitPrefabEdit();
        }
        ImGui::PopStyleColor(2);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(hasChanges ? "Save changes to prefab file" : "No changes");
        ImGui::SameLine(0, 4);
        ImGui::BeginDisabled(!hasChanges);
        if (ImGui::Button("Revert", ImVec2(bw, 0)) && session->rootObject){
            PrefabManager::revertToPrefab(session->rootObject, session->isolatedScene.get());
            m_editor->getSelection().object = session->rootObject;
            m_editor->log(("Reverted: " + session->prefabName).c_str(), EditorColors::Warning);
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.12f, 0.12f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.17f, 0.17f, 1.f));
        if (ImGui::Button("Exit", ImVec2(bw, 0))) m_editor->exitPrefabEdit();
        ImGui::PopStyleColor(2);
        ImGui::Spacing();
    }

    if (!prefabMode && PrefabManager::isPrefabInstance(go)){
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Active);
        ImGui::Text("[Prefab: %s]", PrefabManager::getPrefabName(go).c_str());
        ImGui::PopStyleColor();
        const PrefabInstanceData* inst = PrefabManager::getInstanceData(go);
        if (inst && !inst->overrides.isEmpty()){
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Override);
            ImGui::Text("(overrides)");
            ImGui::PopStyleColor();
        }
    }

    ImGui::Separator();
    drawTransform();

    Component::Type toRemove = Component::Type::Transform;
    bool wantsRemove = false;

    auto compBorderColor = [](Component::Type t) -> ImU32 {
        switch (t){
        case Component::Type::Mesh: return ImGui::ColorConvertFloat4ToU32(EditorColors::Ok);
        case Component::Type::Camera: return ImGui::ColorConvertFloat4ToU32(EditorColors::Inf);
        case Component::Type::DirectionalLight:
        case Component::Type::PointLight:
        case Component::Type::SpotLight: return ImGui::ColorConvertFloat4ToU32(EditorColors::Warn);
        case Component::Type::Animation: return ImGui::ColorConvertFloat4ToU32(EditorColors::Acc);
        case Component::Type::Rigidbody: return ImGui::ColorConvertFloat4ToU32(EditorColors::Hot);
        case Component::Type::Bounds: return ImGui::ColorConvertFloat4ToU32(EditorColors::Crit);
        case Component::Type::Script: return ImGui::ColorConvertFloat4ToU32(EditorColors::Tx1);
        case Component::Type::ParticleSystem:
        case Component::Type::Trail: return ImGui::ColorConvertFloat4ToU32(EditorColors::Acc);
        default: return ImGui::ColorConvertFloat4ToU32(EditorColors::Tx2);
        }
    };

    for (const auto& comp : go->getComponents()){
        if (comp->getType() == Component::Type::Transform) continue;
        const char* label =
            comp->getType() == Component::Type::Camera ? "Camera" :
            comp->getType() == Component::Type::Mesh ? "Mesh" :
            comp->getType() == Component::Type::DirectionalLight ? "Directional Light" :
            comp->getType() == Component::Type::PointLight ? "Point Light" :
            comp->getType() == Component::Type::SpotLight ? "Spot Light" :
            comp->getType() == Component::Type::Script ? "Script" :
            comp->getType() == Component::Type::Animation ? "Animation" :
            comp->getType() == Component::Type::CharacterMotion ? "Character Motion" :
            comp->getType() == Component::Type::SimpleCharacterController ? "Character Controller" :
            comp->getType() == Component::Type::Rigidbody ? "Rigidbody" :
            comp->getType() == Component::Type::Bounds ? "Bounds" :
            comp->getType() == Component::Type::Decal ? "Decal" :
            comp->getType() == Component::Type::Billboard ? "Billboard" :
            comp->getType() == Component::Type::ParticleSystem ? "Particle System" :
            comp->getType() == Component::Type::Trail ? "Trail" :
            "Component";

        ImGui::PushID((int)comp->getType());

        float headerY = ImGui::GetCursorScreenPos().y;

        bool headerOpen = true;
        bool expanded = ImGui::CollapsingHeader(label, &headerOpen,
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap |
            ImGuiTreeNodeFlags_ClipLabelForTrailingButton);
        if (!headerOpen){ toRemove = comp->getType(); wantsRemove = true; }

        {
            ImVec2 rMin = ImVec2(ImGui::GetWindowPos().x, headerY);
            ImVec2 rMax = ImVec2(rMin.x + 2.f, ImGui::GetItemRectMax().y);
            ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, compBorderColor(comp->getType()));
        }
        if (expanded){
            std::string before;
            comp->onSave(before);

            comp->onEditor();

            std::string after;
            comp->onSave(after);

            if (before != after){
                GameObject* root = findPrefabRoot(go);
                if (root)
                    PrefabManager::markPropertyOverride(root, static_cast<int>(comp->getType()), "data");
            }
        }
        if (ImGui::BeginPopupContextItem("##compctx")){
            ImGui::TextDisabled("%s", label);
            ImGui::Separator();
            if (ImGui::MenuItem("Remove Component")){ toRemove = comp->getType(); wantsRemove = true; }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (wantsRemove && toRemove != Component::Type::Transform){
        if (toRemove == Component::Type::Mesh)
            app->getD3D12()->flush();
        go->removeComponentByType(toRemove);
    }

    ImGui::Spacing(); ImGui::Separator();
    drawAddComponentMenu();
    ImGui::Spacing();
    ImGui::SeparatorText("Prefab");

    static char prefabBuf[256] = "";
    ImGui::SetNextItemWidth(-80);
    ImGui::InputText("##prefabname", prefabBuf, sizeof(prefabBuf));
    ImGui::SameLine();
    if (ImGui::Button("Save") && strlen(prefabBuf) > 0){
        if (PrefabManager::createPrefab(go, prefabBuf)){
            PrefabInstanceData d; d.prefabName = prefabBuf; d.prefabUID = PrefabManager::makePrefabUID(prefabBuf);
            PrefabManager::linkInstance(go, d);
            m_editor->log(("Prefab saved: " + std::string(prefabBuf)).c_str(), EditorColors::Success);
            prefabBuf[0] = '\0';
        }
    }
}

static bool vec3Row(const char* label, float v[3], float speed,
                    const char* fmt = "%.2f"){
    static const ImVec4 kAxisCol[3] = {
        ImVec4(0.86f, 0.32f, 0.32f, 1.f),
        ImVec4(0.30f, 0.78f, 0.45f, 1.f),
        ImVec4(0.30f, 0.55f, 0.90f, 1.f),
    };
    static const char* kAxisLbl[3] = { "X","Y","Z" };
    static const char* kIds[3] = { "##vx","##vy","##vz" };

    bool changed = false;
    ImGui::PushID(label);

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
    ImGui::Text("%s", label);
    ImGui::PopStyleColor();
    ImGui::SameLine(76.f);

    const float avail = ImGui::GetContentRegionAvail().x;
    const float fieldW = (avail - 2.f) / 3.f;

    for (int i = 0; i < 3; ++i){
        if (i > 0) ImGui::SameLine(0, 2.f);

        ImVec2 p = ImGui::GetCursorScreenPos();
        const float chipW = 16.f, h = 20.f;
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + chipW, p.y + h),
            ImGui::ColorConvertFloat4ToU32(kAxisCol[i]), 2.f);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(p.x + 4.f, p.y + 3.f),
            IM_COL32(255, 255, 255, 220), kAxisLbl[i]);
        ImGui::SetCursorScreenPos(ImVec2(p.x + chipW, p.y));

        ImGui::SetNextItemWidth(fieldW - chipW);
        if (ImGui::DragFloat(kIds[i], &v[i], speed, 0.f, 0.f, fmt))
            changed = true;
    }

    ImGui::PopID();
    return changed;
}

void InspectorPanel::drawTransform(){
    GameObject* go = m_editor->getSelection().object;
    ComponentTransform* t = go->getTransform();
    if (!t || !ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return;

    auto markOverride = [&](const char* prop){
        GameObject* root = findPrefabRoot(go);
        if (root) PrefabManager::markPropertyOverride(root, (int)Component::Type::Transform, prop);
    };

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 5.f));

    float pos[3] = { t->position.x, t->position.y, t->position.z };
    if (vec3Row("Position", pos, 0.1f))
        { t->position = { pos[0], pos[1], pos[2] }; t->markDirty(); markOverride("position"); }

    Vector3 euler = t->rotation.ToEuler();
    float deg[3] = { euler.x * kRad2Deg, euler.y * kRad2Deg, euler.z * kRad2Deg };
    if (vec3Row("Rotation", deg, 0.5f, "%.1f"))
        { t->rotation = Quaternion::CreateFromYawPitchRoll(deg[1]*kDeg2Rad, deg[0]*kDeg2Rad, deg[2]*kDeg2Rad); t->markDirty(); markOverride("rotation"); }

    float scl[3] = { t->scale.x, t->scale.y, t->scale.z };
    if (vec3Row("Scale", scl, 0.01f))
        { t->scale = { scl[0], scl[1], scl[2] }; t->markDirty(); markOverride("scale"); }

    ImGui::PopStyleVar();
}

void InspectorPanel::drawAddComponentMenu(){
    GameObject* go = m_editor->getSelection().object;
    const float btnW = 180.0f;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btnW) * 0.5f + ImGui::GetCursorPosX());
    if (ImGui::Button("Add Component", ImVec2(btnW, 0))) ImGui::OpenPopup("##AddComp");
    if (!ImGui::BeginPopup("##AddComp")) return;

    auto addComp = [&](const char* label, Component::Type type, bool hasIt){
        if (!ImGui::MenuItem(label, nullptr, false, !hasIt)) return;
        go->addComponent(ComponentFactory::CreateComponent(type, go));
        GameObject* root = findPrefabRoot(go);
        if (root)
            if (PrefabInstanceData* inst = PrefabManager::getInstanceDataMutable(root))
                inst->overrides.addedComponentTypes.push_back((int)type);
        ImGui::CloseCurrentPopup();
        };

    addComp("Mesh", Component::Type::Mesh, go->getComponent<ComponentMesh>() != nullptr);
    addComp("Camera", Component::Type::Camera, go->getComponent<ComponentCamera>() != nullptr);
    ImGui::Separator();
    if (ImGui::BeginMenu("Lights")){
        addComp("Directional Light", Component::Type::DirectionalLight, go->getComponent<ComponentDirectionalLight>() != nullptr);
        addComp("Point Light", Component::Type::PointLight, go->getComponent<ComponentPointLight>() != nullptr);
        addComp("Spot Light", Component::Type::SpotLight, go->getComponent<ComponentSpotLight>() != nullptr);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    addComp("Rigidbody", Component::Type::Rigidbody, go->getComponent<ComponentRigidbody>() != nullptr);
    addComp("Bounds", Component::Type::Bounds, go->getComponent<ComponentBounds>() != nullptr);
    ImGui::Separator();
    addComp("Animation", Component::Type::Animation, go->getComponent<ComponentAnimation>() != nullptr);
    addComp("Character Motion", Component::Type::CharacterMotion, go->getComponent<ComponentCharacterMotion>() != nullptr);
    addComp("Character Controller",Component::Type::SimpleCharacterController,go->getComponent<ComponentSimpleCharacterController>() != nullptr);
    ImGui::Separator();
    addComp("Decal", Component::Type::Decal, go->getComponent<ComponentDecal>() != nullptr);
    if (ImGui::BeginMenu("Particles")){
        addComp("Billboard", Component::Type::Billboard, go->getComponent<ComponentBillboard>() != nullptr);
        addComp("Particle System", Component::Type::ParticleSystem, go->getComponent<ComponentParticleSystem>() != nullptr);
        addComp("Trail", Component::Type::Trail, go->getComponent<ComponentTrail>() != nullptr);
        ImGui::EndMenu();
    }
    ImGui::EndPopup();
}
