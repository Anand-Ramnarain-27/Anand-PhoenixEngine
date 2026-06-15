#include "Globals.h"
#include "ComponentMesh.h"
#include "ModuleEditor.h"
#include "EditorColors.h"
#include "TextureImporter.h"
#include "ModuleFileSystem.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "Application.h"
#include "ModuleGPUResources.h"
#include "ModuleAssets.h"
#include "ModuleResources.h"
#include "ModuleCamera.h"
#include "ModuleD3D12.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <d3dx12.h>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <cstdarg>
#include <cctype>

using namespace rapidjson;

ComponentMesh::ComponentMesh(GameObject* owner) : Component(owner){}
ComponentMesh::~ComponentMesh(){ releaseEntries(); }

void ComponentMesh::releaseEntries(){
    for (auto& e : m_entries){
        if (e.meshRes) app->getResources()->ReleaseResource(e.meshRes);
        if (e.materialRes) app->getResources()->ReleaseResource(e.materialRes);
        e.meshRes = nullptr;
        e.materialRes = nullptr;
    }
    m_entries.clear();
}

static ComPtr<ID3D12Resource> makeMaterialCB(const Material::Data& data){
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(Material::Data) + 255) & ~255);
    ComPtr<ID3D12Resource> buf;
    app->getD3D12()->getDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf));
    void* mapped = nullptr;
    buf->Map(0, nullptr, &mapped);
    memcpy(mapped, &data, sizeof(Material::Data));
    buf->Unmap(0, nullptr);
    buf->SetName(L"MaterialCB");
    return buf;
}

void ComponentMesh::rebuildEntry(MeshEntry& e){
    if (!e.instanceMaterial){
        e.instanceMaterial = std::make_unique<Material>();
        if (e.materialRes && e.materialRes->getMaterial())
            *e.instanceMaterial = *e.materialRes->getMaterial();
    }
    Material::Data data = e.instanceMaterial ? e.instanceMaterial->getData() : Material::Data{};
    if (e.materialCB) m_deferredRelease.push_back(std::move(e.materialCB));
    e.materialCB = makeMaterialCB(data);
}

void ComponentMesh::markMaterialsDirty(){ m_materialsDirty = true; }

void ComponentMesh::flushDeferredReleases(){
    m_deferredRelease.clear();
    if (m_materialsDirty){ m_materialsDirty = false; rebuildMaterialBuffers(); }
}

void ComponentMesh::rebuildMaterialBuffers(){
    for (auto& e : m_entries) rebuildEntry(e);
    if (m_proceduralModel){
        m_proceduralMaterialBuffers.clear();
        for (const auto& mat : m_proceduralModel->getMaterials()) m_proceduralMaterialBuffers.push_back(makeMaterialCB(mat->getData()));
    }
}

static std::string resolveCanonicalPath(const char* filePath, UID& outUID){
    std::string normPath = filePath;
    for (char& c : normPath) if (c == '\\') c = '/';
    std::string sceneName = std::filesystem::path(normPath).stem().string();

    std::string libPath = app->getFileSystem()->GetLibraryPath();
    if (normPath.find(libPath) == 0){
        std::string resolved = app->getAssets()->getAssetPathForScene(sceneName);
        if (resolved.empty()){ outUID = 0; return {}; }
        normPath = resolved;
        for (char& c : normPath) if (c == '\\') c = '/';
    }

    outUID = app->getAssets()->findUID(normPath);
    if (outUID == 0){
        std::string resolved = app->getAssets()->getAssetPathForScene(
            std::filesystem::path(normPath).stem().string());
        if (!resolved.empty()) outUID = app->getAssets()->findUID(resolved);
    }
    if (outUID == 0) return {};

    std::string canonical = app->getAssets()->getPathFromUID(outUID);
    return canonical.empty() ? normPath : canonical;
}

bool ComponentMesh::loadModel(const char* filePath){
    releaseEntries();
    m_proceduralModel.reset();
    m_proceduralMaterialBuffers.clear();
    m_modelPath.clear();
    m_modelUID = 0;
    m_meshFileStart = -1;
    m_meshFileCount = 0;

    UID sceneUID = 0;
    std::string canonicalPath = resolveCanonicalPath(filePath, sceneUID);
    if (canonicalPath.empty()){ LOG("ComponentMesh: Cannot resolve '%s' - import it first", filePath); return false; }
    m_modelUID = sceneUID;
    m_modelPath = canonicalPath;
    std::string sceneName = std::filesystem::path(canonicalPath).stem().string();

    std::string meshFolder = app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/";
    int meshCount = 0;
    while (app->getFileSystem()->Exists((meshFolder + std::to_string(meshCount) + ".mesh").c_str())) ++meshCount;
    if (meshCount == 0){
        LOG("ComponentMesh: No meshes found, forcing reimport for '%s'", sceneName.c_str());
        return false;
    }

    for (int i = 0; i < meshCount; ++i){
        MeshEntry e;
        e.meshUID = app->getAssets()->findSubUID(canonicalPath, "mesh", i);
        e.materialUID = 0;

        if (e.meshUID)
            e.meshRes = app->getResources()->RequestMesh(e.meshUID);

        if (e.meshRes && e.meshRes->getMesh()){
            int matIdx = e.meshRes->getMesh()->getMaterialIndex();

            if (matIdx >= 0){
                e.materialUID = app->getAssets()->findSubUID(
                    canonicalPath, "mat", matIdx);
            }

            if (e.materialUID == 0){
                LOG("ComponentMesh: submesh %d has invalid material, using default", i);
                e.materialUID = 0;
            }
        }

        if (e.materialUID != 0){
            e.materialRes = app->getResources()->RequestMaterial(e.materialUID);
        }
        else {
            e.materialRes = nullptr;
        }
        rebuildEntry(e);
        m_entries.push_back(std::move(e));
    }

    computeLocalAABB();
    return !m_entries.empty();
}

bool ComponentMesh::loadMeshSubset(const std::string& assetPath, int startMesh, int meshCount){
    releaseEntries();
    m_proceduralModel.reset();
    m_proceduralMaterialBuffers.clear();
    m_modelPath.clear();
    m_modelUID = 0;
    m_meshFileStart = -1;
    m_meshFileCount = 0;
    if (meshCount <= 0) return false;

    UID sceneUID = 0;
    std::string canonical = resolveCanonicalPath(assetPath.c_str(), sceneUID);
    if (canonical.empty()){ LOG("ComponentMesh: Cannot resolve '%s' - import it first", assetPath.c_str()); return false; }
    m_modelUID = sceneUID;
    m_modelPath = canonical;
    m_meshFileStart = startMesh;
    m_meshFileCount = meshCount;

    for (int i = startMesh; i < startMesh + meshCount; ++i){
        MeshEntry e;
        e.meshUID = app->getAssets()->findSubUID(canonical, "mesh", i);
        if (e.meshUID) e.meshRes = app->getResources()->RequestMesh(e.meshUID);
        if (e.meshRes && e.meshRes->getMesh()){
            int matIdx = e.meshRes->getMesh()->getMaterialIndex();
            if (matIdx >= 0) e.materialUID = app->getAssets()->findSubUID(canonical, "mat", matIdx);
        }
        if (e.materialUID != 0) e.materialRes = app->getResources()->RequestMaterial(e.materialUID);
        rebuildEntry(e);
        m_entries.push_back(std::move(e));
    }
    computeLocalAABB();
    return !m_entries.empty();
}

void ComponentMesh::addMeshEntry(UID meshUID, UID materialUID){
    MeshEntry e;
    e.meshUID = meshUID;
    e.materialUID = materialUID;
    if (e.meshUID) e.meshRes = app->getResources()->RequestMesh(e.meshUID);
    if (e.materialUID) e.materialRes = app->getResources()->RequestMaterial(e.materialUID);
    rebuildEntry(e);
    m_entries.push_back(std::move(e));
}

void ComponentMesh::setSkinData(const ResourceModel::Skin& skin, std::vector<GameObject*> joints){
    m_localSkin = skin;
    m_skinJoints = std::move(joints);
    m_hasSkin = true;
}

void ComponentMesh::setMorphWeight(int index, float weight){
    if (index < 0 || index >= MAX_MORPH_WEIGHTS) return;
    m_morphWeights[index] = weight;
    m_morphWeightsDirty = true;
}

void ComponentMesh::setProceduralModel(std::unique_ptr<Model> model){
    releaseEntries();
    m_proceduralModel = std::shared_ptr<Model>(std::move(model));
    m_modelUID = 0;
    m_modelPath.clear();
    m_proceduralMaterialBuffers.clear();
    for (const auto& mat : m_proceduralModel->getMaterials()) m_proceduralMaterialBuffers.push_back(makeMaterialCB(mat->getData()));
    computeLocalAABB();
}

void ComponentMesh::overrideMaterial(int slot, UID materialUID){
    if (slot < 0 || slot >= (int)m_entries.size()) return;
    MeshEntry& e = m_entries[slot];
    if (e.materialRes){ app->getResources()->ReleaseResource(e.materialRes); e.materialRes = nullptr; }
    e.instanceMaterial.reset();
    e.materialUID = materialUID;
    if (materialUID != 0) e.materialRes = app->getResources()->RequestMaterial(materialUID);
    rebuildEntry(e);
}

void ComponentMesh::setLODLevels(std::vector<LODLevel> levels){
    m_lodLevels = std::move(levels);
    m_currentLOD = 0;
}

void ComponentMesh::updateLOD(float coverage, int forceIndex){
    if (m_lodLevels.empty() || m_entries.empty()) return;
    m_lastScreenCoverage = coverage;

    int selected = (int)m_lodLevels.size() - 1;
    if (forceIndex >= 0){
        selected = std::min(forceIndex, (int)m_lodLevels.size() - 1);
    } else {
        for (int i = 0; i < (int)m_lodLevels.size(); ++i){
            if (coverage > m_lodLevels[i].screenCoverageThreshold){ selected = i; break; }
        }
    }

    if (selected == m_currentLOD) return;
    m_currentLOD = selected;

    UID newMeshUID = m_lodLevels[selected].meshUID;
    if (newMeshUID == 0 || newMeshUID == m_entries[0].meshUID) return;

    MeshEntry& e = m_entries[0];
    if (e.meshRes) app->getResources()->ReleaseResource(e.meshRes);
    e.meshUID = newMeshUID;
    e.meshRes = app->getResources()->RequestMesh(e.meshUID);
}

void ComponentMesh::render(ID3D12GraphicsCommandList* ){
}

namespace fs = std::filesystem;

static std::string toLower(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static void logResult(ModuleEditor* ed, bool ok, const char* good, const char* bad){
    ed->log(ok ? good : bad, ok ? EditorColors::Success : EditorColors::Danger);
}

static void textMuted(const char* fmt, ...){
    va_list a; va_start(a, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted);
    ImGui::TextV(fmt, a);
    ImGui::PopStyleColor();
    va_end(a);
}

static void drawTexturePicker(ComponentMesh* mesh, Material* mat, int submeshIdx,
    const char* label, bool hasTex, const char* tooltip,
    std::function<void(ComPtr<ID3D12Resource>, D3D12_GPU_DESCRIPTOR_HANDLE)> onApply){
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
    for (const auto& tf : texFiles){
        std::string tname = fs::path(tf).stem().string();
        if (!ts.empty() && toLower(tname).find(ts) == std::string::npos) continue;
        if (ImGui::Selectable(("  [T]  " + tname).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)
            && ImGui::IsMouseDoubleClicked(0)){
            ComPtr<ID3D12Resource> tex;
            D3D12_GPU_DESCRIPTOR_HANDLE srv{};
            if (TextureImporter::Load(tf, tex, srv)){
                onApply(tex, srv);
                app->getD3D12()->flush(); mesh->rebuildMaterialBuffers();
                app->getEditor()->log(("Applied " + std::string(label) + ": " + tname).c_str(), EditorColors::Success);
            }
            else app->getEditor()->log(("Failed: " + tf).c_str(), EditorColors::Danger);
            ImGui::CloseCurrentPopup();
        }
        anyTex = true;
    }
    if (!anyTex){ ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("    No textures imported yet."); ImGui::PopStyleColor(); }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.12f, 0.12f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.17f, 0.17f, 1.f));
    if (ImGui::Button("Clear", ImVec2(80, 0)) && hasTex){
        onApply({}, {});
        app->getD3D12()->flush(); mesh->rebuildMaterialBuffers();
        app->getEditor()->log((std::string("Cleared ") + label + " map").c_str(), EditorColors::Warning);
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void ComponentMesh::onEditor(){
    ComponentMesh* mesh = this;
    bool hasEntries = !mesh->getEntries().empty();
    bool hasProcedural = (mesh->getProceduralModel() != nullptr);
    bool hasAnything = hasEntries || hasProcedural;
    std::string modelPath = mesh->getModelPath();
    std::string modelName = hasEntries ? (modelPath.empty() ? "(unknown)" : fs::path(modelPath).stem().string()) : hasProcedural ? "(procedural)" : "None";

    if (hasAnything){
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
    if (ImGui::Button("Load##ml", ImVec2(70, 0)) && strlen(meshPathBuf) > 0){
        bool ok = mesh->loadModel(meshPathBuf);
        logResult(app->getEditor(), ok, ("Loaded: " + std::string(meshPathBuf)).c_str(), ("Failed: " + std::string(meshPathBuf)).c_str());
        if (ok) meshPathBuf[0] = '\0';
    }
    ImGui::SameLine(0, 4);
    if (ImGui::Button("Pick##ml", ImVec2(70, 0))) ImGui::OpenPopup("##ModelPicker");

    ImGui::SetNextWindowSize(ImVec2(320, 280), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##ModelPicker")){
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
            for (const auto& entry : fs::directory_iterator(meshesRoot)){
                if (!entry.is_directory()) continue;
                std::string name = entry.path().filename().string();
                if (!search.empty() && toLower(name).find(search) == std::string::npos) continue;
                std::string assetPath = app->getAssets()->getAssetPathForScene(name);
                bool isCurrent = (modelName == name);
                if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success);
                bool clicked = ImGui::Selectable(("  [M]  " + name).c_str(), isCurrent, ImGuiSelectableFlags_AllowDoubleClick);
                if (isCurrent) ImGui::PopStyleColor();
                if (clicked && ImGui::IsMouseDoubleClicked(0)){
                    if (assetPath.empty()) app->getEditor()->log(("No asset path for: " + name).c_str(), EditorColors::Danger);
                    else { bool ok = mesh->loadModel(assetPath.c_str()); logResult(app->getEditor(), ok, ("Loaded: " + name).c_str(), ("Failed: " + assetPath).c_str()); }
                    ImGui::CloseCurrentPopup();
                }
                any = true;
            }
        }
        catch (...){}
        if (!any){ ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("    No models imported yet."); ImGui::PopStyleColor(); }
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Skinning");
    if (mesh->hasSkinData()){
        int jointCount = (int)mesh->getSkinJoints().size();
        ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Skin data: %d joints", jointCount);

        const auto& entries = mesh->getEntries();
        int gpuCount = 0, totalCount = 0;
        for (const auto& e : entries){
            if (!e.meshRes || !e.meshRes->getMesh()) continue;
            ++totalCount;
            const Mesh* m = e.meshRes->getMesh();
            bool onGPU = (m->getBoneWeightBufferVA() != 0);
            bool hasBW = m->hasBoneWeights();
            if (onGPU) ++gpuCount;
            ImGui::PushID(totalCount);
            if (onGPU)
                ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f),
                    "  [%d] Bone weights: GPU", totalCount - 1);
            else if (hasBW)
                ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f),
                    "  [%d] Bone weights: CPU only (uploading...)", totalCount - 1);
            else
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f),
                    "  [%d] No bone weights (re-import model)", totalCount - 1);
            ImGui::PopID();
        }
        if (totalCount > 0 && gpuCount < totalCount)
            ImGui::TextDisabled("  Tip: delete model from Library/ and re-drag to reimport");
    } else {
        ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "No skin data");
        ImGui::TextDisabled("  (normal for non-skinned meshes)");
    }

    if (!hasAnything || !hasEntries) return;
    ImGui::Spacing();
    ImGui::SeparatorText("Materials");

    auto& entries = mesh->getEntries();
    for (int mi = 0; mi < (int)entries.size(); ++mi){
        MeshEntry& e = entries[mi];
        Material* mat = e.instanceMaterial.get();
        if (!mat) mat = e.material;
        if (!mat && e.materialRes) mat = e.materialRes->getMaterial();
        if (!mat){ ImGui::PushID(mi); ImGui::TextDisabled("Submesh %d  (no material)", mi); ImGui::PopID(); continue; }
        Material::Data& data = mat->getData();

        ImGui::PushID(mi);
        std::string header = "Submesh " + std::to_string(mi) + "  (" + modelName + ")";
        if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)){ ImGui::PopID(); continue; }
        ImGui::Indent(8.0f);

        ImGui::SeparatorText("Base Color");
        if (ImGui::ColorEdit4("Color##bc", &data.baseColor.x)){ app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }
        ImGui::Spacing();

        if (mat->hasTexture()){ ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success); ImGui::Text("[Albedo] Applied"); ImGui::PopStyleColor(); }
        else { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("[Albedo] None"); ImGui::PopStyleColor(); }
        ImGui::SameLine();
        drawTexturePicker(mesh, mat, mi, "Albedo", mat->hasTexture(), "Base color / albedo texture (.dds)",
            [&](ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv){ mat->setBaseColorTexture(tex, srv); });

        ImGui::SeparatorText("Surface");
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));
        if (ImGui::BeginTable("##pbr", 2, ImGuiTableFlags_SizingFixedFit)){
            ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 80.f);
            ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); textMuted("Metallic");
            ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##metal", &data.metallic, 0.f, 1.f)){ app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); textMuted("Roughness");
            ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##rough", &data.roughness, 0.f, 1.f)){ app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        ImGui::SeparatorText("Normal Map");
        if (mat->hasNormalMap()){ ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success); ImGui::Text("[N] Applied"); ImGui::PopStyleColor(); }
        else { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("[N] None"); ImGui::PopStyleColor(); }
        ImGui::SameLine();
        drawTexturePicker(mesh, mat, mi, "Normal", mat->hasNormalMap(), "Tangent-space normal map (.dds)",
            [&](ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv){ mat->setNormalMap(tex, srv); });
        if (mat->hasNormalMap()){
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("Strength##ns", &data.normalStrength, 0.f, 3.f, "%.2f")){ app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scales XY deviation of the normal map.\n1.0 = full strength, 0.0 = flat surface.");
        }

        ImGui::SeparatorText("Ambient Occlusion");
        if (mat->hasAOMap()){ ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success); ImGui::Text("[AO] Applied"); ImGui::PopStyleColor(); }
        else { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("[AO] None"); ImGui::PopStyleColor(); }
        ImGui::SameLine();
        drawTexturePicker(mesh, mat, mi, "AO", mat->hasAOMap(), "Ambient Occlusion map - single channel (.dds)",
            [&](ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv){ mat->setAOMap(tex, srv); });
        if (mat->hasAOMap()){
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("Strength##aos", &data.aoStrength, 0.f, 1.f, "%.2f")){ app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = AO ignored (fully lit)\n1 = Full AO effect applied");
        }

        ImGui::SeparatorText("Emissive");
        if (mat->hasEmissive()){ ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.9f, 0.3f, 1.f)); ImGui::Text("[E] Applied"); ImGui::PopStyleColor(); }
        else { ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted); ImGui::Text("[E] None"); ImGui::PopStyleColor(); }
        ImGui::SameLine();
        drawTexturePicker(mesh, mat, mi, "Emissive", mat->hasEmissive(), "Emissive color map - additively blended (.dds)",
            [&](ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv){ mat->setEmissiveMap(tex, srv); });
        if (ImGui::ColorEdit3("Tint##emtint", &data.emissiveFactor.x)){ app->getD3D12()->flush(); mesh->rebuildMaterialBuffers(); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Multiplied with emissive map.\nWhite = use map as-is, Black = no emission.");

        ImGui::Unindent(8.0f);
        ImGui::Spacing();
        ImGui::PopID();
    }
}

void ComponentMesh::onDrawGizmos(){
    if (!m_drawBindPose || !m_hasSkin) return;

    const int n = (int)m_skinJoints.size();
    for (int i = 0; i < n; ++i){
        Matrix bindWorld = m_localSkin.inverseBindMatrices[i].Invert();
        Vector3 myPos = bindWorld.Translation();
        ddVec3 to = { myPos.x, myPos.y, myPos.z };

        dd::axisTriad(bindWorld.m[0], 0.f, 0.05f);

        GameObject* parentGO = m_skinJoints[i]->getParent();
        for (int p = 0; p < n; ++p){
            if (m_skinJoints[p] == parentGO){
                Matrix parentBind = m_localSkin.inverseBindMatrices[p].Invert();
                Vector3 parentPos = parentBind.Translation();
                ddVec3 from = { parentPos.x, parentPos.y, parentPos.z };
                dd::line(from, to, dd::colors::Cyan);
                break;
            }
        }
    }
}

void ComponentMesh::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("ModelUID", m_modelUID, a);
    Value pathVal; pathVal.SetString(m_modelPath.c_str(), a);
    doc.AddMember("ModelPath", pathVal, a);
    doc.AddMember("MeshFileStart", m_meshFileStart, a);
    doc.AddMember("MeshFileCount", m_meshFileCount, a);
    Value entries(kArrayType);
    for (const auto& e : m_entries){ Value ev(kObjectType); ev.AddMember("meshUID", e.meshUID, a); ev.AddMember("materialUID", e.materialUID, a); entries.PushBack(ev, a); }
    doc.AddMember("Entries", entries, a);

    if (m_hasSkin){
        Value jointNames(kArrayType);
        for (auto* j : m_skinJoints){ Value n; n.SetString(j->getName().c_str(), a); jointNames.PushBack(n, a); }
        doc.AddMember("SkinJointNames", jointNames, a);

        Value ibms(kArrayType);
        for (const auto& mat : m_localSkin.inverseBindMatrices){
            Value mArr(kArrayType);
            const float* f = reinterpret_cast<const float*>(&mat);
            for (int k = 0; k < 16; ++k) mArr.PushBack(f[k], a);
            ibms.PushBack(mArr, a);
        }
        doc.AddMember("SkinIBMs", ibms, a);
    }

    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

static GameObject* findInSubtree(GameObject* node, const std::string& name){
    if (node->getName() == name) return node;
    for (auto* child : node->getChildren()) if (auto* found = findInSubtree(child, name)) return found;
    return nullptr;
}

static GameObject* hierarchyRoot(GameObject* go){
    while (go->getParent() && go->getParent()->getParent()) go = go->getParent();
    return go;
}

void ComponentMesh::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;

    std::string path;
    if (doc.HasMember("ModelPath") && doc["ModelPath"].IsString())
        path = doc["ModelPath"].GetString();

    if (!path.empty()){
        int start = (doc.HasMember("MeshFileStart") && doc["MeshFileStart"].IsInt()) ? doc["MeshFileStart"].GetInt() : -1;
        int count = (doc.HasMember("MeshFileCount") && doc["MeshFileCount"].IsInt()) ? doc["MeshFileCount"].GetInt() : 0;
        if (start >= 0 && count > 0)
            loadMeshSubset(path, start, count);
        else
            loadModel(path.c_str());

        if (doc.HasMember("Entries") && doc["Entries"].IsArray()){
            const auto& arr = doc["Entries"].GetArray();
            for (int i = 0; i < (int)arr.Size() && i < (int)m_entries.size(); ++i){
                UID savedMat = arr[i]["materialUID"].GetUint64();
                if (savedMat != m_entries[i].materialUID) overrideMaterial(i, savedMat);
            }
        }
    } else if (doc.HasMember("Entries") && doc["Entries"].IsArray()){
        releaseEntries();
        const auto& arr = doc["Entries"].GetArray();
        for (const auto& ev : arr){
            UID meshUID = ev.HasMember("meshUID") ? ev["meshUID"].GetUint64() : 0;
            UID matUID = ev.HasMember("materialUID") ? ev["materialUID"].GetUint64() : 0;
            if (meshUID) addMeshEntry(meshUID, matUID);
        }
        computeLocalAABB();
    }

    if (doc.HasMember("SkinJointNames") && doc.HasMember("SkinIBMs")){
        const auto& names = doc["SkinJointNames"].GetArray();
        const auto& ibmsArr = doc["SkinIBMs"].GetArray();
        if (names.Size() == ibmsArr.Size() && names.Size() > 0){
            m_pendingSkin = ResourceModel::Skin{};
            m_pendingSkin.jointNodeIndices.resize(names.Size());
            m_pendingSkin.inverseBindMatrices.resize(names.Size());
            m_pendingJointNames.clear();
            m_pendingJointNames.reserve(names.Size());

            for (SizeType k = 0; k < names.Size(); ++k){
                m_pendingSkin.jointNodeIndices[k] = static_cast<int>(k);
                const auto& mArr = ibmsArr[k].GetArray();
                float* f = reinterpret_cast<float*>(&m_pendingSkin.inverseBindMatrices[k]);
                for (int fi = 0; fi < 16; ++fi) f[fi] = mArr[fi].GetFloat();
                m_pendingJointNames.emplace_back(names[k].GetString());
            }
            m_hasPendingSkin = true;
        }
    } else if (doc.HasMember("SkinJointNames") && !doc.HasMember("SkinIBMs")){
        LOG("ComponentMesh: scene JSON has SkinJointNames but no SkinIBMs — "
            "IBP was not saved. Re-import the model and re-save the scene.");
    }
}

void ComponentMesh::resolveDeferredSkin(){
    if (!m_hasPendingSkin) return;
    m_hasPendingSkin = false;

    GameObject* root = hierarchyRoot(owner);
    std::vector<GameObject*> joints;
    joints.reserve(m_pendingJointNames.size());
    bool ok = true;
    for (const auto& name : m_pendingJointNames){
        GameObject* jgo = findInSubtree(root, name);
        if (!jgo){ LOG("ComponentMesh: skin joint '%s' not found", name.c_str()); ok = false; break; }
        joints.push_back(jgo);
    }
    if (ok) setSkinData(m_pendingSkin, std::move(joints));

    m_pendingJointNames.clear();
    m_pendingJointNames.shrink_to_fit();
    m_pendingSkin = ResourceModel::Skin{};
}

void ComponentMesh::computeLocalAABB(){
    m_localAABBMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    m_localAABBMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    m_hasAABB = false;
    for (const auto& e : m_entries){
        if (!e.meshRes || !e.meshRes->getMesh()) continue;
        const Mesh* mesh = e.meshRes->getMesh();
        if (!mesh->hasAABB()) continue;
        m_localAABBMin = Vector3::Min(m_localAABBMin, mesh->getAABBMin());
        m_localAABBMax = Vector3::Max(m_localAABBMax, mesh->getAABBMax());
        m_hasAABB = true;
    }
    if (m_proceduralModel){
        for (const auto& mesh : m_proceduralModel->getMeshes()){
            if (!mesh || !mesh->hasAABB()) continue;
            m_localAABBMin = Vector3::Min(m_localAABBMin, mesh->getAABBMin());
            m_localAABBMax = Vector3::Max(m_localAABBMax, mesh->getAABBMax());
            m_hasAABB = true;
        }
    }
}

void ComponentMesh::getWorldAABB(Vector3& outMin, Vector3& outMax) const{
    auto* t = owner->getTransform();
    Matrix world = t ? t->getGlobalMatrix() : Matrix::Identity;
    const Vector3& mn = m_localAABBMin;
    const Vector3& mx = m_localAABBMax;
    Vector3 corners[8] = {
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},
        {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},
    };
    outMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    outMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& c : corners){
        Vector3 wc = Vector3::Transform(c, world);
        outMin = Vector3::Min(outMin, wc);
        outMax = Vector3::Max(outMax, wc);
    }
}
