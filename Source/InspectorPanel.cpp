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
#include "ComponentDirectionalLight.h"
#include "ComponentPointLight.h"
#include "ComponentSpotLight.h"
#include "ComponentFactory.h"
#include "PrefabManager.h"
#include "TextureImporter.h"
#include "Material.h"
#include "Model.h"
#include "EditorSelection.h"
#include <filesystem>
#include <algorithm>

void InspectorPanel::draw()
{
    ImGui::Begin("Inspector", &open);

    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has())
    {
        ImGui::TextDisabled("No GameObject selected.");
        ImGui::End();
        return;
    }

    GameObject* go = sel.object;

    bool active = go->isActive();
    if (ImGui::Checkbox("##active", &active)) go->setActive(active);
    ImGui::SameLine();

    char nameBuf[256];
    strncpy_s(nameBuf, go->getName().c_str(), sizeof(nameBuf) - 1);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##goname", nameBuf, sizeof(nameBuf)))
        go->setName(nameBuf);

    ImGui::TextDisabled("UID: %u", go->getUID());
    ImGui::Separator();

    drawTransform();

    Component::Type toRemove = Component::Type::Transform;
    bool            wantsRemove = false;

    for (const auto& comp : go->getComponents())
    {
        if (comp->getType() == Component::Type::Transform) continue;

        ImGui::PushID((int)comp->getType());

        const char* label =
            comp->getType() == Component::Type::Camera ? "Camera" :
            comp->getType() == Component::Type::Mesh ? "Mesh" :
            comp->getType() == Component::Type::DirectionalLight ? "Directional Light" :
            comp->getType() == Component::Type::PointLight ? "Point Light" :
            comp->getType() == Component::Type::SpotLight ? "Spot Light" : "Component";

        bool headerOpen = true;
        bool expanded = ImGui::CollapsingHeader(label, &headerOpen,
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_AllowItemOverlap |
            ImGuiTreeNodeFlags_ClipLabelForTrailingButton);

        if (!headerOpen) { toRemove = comp->getType(); wantsRemove = true; }

        if (expanded)
        {
            if (comp->getType() == Component::Type::Camera) drawComponentCamera(static_cast<ComponentCamera*>(comp.get()));
            else if (comp->getType() == Component::Type::Mesh)   drawComponentMesh(static_cast<ComponentMesh*>(comp.get()));
            else                                                  comp->onEditor();
        }

        if (ImGui::BeginPopupContextItem("##compctx"))
        {
            ImGui::TextDisabled("%s", label);
            ImGui::Separator();
            if (ImGui::MenuItem("Remove Component")) { toRemove = comp->getType(); wantsRemove = true; }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (wantsRemove && toRemove != Component::Type::Transform)
        go->removeComponentByType(toRemove);

    ImGui::Spacing();
    ImGui::Separator();
    drawAddComponentMenu();

    ImGui::Spacing();
    ImGui::SeparatorText("Prefab");
    static char prefabBuf[256] = "";
    ImGui::SetNextItemWidth(-80);
    ImGui::InputText("##prefabname", prefabBuf, sizeof(prefabBuf));
    ImGui::SameLine();
    if (ImGui::Button("Save") && strlen(prefabBuf) > 0)
    {
        if (PrefabManager::createPrefab(go, prefabBuf))
        {
            m_editor->log(("Prefab saved: " + std::string(prefabBuf)).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
            prefabBuf[0] = '\0';
        }
    }

    ImGui::End();
}

void InspectorPanel::drawTransform()
{
    GameObject* go = m_editor->getSelection().object;
    ComponentTransform* t = go->getTransform();
    if (!t || !ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return;

    float pos[3] = { t->position.x, t->position.y, t->position.z };
    if (ImGui::DragFloat3("Position", pos, 0.1f))
    {
        t->position = { pos[0], pos[1], pos[2] };
        t->markDirty();
    }

    Vector3 euler = t->rotation.ToEuler();
    float   deg[3] = { euler.x * 57.2957795f, euler.y * 57.2957795f, euler.z * 57.2957795f };
    if (ImGui::DragFloat3("Rotation", deg, 0.5f))
    {
        t->rotation = Quaternion::CreateFromYawPitchRoll(
            deg[1] * 0.0174532925f, deg[0] * 0.0174532925f, deg[2] * 0.0174532925f);
        t->markDirty();
    }

    float scl[3] = { t->scale.x, t->scale.y, t->scale.z };
    if (ImGui::DragFloat3("Scale", scl, 0.01f))
    {
        t->scale = { scl[0], scl[1], scl[2] };
        t->markDirty();
    }
}

void InspectorPanel::drawAddComponentMenu()
{
    GameObject* go = m_editor->getSelection().object;

    const float btnW = 180.0f;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btnW) * 0.5f + ImGui::GetCursorPosX());
    if (ImGui::Button("Add Component", ImVec2(btnW, 0))) ImGui::OpenPopup("##AddComp");

    if (!ImGui::BeginPopup("##AddComp")) return;

    auto addComp = [&](const char* label, Component::Type type, bool hasIt)
        {
            if (ImGui::MenuItem(label, nullptr, false, !hasIt))
            {
                go->addComponent(ComponentFactory::CreateComponent(type, go));
                ImGui::CloseCurrentPopup();
            }
        };

    addComp("Camera", Component::Type::Camera, go->getComponent<ComponentCamera>() != nullptr);
    addComp("Mesh", Component::Type::Mesh, go->getComponent<ComponentMesh>() != nullptr);
    ImGui::Separator();
    addComp("Directional Light", Component::Type::DirectionalLight, go->getComponent<ComponentDirectionalLight>() != nullptr);
    addComp("Point Light", Component::Type::PointLight, go->getComponent<ComponentPointLight>() != nullptr);
    addComp("Spot Light", Component::Type::SpotLight, go->getComponent<ComponentSpotLight>() != nullptr);

    ImGui::EndPopup();
}

void InspectorPanel::drawComponentCamera(ComponentCamera* cam)
{
    bool isMain = cam->isMainCamera();
    if (ImGui::Checkbox("Is Active Camera", &isMain)) cam->setMainCamera(isMain);
    if (isMain) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "(rendering camera)"); }

    ModuleCamera* modCam = app->getCamera();
    bool isCulling = (modCam->cullSource == ModuleCamera::CullSource::GameCamera);
    if (ImGui::Checkbox("Is Culling Camera", &isCulling))
        modCam->cullSource = isCulling ? ModuleCamera::CullSource::GameCamera : ModuleCamera::CullSource::EditorCamera;
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("When checked, this camera's frustum is used\nfor frustum culling instead of the editor camera.");

    ImGui::Separator();

    float fovDeg = cam->getFOV() * 57.2957795f;
    if (ImGui::SliderFloat("Field of View", &fovDeg, 10.0f, 170.0f)) cam->setFOV(fovDeg * 0.0174532925f);

    float nearP = cam->getNearPlane(), farP = cam->getFarPlane();
    if (ImGui::DragFloat("Near Plane", &nearP, 0.01f, 0.001f, farP - 0.01f)) cam->setNearPlane(nearP);
    if (ImGui::DragFloat("Far Plane", &farP, 1.0f, nearP + 0.01f, 10000.f)) cam->setFarPlane(farP);

    ImVec2 svSize = m_editor->getSceneViewSize();
    if (svSize.x > 0 && svSize.y > 0)
        ImGui::Text("Aspect Ratio: %.3f  (%dx%d)", svSize.x / svSize.y, (int)svSize.x, (int)svSize.y);

    ImGui::Separator();
    Vector4 bg = cam->getBackgroundColor();
    if (ImGui::ColorEdit4("Background", &bg.x)) cam->setBackgroundColor(bg);
    ImGui::Separator();

    ModuleScene* scene = m_editor->getActiveModuleScene();
    if (!scene) return;

    struct CamEntry { GameObject* go; ComponentCamera* cam; };
    std::vector<CamEntry> allCams;
    std::function<void(GameObject*)> collect = [&](GameObject* node)
        {
            if (auto* c = node->getComponent<ComponentCamera>()) allCams.push_back({ node, c });
            for (auto* child : node->getChildren()) collect(child);
        };
    collect(scene->getRoot());
    if (allCams.empty()) return;

    ImGui::TextDisabled("Scene cameras:");
    float listH = std::min<float>(allCams.size() * 22.0f + 8.0f, 120.0f);
    if (ImGui::BeginChild("##camList", ImVec2(0, listH), true))
    {
        for (auto& e : allCams)
        {
            bool isThis = (e.cam == cam);
            bool isMain2 = e.cam->isMainCamera();
            std::string lbl = isMain2 ? "[Main] " + e.go->getName() : e.go->getName();

            ImGui::PushID(e.go->getUID());
            if (isThis) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 1.0f, 1.0f));

            if (ImGui::Selectable(lbl.c_str(), isMain2))
            {
                for (auto& e2 : allCams) e2.cam->setMainCamera(false);
                e.cam->setMainCamera(true);
                m_editor->getSelection().object = e.go;
            }
            if (isThis) ImGui::PopStyleColor();

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("GameObject: %s", e.go->getName().c_str());
                ImGui::Text("FOV: %.1f deg", e.cam->getFOV() * 57.2957795f);
                ImGui::Text("Near: %.3f  Far: %.1f", e.cam->getNearPlane(), e.cam->getFarPlane());
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    if (!cam->isMainCamera())
    {
        if (ImGui::Button("Make Active Camera"))
        {
            for (auto& e : allCams) e.cam->setMainCamera(false);
            cam->setMainCamera(true);
        }
    }
}

void InspectorPanel::drawComponentMesh(ComponentMesh* mesh)
{
    namespace fs = std::filesystem;

    Model* model = mesh->getModel();

    std::string modelName = "None";
    std::string modelPath;
    if (model)
    {
        UID uid = mesh->getModelUID();
        if (uid != 0)
        {
            modelPath = app->getAssets()->getPathFromUID(uid);
            modelName = modelPath.empty() ? "(unknown)" : fs::path(modelPath).stem().string();
        }
        else
        {
            modelName = "(procedural)";
        }
    }

    if (model)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
        ImGui::Text("[M]  %s", modelName.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("  %d mesh(es)  %d mat(s)",
            (int)model->getMeshes().size(), (int)model->getMaterials().size());
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f), "[M]  No model loaded");
    }

    static char meshPathBuf[256] = "";
    ImGui::SetNextItemWidth(-160.0f);
    ImGui::InputTextWithHint("##meshpath", "Assets/Models/name/name.gltf", meshPathBuf, sizeof(meshPathBuf));
    ImGui::SameLine(0, 4);

    if (ImGui::Button("Load##ml", ImVec2(70, 0)) && strlen(meshPathBuf) > 0)
    {
        bool ok = mesh->loadModel(meshPathBuf);
        m_editor->log(ok ? ("Loaded: " + std::string(meshPathBuf)).c_str()
            : ("Failed: " + std::string(meshPathBuf)).c_str(),
            ok ? ImVec4(0.6f, 1, 0.6f, 1) : ImVec4(1, 0.4f, 0.4f, 1));
        if (ok) meshPathBuf[0] = '\0';
    }
    ImGui::SameLine(0, 4);
    if (ImGui::Button("Pick##ml", ImVec2(70, 0))) ImGui::OpenPopup("##ModelPicker");

    ImGui::SetNextWindowSize(ImVec2(320, 280), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##ModelPicker"))
    {
        ImGui::TextDisabled("Imported models  (double-click to load)");
        ImGui::Separator();
        static char pickerSearch[64] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##pksearch", "Search...", pickerSearch, sizeof(pickerSearch));
        ImGui::Separator();

        std::string search(pickerSearch);
        std::transform(search.begin(), search.end(), search.begin(), ::tolower);
        std::string meshesRoot = app->getFileSystem()->GetLibraryPath() + "Meshes/";
        bool any = false;

        try
        {
            for (const auto& entry : fs::directory_iterator(meshesRoot))
            {
                if (!entry.is_directory()) continue;
                std::string name = entry.path().filename().string();
                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (!search.empty() && lower.find(search) == std::string::npos) continue;

                std::string gltf = "Assets/Models/" + name + "/" + name + ".gltf";
                bool        isCurrent = (modelName == name);

                if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
                bool clicked = ImGui::Selectable(("  [M]  " + name).c_str(), isCurrent,
                    ImGuiSelectableFlags_AllowDoubleClick);
                if (isCurrent) ImGui::PopStyleColor();

                if (clicked && ImGui::IsMouseDoubleClicked(0))
                {
                    bool ok = mesh->loadModel(gltf.c_str());
                    m_editor->log(ok ? ("Loaded: " + name).c_str() : ("Failed: " + gltf).c_str(),
                        ok ? ImVec4(0.6f, 1, 0.6f, 1) : ImVec4(1, 0.4f, 0.4f, 1));
                    ImGui::CloseCurrentPopup();
                }
                any = true;
            }
        }
        catch (...) {}

        if (!any)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1));
            ImGui::Text("    No models imported yet.");
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }

    if (!model) return;

    ImGui::Spacing();
    ImGui::SeparatorText("Materials");
    auto& mats = model->getMaterialsMutable();

    for (int mi = 0; mi < (int)mats.size(); ++mi)
    {
        Material* mat = mats[mi].get();
        if (!mat) continue;

        ImGui::PushID(mi);
        std::string matLabel = "Material " + std::to_string(mi);

        if (ImGui::CollapsingHeader(matLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(8.0f);
            if (ImGui::SmallButton("Pick texture"))
                ImGui::OpenPopup(("##TexPicker" + std::to_string(mi)).c_str());

            ImGui::SetNextWindowSize(ImVec2(300, 240), ImGuiCond_Appearing);
            if (ImGui::BeginPopup(("##TexPicker" + std::to_string(mi)).c_str()))
            {
                ImGui::TextDisabled("Imported textures  (double-click to apply)");
                ImGui::Separator();
                static char texSearch[64] = "";
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##txs", "Search...", texSearch, sizeof(texSearch));

                std::string ts(texSearch);
                std::transform(ts.begin(), ts.end(), ts.begin(), ::tolower);
                std::string texDir = app->getFileSystem()->GetLibraryPath() + "Textures/";
                auto        texFiles = app->getFileSystem()->GetFilesInDirectory(texDir.c_str(), ".dds");
                bool anyTex = false;

                for (const auto& tf : texFiles)
                {
                    std::string tname = fs::path(tf).stem().string();
                    std::string tlower = tname;
                    std::transform(tlower.begin(), tlower.end(), tlower.begin(), ::tolower);
                    if (!ts.empty() && tlower.find(ts) == std::string::npos) continue;

                    if (ImGui::Selectable(("  [T]  " + tname).c_str(), false,
                        ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(0))
                    {
                        ComPtr<ID3D12Resource>      tex;
                        D3D12_GPU_DESCRIPTOR_HANDLE srv{};
                        if (TextureImporter::Load(tf, tex, srv))
                        {
                            mat->setBaseColorTexture(tex, srv);
                            mesh->rebuildMaterialBuffers();
                            m_editor->log(("Applied texture: " + tname).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
                        }
                        else m_editor->log(("Failed: " + tf).c_str(), ImVec4(1, 0.4f, 0.4f, 1));
                        ImGui::CloseCurrentPopup();
                    }
                    anyTex = true;
                }
                if (!anyTex)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1));
                    ImGui::Text("    No textures imported yet.");
                    ImGui::PopStyleColor();
                }
                ImGui::EndPopup();
            }
            ImGui::Unindent(8.0f);
        }
        ImGui::PopID();
    }
}