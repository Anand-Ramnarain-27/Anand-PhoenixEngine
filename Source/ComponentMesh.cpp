#include "Globals.h"
#include "ComponentMesh.h"
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

using namespace rapidjson;

ComponentMesh::ComponentMesh(GameObject* owner) : Component(owner) {}
ComponentMesh::~ComponentMesh() { releaseEntries(); }

void ComponentMesh::releaseEntries(){
    for (auto& e : m_entries) {
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
    // Initialise the per-instance material copy the first time (or after a reset).
    // Subsequent calls (e.g. from markMaterialsDirty) reuse the existing copy so
    // that in-editor changes to one instance are not clobbered.
    if (!e.instanceMaterial) {
        e.instanceMaterial = std::make_unique<Material>();
        if (e.materialRes && e.materialRes->getMaterial())
            *e.instanceMaterial = *e.materialRes->getMaterial();
    }
    Material::Data data = e.instanceMaterial ? e.instanceMaterial->getData() : Material::Data{};
    if (e.materialCB) m_deferredRelease.push_back(std::move(e.materialCB));
    e.materialCB = makeMaterialCB(data);
}

void ComponentMesh::markMaterialsDirty() { m_materialsDirty = true; }

void ComponentMesh::flushDeferredReleases(){
    m_deferredRelease.clear();
    if (m_materialsDirty) { m_materialsDirty = false; rebuildMaterialBuffers(); }
}

void ComponentMesh::rebuildMaterialBuffers(){
    for (auto& e : m_entries) rebuildEntry(e);
    if (m_proceduralModel) {
        m_proceduralMaterialBuffers.clear();
        for (const auto& mat : m_proceduralModel->getMaterials()) m_proceduralMaterialBuffers.push_back(makeMaterialCB(mat->getData()));
    }
}

static std::string resolveCanonicalPath(const char* filePath, UID& outUID){
    std::string normPath = filePath;
    for (char& c : normPath) if (c == '\\') c = '/';
    std::string sceneName = std::filesystem::path(normPath).stem().string();

    std::string libPath = app->getFileSystem()->GetLibraryPath();
    if (normPath.find(libPath) == 0) {
        std::string resolved = app->getAssets()->getAssetPathForScene(sceneName);
        if (resolved.empty()) { outUID = 0; return {}; }
        normPath = resolved;
        for (char& c : normPath) if (c == '\\') c = '/';
    }

    outUID = app->getAssets()->findUID(normPath);
    if (outUID == 0) {
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
    if (canonicalPath.empty()) { LOG("ComponentMesh: Cannot resolve '%s' - import it first", filePath); return false; }
    m_modelUID = sceneUID;
    m_modelPath = canonicalPath;
    std::string sceneName = std::filesystem::path(canonicalPath).stem().string();

    std::string meshFolder = app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/";
    int meshCount = 0;
    while (app->getFileSystem()->Exists((meshFolder + std::to_string(meshCount) + ".mesh").c_str())) ++meshCount;
    if (meshCount == 0) {
        LOG("ComponentMesh: No meshes found, forcing reimport for '%s'", sceneName.c_str());
        return false;
    }

    for (int i = 0; i < meshCount; ++i) {
        MeshEntry e;
        e.meshUID = app->getAssets()->findSubUID(canonicalPath, "mesh", i);
        e.materialUID = 0;

        if (e.meshUID)
            e.meshRes = app->getResources()->RequestMesh(e.meshUID);

        if (e.meshRes && e.meshRes->getMesh()) {
            int matIdx = e.meshRes->getMesh()->getMaterialIndex();

            if (matIdx >= 0) {
                e.materialUID = app->getAssets()->findSubUID(
                    canonicalPath, "mat", matIdx);
            }

            if (e.materialUID == 0) {
                LOG("ComponentMesh: submesh %d has invalid material, using default", i);
                e.materialUID = 0;
            }
        }

        if (e.materialUID != 0) {
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
    if (canonical.empty()) { LOG("ComponentMesh: Cannot resolve '%s' - import it first", assetPath.c_str()); return false; }
    m_modelUID = sceneUID;
    m_modelPath = canonical;
    m_meshFileStart = startMesh;
    m_meshFileCount = meshCount;

    for (int i = startMesh; i < startMesh + meshCount; ++i) {
        MeshEntry e;
        e.meshUID = app->getAssets()->findSubUID(canonical, "mesh", i);
        if (e.meshUID) e.meshRes = app->getResources()->RequestMesh(e.meshUID);
        if (e.meshRes && e.meshRes->getMesh()) {
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
    if (e.materialRes) { app->getResources()->ReleaseResource(e.materialRes); e.materialRes = nullptr; }
    e.instanceMaterial.reset(); // force re-initialisation from the new resource
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
    if (forceIndex >= 0) {
        selected = std::min(forceIndex, (int)m_lodLevels.size() - 1);
    } else {
        for (int i = 0; i < (int)m_lodLevels.size(); ++i) {
            if (coverage > m_lodLevels[i].screenCoverageThreshold) { selected = i; break; }
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

void ComponentMesh::render(ID3D12GraphicsCommandList* /*cmd*/){
    // Rendering is handled by MeshRenderPass via MeshEntry lists built in
    // ModuleEditor::renderSceneWithCamera. This override is intentionally empty.
}

void ComponentMesh::onEditor(){
    if (hasLODLevels()) {
        ImGui::Text("LOD: %d / %d   Coverage: %.3f", m_currentLOD, (int)m_lodLevels.size() - 1, m_lastScreenCoverage);
    }
    if (!m_hasSkin) return;
    ImGui::Checkbox("Draw Bind Pose", &m_drawBindPose);
}

void ComponentMesh::onDrawGizmos(){
    if (!m_drawBindPose || !m_hasSkin) return;

    const int n = (int)m_skinJoints.size();
    for (int i = 0; i < n; ++i) {
        Matrix bindWorld = m_localSkin.inverseBindMatrices[i].Invert();
        Vector3 myPos = bindWorld.Translation();
        ddVec3 to = { myPos.x, myPos.y, myPos.z };

        dd::axisTriad(bindWorld.m[0], 0.f, 0.05f);

        GameObject* parentGO = m_skinJoints[i]->getParent();
        for (int p = 0; p < n; ++p) {
            if (m_skinJoints[p] == parentGO) {
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
    for (const auto& e : m_entries) { Value ev(kObjectType); ev.AddMember("meshUID", e.meshUID, a); ev.AddMember("materialUID", e.materialUID, a); entries.PushBack(ev, a); }
    doc.AddMember("Entries", entries, a);

    if (m_hasSkin) {
        Value jointNames(kArrayType);
        for (auto* j : m_skinJoints) { Value n; n.SetString(j->getName().c_str(), a); jointNames.PushBack(n, a); }
        doc.AddMember("SkinJointNames", jointNames, a);

        Value ibms(kArrayType);
        for (const auto& mat : m_localSkin.inverseBindMatrices) {
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

    if (!path.empty()) {
        int start = (doc.HasMember("MeshFileStart") && doc["MeshFileStart"].IsInt()) ? doc["MeshFileStart"].GetInt() : -1;
        int count = (doc.HasMember("MeshFileCount") && doc["MeshFileCount"].IsInt()) ? doc["MeshFileCount"].GetInt() : 0;
        if (start >= 0 && count > 0)
            loadMeshSubset(path, start, count);
        else
            loadModel(path.c_str());

        if (doc.HasMember("Entries") && doc["Entries"].IsArray()) {
            const auto& arr = doc["Entries"].GetArray();
            for (int i = 0; i < (int)arr.Size() && i < (int)m_entries.size(); ++i) {
                UID savedMat = arr[i]["materialUID"].GetUint64();
                if (savedMat != m_entries[i].materialUID) overrideMaterial(i, savedMat);
            }
        }
    } else if (doc.HasMember("Entries") && doc["Entries"].IsArray()) {
        // UID-only path: spawned from ResourceModel with no file range
        releaseEntries();
        const auto& arr = doc["Entries"].GetArray();
        for (const auto& ev : arr) {
            UID meshUID = ev.HasMember("meshUID") ? ev["meshUID"].GetUint64() : 0;
            UID matUID = ev.HasMember("materialUID") ? ev["materialUID"].GetUint64() : 0;
            if (meshUID) addMeshEntry(meshUID, matUID);
        }
        computeLocalAABB();
    }

    if (doc.HasMember("SkinJointNames") && doc.HasMember("SkinIBMs")) {
        const auto& names = doc["SkinJointNames"].GetArray();
        const auto& ibmsArr = doc["SkinIBMs"].GetArray();
        if (names.Size() == ibmsArr.Size() && names.Size() > 0) {
            // Parse the skin now, but defer binding joint GameObjects until the full scene
            // hierarchy exists (resolveDeferredSkin). During deserialization the bone nodes may
            // not be parented yet, so resolving by name here would silently fail.
            m_pendingSkin = ResourceModel::Skin{};
            m_pendingSkin.jointNodeIndices.resize(names.Size());
            m_pendingSkin.inverseBindMatrices.resize(names.Size());
            m_pendingJointNames.clear();
            m_pendingJointNames.reserve(names.Size());

            for (SizeType k = 0; k < names.Size(); ++k) {
                m_pendingSkin.jointNodeIndices[k] = static_cast<int>(k);
                const auto& mArr = ibmsArr[k].GetArray();
                float* f = reinterpret_cast<float*>(&m_pendingSkin.inverseBindMatrices[k]);
                for (int fi = 0; fi < 16; ++fi) f[fi] = mArr[fi].GetFloat();
                m_pendingJointNames.emplace_back(names[k].GetString());
            }
            m_hasPendingSkin = true;
        }
    } else if (doc.HasMember("SkinJointNames") && !doc.HasMember("SkinIBMs")) {
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
    for (const auto& name : m_pendingJointNames) {
        GameObject* jgo = findInSubtree(root, name);
        if (!jgo) { LOG("ComponentMesh: skin joint '%s' not found", name.c_str()); ok = false; break; }
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
    for (const auto& e : m_entries) {
        if (!e.meshRes || !e.meshRes->getMesh()) continue;
        const Mesh* mesh = e.meshRes->getMesh();
        if (!mesh->hasAABB()) continue;
        m_localAABBMin = Vector3::Min(m_localAABBMin, mesh->getAABBMin());
        m_localAABBMax = Vector3::Max(m_localAABBMax, mesh->getAABBMax());
        m_hasAABB = true;
    }
    if (m_proceduralModel) {
        for (const auto& mesh : m_proceduralModel->getMeshes()) {
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
    for (const auto& c : corners) {
        Vector3 wc = Vector3::Transform(c, world);
        outMin = Vector3::Min(outMin, wc);
        outMax = Vector3::Max(outMax, wc);
    }
}
