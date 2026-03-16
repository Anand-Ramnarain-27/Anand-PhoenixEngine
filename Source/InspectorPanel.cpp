#include "Globals.h"
#include "InspectorPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleCamera.h"
#include "ModuleScene.h"
#include "ModuleAssets.h"
#include "ModuleResources.h"
#include "ResourceMesh.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentCamera.h"
#include "ComponentMesh.h"
#include "ComponentLights.h"
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
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static constexpr float kDeg2Rad = 0.0174532925f;
static constexpr float kRad2Deg = 57.2957795f;

static GameObject* findPrefabRoot(GameObject* go) {
    GameObject* cur = go;
    while (cur) {
        if (PrefabManager::isPrefabInstance(cur)) return cur;
        cur = cur->getParent();
    }
    return nullptr;
}

void InspectorPanel::drawContent() {
    EditorSelection& sel = m_editor->getSelection();
    bool prefabMode = m_editor->getSceneManager() && m_editor->getSceneManager()->isEditingPrefab();

    if (prefabMode) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 p2 = { p.x + ImGui::GetContentRegionAvail().x, p.y + 28.f };
        ImGui::GetWindowDrawList()->AddRectFilled(p, p2, IM_COL32(25, 80, 25, 210));
        ImGui::GetWindowDrawList()->AddRect(p, p2, IM_COL32(50, 190, 50, 180));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 1.f, 0.35f, 1.f));
        ImGui::Text("  Prefab: %s", m_editor->getSceneManager()->getPrefabEditName().c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
    }

    if (!sel.has()) { textMuted("No GameObject selected."); return; }
    GameObject* go = sel.object;

    PrefabEditSession* session = m_editor->getPrefabSession();
    bool isEditRoot = prefabMode && session && go == session->rootObject;
    bool isInPrefabEdit = prefabMode && session && session->rootObject;

    bool active = go->isActive();
    if (ImGui::Checkbox("##active", &active)) go->setActive(active);
    ImGui::SameLine();

    char nameBuf[256];
    strncpy_s(nameBuf, go->getName().c_str(), sizeof(nameBuf) - 1);
    ImGui::SetNextItemWidth(-1);

    if (isEditRoot) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.28f, 0.18f, 0.04f, 1.f));
    if (ImGui::InputText("##goname", nameBuf, sizeof(nameBuf))) go->setName(nameBuf);
    if (isEditRoot) ImGui::PopStyleColor();

    if (isInPrefabEdit) {
        if (isEditRoot) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.20f, 1.f));
            ImGui::Text("Root  |  UID: %u", go->getUID());
            ImGui::PopStyleColor();
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.f));
            ImGui::Text("Child  |  UID: %u", go->getUID());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        const float bw = (ImGui::GetContentRegionAvail().x - 8.f) / 3.f;

        const PrefabInstanceData* instData = PrefabManager::getInstanceData(session->rootObject);
        bool hasChanges = instData && !instData->overrides.isEmpty();

        ImGui::BeginDisabled(!hasChanges);
        ImGui::PushStyleColor(ImGuiCol_Button, hasChanges ? ImVec4(0.14f, 0.42f, 0.14f, 1.f) : ImVec4(0.15f, 0.15f, 0.15f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.58f, 0.20f, 1.f));
        if (ImGui::Button("Apply", ImVec2(bw, 0)) && session->rootObject) {
            PrefabManager::applyToPrefab(session->rootObject);
            m_editor->log(("Applied prefab: " + session->prefabName).c_str(), EditorColors::Success);
            m_editor->exitPrefabEdit();
        }
        ImGui::PopStyleColor(2);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(hasChanges ? "Save all changes to the prefab file" : "No changes to apply");

        ImGui::SameLine(0, 4);

        ImGui::BeginDisabled(!hasChanges);
        if (ImGui::Button("Revert", ImVec2(bw, 0)) && session->rootObject) {
            PrefabManager::revertToPrefab(session->rootObject, session->isolatedScene.get());
            m_editor->getSelection().object = session->rootObject;
            m_editor->log(("Reverted prefab: " + session->prefabName).c_str(), EditorColors::Warning);
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(hasChanges ? "Discard edits and reload from disk" : "No changes to revert");

        ImGui::SameLine(0, 4);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.12f, 0.12f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.17f, 0.17f, 1.f));
        if (ImGui::Button("Exit", ImVec2(bw, 0))) m_editor->exitPrefabEdit();
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Leave prefab edit without saving  [Esc]");

        ImGui::Spacing();
    }
    else {
        ImGui::TextDisabled("UID: %u", go->getUID());
    }

    if (!prefabMode && PrefabManager::isPrefabInstance(go)) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Active);
        ImGui::Text("[Prefab: %s]", PrefabManager::getPrefabName(go).c_str());
        ImGui::PopStyleColor();
        const PrefabInstanceData* inst = PrefabManager::getInstanceData(go);
        if (inst && !inst->overrides.isEmpty()) {
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

    for (const auto& comp : go->getComponents()) {
        if (comp->getType() == Component::Type::Transform) continue;
        const char* label =
            comp->getType() == Component::Type::Camera ? "Camera" :
            comp->getType() == Component::Type::Mesh ? "Mesh" :
            comp->getType() == Component::Type::DirectionalLight ? "Directional Light" :
            comp->getType() == Component::Type::PointLight ? "Point Light" :
            comp->getType() == Component::Type::SpotLight ? "Spot Light" : "Component";

        ImGui::PushID((int)comp->getType());
        bool headerOpen = true;
        bool expanded = ImGui::CollapsingHeader(label, &headerOpen, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton);
        if (!headerOpen) { toRemove = comp->getType(); wantsRemove = true; }
        if (expanded) {
            std::string before;
            comp->onSave(before);

            if (comp->getType() == Component::Type::Camera) drawComponentCamera(static_cast<ComponentCamera*>(comp.get()));
            else if (comp->getType() == Component::Type::Mesh) drawComponentMesh(static_cast<ComponentMesh*>(comp.get()));
            else comp->onEditor();

            std::string after;
            comp->onSave(after);

            if (before != after) {
                GameObject* root = findPrefabRoot(go);
                if (root)
                    PrefabManager::markPropertyOverride(root, static_cast<int>(comp->getType()), "data");
            }
        }
        if (ImGui::BeginPopupContextItem("##compctx")) {
            ImGui::TextDisabled("%s", label);
            ImGui::Separator();
            if (ImGui::MenuItem("Remove Component")) { toRemove = comp->getType(); wantsRemove = true; }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (wantsRemove && toRemove != Component::Type::Transform) {
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
    if (ImGui::Button("Save") && strlen(prefabBuf) > 0) {
        if (PrefabManager::createPrefab(go, prefabBuf)) {
            PrefabInstanceData d; d.prefabName = prefabBuf; d.prefabUID = PrefabManager::makePrefabUID(prefabBuf);
            PrefabManager::linkInstance(go, d);
            m_editor->log(("Prefab saved: " + std::string(prefabBuf)).c_str(), EditorColors::Success);
            prefabBuf[0] = '\0';
        }
    }
}

void InspectorPanel::drawTransform() {
    GameObject* go = m_editor->getSelection().object;
    ComponentTransform* t = go->getTransform();
    if (!t || !ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return;

    auto markOverride = [&](const char* prop) {
        GameObject* root = findPrefabRoot(go);
        if (root) PrefabManager::markPropertyOverride(root, (int)Component::Type::Transform, prop);
        };

    float pos[3] = { t->position.x, t->position.y, t->position.z };
    if (ImGui::DragFloat3("Position", pos, 0.1f)) { t->position = { pos[0], pos[1], pos[2] }; t->markDirty(); markOverride("position"); }

    Vector3 euler = t->rotation.ToEuler();
    float deg[3] = { euler.x * kRad2Deg, euler.y * kRad2Deg, euler.z * kRad2Deg };
    if (ImGui::DragFloat3("Rotation", deg, 0.5f)) { t->rotation = Quaternion::CreateFromYawPitchRoll(deg[1] * kDeg2Rad, deg[0] * kDeg2Rad, deg[2] * kDeg2Rad); t->markDirty(); markOverride("rotation"); }

    float scl[3] = { t->scale.x, t->scale.y, t->scale.z };
    if (ImGui::DragFloat3("Scale", scl, 0.01f)) { t->scale = { scl[0], scl[1], scl[2] }; t->markDirty(); markOverride("scale"); }
}

void InspectorPanel::drawAddComponentMenu() {
    GameObject* go = m_editor->getSelection().object;
    const float btnW = 180.0f;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btnW) * 0.5f + ImGui::GetCursorPosX());
    if (ImGui::Button("Add Component", ImVec2(btnW, 0))) ImGui::OpenPopup("##AddComp");
    if (!ImGui::BeginPopup("##AddComp")) return;

    auto addComp = [&](const char* label, Component::Type type, bool hasIt) {
        if (!ImGui::MenuItem(label, nullptr, false, !hasIt)) return;
        go->addComponent(ComponentFactory::CreateComponent(type, go));
        GameObject* root = findPrefabRoot(go);
        if (root)
            if (PrefabInstanceData* inst = PrefabManager::getInstanceDataMutable(root))
                inst->overrides.addedComponentTypes.push_back((int)type);
        ImGui::CloseCurrentPopup();
        };

    addComp("Camera", Component::Type::Camera, go->getComponent<ComponentCamera>() != nullptr);
    addComp("Mesh", Component::Type::Mesh, go->getComponent<ComponentMesh>() != nullptr);
    ImGui::Separator();
    addComp("Directional Light", Component::Type::DirectionalLight, go->getComponent<ComponentDirectionalLight>() != nullptr);
    addComp("Point Light", Component::Type::PointLight, go->getComponent<ComponentPointLight>() != nullptr);
    addComp("Spot Light", Component::Type::SpotLight, go->getComponent<ComponentSpotLight>() != nullptr);
    ImGui::EndPopup();
}

void InspectorPanel::drawComponentCamera(ComponentCamera* cam) {
    bool isMain = cam->isMainCamera();
    if (ImGui::Checkbox("Is Active Camera", &isMain)) cam->setMainCamera(isMain);
    if (isMain) { ImGui::SameLine(); ImGui::TextColored(EditorColors::Success, "(rendering camera)"); }

    ModuleCamera* modCam = app->getCamera();
    bool isCulling = (modCam->cullSource == ModuleCamera::CullSource::GameCamera);
    if (ImGui::Checkbox("Is Culling Camera", &isCulling)) modCam->cullSource = isCulling ? ModuleCamera::CullSource::GameCamera : ModuleCamera::CullSource::EditorCamera;
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("When checked, this camera's frustum is used\nfor frustum culling instead of the editor camera.");
    ImGui::Separator();

    float fovDeg = cam->getFOV() * kRad2Deg;
    if (ImGui::SliderFloat("Field of View", &fovDeg, 10.0f, 170.0f)) cam->setFOV(fovDeg * kDeg2Rad);

    float nearP = cam->getNearPlane(), farP = cam->getFarPlane();
    if (ImGui::DragFloat("Near Plane", &nearP, 0.01f, 0.001f, farP - 0.01f)) cam->setNearPlane(nearP);
    if (ImGui::DragFloat("Far Plane", &farP, 1.0f, nearP + 0.01f, 10000.f)) cam->setFarPlane(farP);

    ImVec2 svSize = m_editor->getSceneViewSize();
    if (svSize.x > 0 && svSize.y > 0) ImGui::Text("Aspect Ratio: %.3f  (%dx%d)", svSize.x / svSize.y, (int)svSize.x, (int)svSize.y);
    ImGui::Separator();

    Vector4 bg = cam->getBackgroundColor();
    if (ImGui::ColorEdit4("Background", &bg.x)) cam->setBackgroundColor(bg);
    ImGui::Separator();

    ModuleScene* scene = m_editor->getActiveModuleScene();
    if (!scene) return;

    struct CamEntry { GameObject* go; ComponentCamera* cam; };
    std::vector<CamEntry> allCams;
    std::function<void(GameObject*)> collect = [&](GameObject* node) {
        if (auto* c = node->getComponent<ComponentCamera>()) allCams.push_back({ node, c });
        for (auto* child : node->getChildren()) collect(child);
        };
    collect(scene->getRoot());
    if (allCams.empty()) return;

    ImGui::TextDisabled("Scene cameras:");
    float listH = std::min<float>(allCams.size() * 22.0f + 8.0f, 120.0f);
    if (ImGui::BeginChild("##camList", ImVec2(0, listH), true)) {
        for (auto& e : allCams) {
            bool isThis = (e.cam == cam);
            bool isMain2 = e.cam->isMainCamera();
            std::string lbl = isMain2 ? "[Main] " + e.go->getName() : e.go->getName();
            ImGui::PushID(e.go->getUID());
            if (isThis) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
            if (ImGui::Selectable(lbl.c_str(), isMain2)) {
                for (auto& e2 : allCams) e2.cam->setMainCamera(false);
                e.cam->setMainCamera(true);
                m_editor->getSelection().object = e.go;
            }
            if (isThis) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("GameObject: %s", e.go->getName().c_str());
                ImGui::Text("FOV: %.1f deg", e.cam->getFOV() * kRad2Deg);
                ImGui::Text("Near: %.3f  Far: %.1f", e.cam->getNearPlane(), e.cam->getFarPlane());
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    if (!cam->isMainCamera())
        if (ImGui::Button("Make Active Camera")) {
            for (auto& e : allCams) e.cam->setMainCamera(false);
            cam->setMainCamera(true);
        }
}

void InspectorPanel::drawTexturePicker(ComponentMesh* mesh, Material* mat, int submeshIdx,
    const char* label, bool hasTex, const char* tooltip,
    std::function<void(ComPtr<ID3D12Resource>, D3D12_GPU_DESCRIPTOR_HANDLE)> onApply)
{
    std::string popupId = std::string("##TexPick_") + label + std::to_string(submeshIdx);
    std::string btnLabel = std::string(hasTex ? "Change##" : "Pick##") + label + std::to_string(submeshIdx);

    if (ImGui::SmallButton(btnLabel.c_str())) ImGui::OpenPopup(popupId.c_str());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);

    ImGui::SetNextWindowSize(ImVec2(300, 320), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(popupId.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;

    ImGui::TextDisabled("Double-click a texture to apply  [%s]", label);
    ImGui::Separator();
    static char texSearch[64] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##txs", "Search...", texSearch, sizeof(texSearch));
    ImGui::Separator();

    std::string ts = toLower(texSearch);
    std::string texDir = app->getFileSystem()->GetLibraryPath() + "Textures/";
    auto texFiles = app->getFileSystem()->GetFilesInDirectory(texDir.c_str(), ".dds");

    ImGui::BeginChild("##texlist", ImVec2(0, 220));
    bool anyTex = false;
    for (const auto& tf : texFiles) {
        std::string tname = fs::path(tf).stem().string();
        if (!ts.empty() && toLower(tname).find(ts) == std::string::npos) continue;
        if (ImGui::Selectable(("  [T]  " + tname).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)
            && ImGui::IsMouseDoubleClicked(0))
        {
            ComPtr<ID3D12Resource> tex;
            D3D12_GPU_DESCRIPTOR_HANDLE srv{};
            if (TextureImporter::Load(tf, tex, srv)) {
                onApply(tex, srv);
                app->getD3D12()->flush(); mesh->rebuildMaterialBuffers();
                m_editor->log(("Applied " + std::string(label) + ": " + tname).c_str(), EditorColors::Success);
            }
            else m_editor->log(("Failed: " + tf).c_str(), EditorColors::Danger);
            ImGui::CloseCurrentPopup();
        }
        anyTex = true;
    }
    if (!anyTex) { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("    No textures imported yet."); ImGui::PopStyleColor(); }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.12f, 0.12f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.17f, 0.17f, 1.f));
    if (ImGui::Button("Clear", ImVec2(80, 0)) && hasTex) {
        onApply({}, {});
        app->getD3D12()->flush(); mesh->rebuildMaterialBuffers();
        m_editor->log((std::string("Cleared ") + label + " map").c_str(), EditorColors::Warning);
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void InspectorPanel::drawComponentMesh(ComponentMesh* mesh) {
    bool hasEntries = !mesh->getEntries().empty();
    bool hasProcedural = (mesh->getProceduralModel() != nullptr);
    bool hasAnything = hasEntries || hasProcedural;
    std::string modelPath = mesh->getModelPath();
    std::string modelName = hasEntries ? (modelPath.empty() ? "(unknown)" : fs::path(modelPath).stem().string()) : hasProcedural ? "(procedural)" : "None";

    if (hasAnything) {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success);
        ImGui::Text("[M]  %s", modelName.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled(hasEntries ? "  %d submesh(es)" : "  procedural", (int)mesh->getEntries().size());
    }
    else ImGui::TextColored(EditorColors::Danger, "[M]  No model loaded");

    static char meshPathBuf[256] = "";
    ImGui::SetNextItemWidth(-160.0f);
    ImGui::InputTextWithHint("##meshpath", "Assets/Models/name/name.gltf", meshPathBuf, sizeof(meshPathBuf));
    ImGui::SameLine(0, 4);
    if (ImGui::Button("Load##ml", ImVec2(70, 0)) && strlen(meshPathBuf) > 0) {
        bool ok = mesh->loadModel(meshPathBuf);
        logResult(m_editor, ok, ("Loaded: " + std::string(meshPathBuf)).c_str(), ("Failed: " + std::string(meshPathBuf)).c_str());
        if (ok) meshPathBuf[0] = '\0';
    }
    ImGui::SameLine(0, 4);
    if (ImGui::Button("Pick##ml", ImVec2(70, 0))) ImGui::OpenPopup("##ModelPicker");

    ImGui::SetNextWindowSize(ImVec2(320, 280), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##ModelPicker")) {
        ImGui::TextDisabled("Imported models  (double-click to load)");
        ImGui::Separator();
        static char pickerSearch[64] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##pksearch", "Search...", pickerSearch, sizeof(pickerSearch));
        ImGui::Separator();
        std::string search = toLower(pickerSearch);
        std::string meshesRoot = app->getFileSystem()->GetLibraryPath() + "Meshes/";
        bool any = false;
        try {
            for (const auto& entry : fs::directory_iterator(meshesRoot)) {
                if (!entry.is_directory()) continue;
                std::string name = entry.path().filename().string();
                if (!search.empty() && toLower(name).find(search) == std::string::npos) continue;
                std::string assetPath = app->getAssets()->getAssetPathForScene(name);
                bool isCurrent = (modelName == name);
                if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success);
                bool clicked = ImGui::Selectable(("  [M]  " + name).c_str(), isCurrent, ImGuiSelectableFlags_AllowDoubleClick);
                if (isCurrent) ImGui::PopStyleColor();
                if (clicked && ImGui::IsMouseDoubleClicked(0)) {
                    if (assetPath.empty()) m_editor->log(("No asset path for: " + name).c_str(), EditorColors::Danger);
                    else { bool ok = mesh->loadModel(assetPath.c_str()); logResult(m_editor, ok, ("Loaded: " + name).c_str(), ("Failed: " + assetPath).c_str()); }
                    ImGui::CloseCurrentPopup();
                }
                any = true;
            }
        }
        catch (...) {}
        if (!any) { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("    No models imported yet."); ImGui::PopStyleColor(); }
        ImGui::EndPopup();
    }

    if (!hasAnything || !hasEntries) return;
    ImGui::Spacing();
    ImGui::SeparatorText("Materials");

    const auto& entries = mesh->getEntries();
    for (int mi = 0; mi < (int)entries.size(); ++mi) {
        const MeshEntry& e = entries[mi];
        Material* mat = (e.materialRes ? e.materialRes->getMaterial() : nullptr);
        if (!mat) { ImGui::PushID(mi); ImGui::TextDisabled("Submesh %d  � no material", mi); ImGui::PopID(); continue; }
        Material::Data& data = mat->getData();

        ImGui::PushID(mi);
        std::string header = "Submesh " + std::to_string(mi) + "  (" + modelName + ")";
        if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) { ImGui::PopID(); continue; }
        ImGui::Indent(8.0f);

        ImGui::SeparatorText("Base Color");
        if (ImGui::ColorEdit4("Color##bc", &data.baseColor.x)) { app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }
        ImGui::Spacing();

        if (mat->hasTexture()) { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success); ImGui::Text("[Albedo] Applied"); ImGui::PopStyleColor(); }
        else { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("[Albedo] None"); ImGui::PopStyleColor(); }
        ImGui::SameLine();
        drawTexturePicker(mesh, mat, mi, "Albedo", mat->hasTexture(), "Base color / albedo texture (.dds)",
            [&](ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) { mat->setBaseColorTexture(tex, srv); });

        ImGui::SeparatorText("Surface");
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));
        if (ImGui::BeginTable("##pbr", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 80.f);
            ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); textMuted("Metallic");
            ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##metal", &data.metallic, 0.f, 1.f)) { app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); textMuted("Roughness");
            ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##rough", &data.roughness, 0.f, 1.f)) { app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); textMuted("Exposure");
            ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##exposure", &data.exposure, 1.0f, 10.0f)) {
                app->getD3D12()->flush();
                mesh->rebuildMaterialBuffers();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        ImGui::SeparatorText("Normal Map");
        if (mat->hasNormalMap()) { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success); ImGui::Text("[N] Applied"); ImGui::PopStyleColor(); }
        else { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("[N] None"); ImGui::PopStyleColor(); }
        ImGui::SameLine();
        drawTexturePicker(mesh, mat, mi, "Normal", mat->hasNormalMap(), "Tangent-space normal map (.dds)",
            [&](ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) { mat->setNormalMap(tex, srv); });
        if (mat->hasNormalMap()) {
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("Strength##ns", &data.normalStrength, 0.f, 3.f, "%.2f")) { app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scales XY deviation of the normal map.\n1.0 = full strength, 0.0 = flat surface.");
        }

        ImGui::SeparatorText("Ambient Occlusion");
        if (mat->hasAOMap()) { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success); ImGui::Text("[AO] Applied"); ImGui::PopStyleColor(); }
        else { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("[AO] None"); ImGui::PopStyleColor(); }
        ImGui::SameLine();
        drawTexturePicker(mesh, mat, mi, "AO", mat->hasAOMap(), "Ambient Occlusion map - single channel (.dds)",
            [&](ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) { mat->setAOMap(tex, srv); });
        if (mat->hasAOMap()) {
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("Strength##aos", &data.aoStrength, 0.f, 1.f, "%.2f")) { app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = AO ignored (fully lit)\n1 = Full AO effect applied");
        }

        ImGui::SeparatorText("Emissive");
        if (mat->hasEmissive()) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.9f, 0.3f, 1.f)); ImGui::Text("[E] Applied"); ImGui::PopStyleColor(); }
        else { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("[E] None"); ImGui::PopStyleColor(); }
        ImGui::SameLine();
        drawTexturePicker(mesh, mat, mi, "Emissive", mat->hasEmissive(), "Emissive color map - additively blended (.dds)",
            [&](ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) { mat->setEmissiveMap(tex, srv); });
        if (ImGui::ColorEdit3("Tint##emtint", &data.emissiveFactor.x)) { app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Multiplied with emissive map.\nWhite = use map as-is, Black = no emission.");

        ImGui::Unindent(8.0f);
        ImGui::Spacing();
        ImGui::PopID();
    }
}