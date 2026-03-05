#include "Globals.h"
#include "ComponentMesh.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Material.h"
#include "Model.h"
#include "Application.h"
#include "ModuleGPUResources.h"
#include "ModuleAssets.h"
#include "ModuleResources.h"
#include "ModuleCamera.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "ResourceCommon.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <d3dx12.h>
#include <filesystem>

using namespace rapidjson;

ComponentMesh::ComponentMesh(GameObject* owner) : Component(owner) {}

ComponentMesh::~ComponentMesh()
{
    releaseEntries();
}

void ComponentMesh::releaseEntries()
{
    for (auto& e : m_entries)
    {
        if (e.meshRes)     app->getResources()->ReleaseResource(e.meshRes);
        if (e.materialRes) app->getResources()->ReleaseResource(e.materialRes);
        e.meshRes = nullptr;
        e.materialRes = nullptr;
    }
    m_entries.clear();
}

static ComPtr<ID3D12Resource> makeMaterialCB(const Material::Data& data)
{
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(Material::Data) + 255) & ~255);

    ComPtr<ID3D12Resource> buf;
    app->getD3D12()->getDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf));

    void* mapped = nullptr;
    buf->Map(0, nullptr, &mapped);
    memcpy(mapped, &data, sizeof(Material::Data));
    buf->Unmap(0, nullptr);
    buf->SetName(L"MaterialCB");
    return buf;
}

void ComponentMesh::rebuildEntry(MeshEntry& e)
{
    Material::Data data = {};
    if (e.materialRes && e.materialRes->getMaterial())
        data = e.materialRes->getMaterial()->getData();

    e.materialCB = makeMaterialCB(data);
}

void ComponentMesh::rebuildMaterialBuffers()
{
    for (auto& e : m_entries)
        rebuildEntry(e);

    if (m_proceduralModel)
    {
        m_proceduralMaterialBuffers.clear();
        for (const auto& mat : m_proceduralModel->getMaterials())
            m_proceduralMaterialBuffers.push_back(makeMaterialCB(mat->getData()));
    }
}

bool ComponentMesh::loadModel(const char* filePath)
{
    releaseEntries();
    m_proceduralModel.reset();
    m_proceduralMaterialBuffers.clear();
    m_modelPath.clear();
    m_modelUID = 0;

    std::string normPath = filePath;
    for (char& c : normPath) if (c == '\\') c = '/';

    namespace fs = std::filesystem;
    std::string sceneName = fs::path(normPath).stem().string();

    if (normPath.find("Library/") == 0 || normPath.find("Library\\") == 0)
    {
        std::string resolved = app->getAssets()->getAssetPathForScene(sceneName);
        if (resolved.empty())
        {
            LOG("ComponentMesh: Cannot resolve asset path for scene '%s'", sceneName.c_str());
            return false;
        }
        normPath = resolved;
        for (char& c : normPath) if (c == '\\') c = '/';
        sceneName = fs::path(normPath).stem().string();
    }

    UID sceneUID = app->getAssets()->findUID(normPath);
    if (sceneUID == 0)
    {
        std::string resolved = app->getAssets()->getAssetPathForScene(sceneName);
        if (!resolved.empty())
            sceneUID = app->getAssets()->findUID(resolved);
    }

    if (sceneUID == 0)
    {
        LOG("ComponentMesh: No UID for '%s' — import it first", normPath.c_str());
        return false;
    }

    std::string canonicalPath = app->getAssets()->getPathFromUID(sceneUID);
    if (canonicalPath.empty()) canonicalPath = normPath;

    m_modelUID = sceneUID;
    m_modelPath = canonicalPath;

    std::string meshFolder = "Library/Meshes/" + sceneName + "/";
    int meshCount = 0;
    while (app->getFileSystem()->Exists(
        (meshFolder + std::to_string(meshCount) + ".mesh").c_str()))
        ++meshCount;

    if (meshCount == 0)
    {
        LOG("ComponentMesh: No meshes found in Library for '%s'", sceneName.c_str());
        return false;
    }

    for (int i = 0; i < meshCount; ++i)
    {
        UID meshUID = app->getAssets()->findSubUID(canonicalPath, "mesh", i);
        UID matUID = app->getAssets()->findSubUID(canonicalPath, "mat", i);

        MeshEntry e;
        e.meshUID = meshUID;
        e.materialUID = matUID;

        if (meshUID)
            e.meshRes = app->getResources()->RequestMesh(meshUID);

        if (matUID)
            e.materialRes = app->getResources()->RequestMaterial(matUID);

        rebuildEntry(e);
        m_entries.push_back(std::move(e));
    }

    computeLocalAABB();
    return !m_entries.empty();
}

void ComponentMesh::setProceduralModel(std::unique_ptr<Model> model)
{
    releaseEntries();
    m_proceduralModel = std::shared_ptr<Model>(std::move(model));
    m_modelUID = 0;
    m_modelPath.clear();

    m_proceduralMaterialBuffers.clear();
    for (const auto& mat : m_proceduralModel->getMaterials())
        m_proceduralMaterialBuffers.push_back(makeMaterialCB(mat->getData()));

    computeLocalAABB();
}

void ComponentMesh::overrideMaterial(int slot, UID materialUID)
{
    if (slot < 0 || slot >= (int)m_entries.size()) return;
    MeshEntry& e = m_entries[slot];

    if (e.materialRes)
    {
        app->getResources()->ReleaseResource(e.materialRes);
        e.materialRes = nullptr;
    }

    e.materialUID = materialUID;

    if (materialUID != 0)
        e.materialRes = app->getResources()->RequestMaterial(materialUID);

    rebuildEntry(e);
}

void ComponentMesh::render(ID3D12GraphicsCommandList* cmd)
{
    if (m_proceduralModel)
    {
        if (m_hasAABB)
        {
            Vector3 wMin, wMax;
            getWorldAABB(wMin, wMax);
            if (!app->getCamera()->isVisible(wMin, wMax)) return;
        }

        Matrix world = (m_proceduralModel->getModelMatrix()
            * owner->getTransform()->getGlobalMatrix()).Transpose();
        cmd->SetGraphicsRoot32BitConstants(1, 16, &world, 0);

        const auto& meshes = m_proceduralModel->getMeshes();
        const auto& mats = m_proceduralModel->getMaterials();

        for (size_t i = 0; i < meshes.size(); ++i)
        {
            int mi = meshes[i]->getMaterialIndex();
            if (mi >= 0 && mi < (int)mats.size())
            {
                if (mi < (int)m_proceduralMaterialBuffers.size())
                    cmd->SetGraphicsRootConstantBufferView(
                        3, m_proceduralMaterialBuffers[mi]->GetGPUVirtualAddress());
                if (mats[mi]->hasTexture())
                    cmd->SetGraphicsRootDescriptorTable(4, mats[mi]->getTextureGPUHandle());
            }
            meshes[i]->draw(cmd);
        }
        return;
    }

    if (m_entries.empty()) return;

    if (m_hasAABB)
    {
        Vector3 wMin, wMax;
        getWorldAABB(wMin, wMax);
        if (!app->getCamera()->isVisible(wMin, wMax)) return;
    }

    Matrix world = owner->getTransform()->getGlobalMatrix().Transpose();
    cmd->SetGraphicsRoot32BitConstants(1, 16, &world, 0);

    for (const auto& e : m_entries)
    {
        if (!e.meshRes || !e.meshRes->getMesh()) continue;

        if (e.materialCB)
            cmd->SetGraphicsRootConstantBufferView(3, e.materialCB->GetGPUVirtualAddress());

        if (e.materialRes && e.materialRes->getMaterial()
            && e.materialRes->getMaterial()->hasTexture())
            cmd->SetGraphicsRootDescriptorTable(
                4, e.materialRes->getMaterial()->getTextureGPUHandle());

        e.meshRes->getMesh()->draw(cmd);
    }
}

void ComponentMesh::onSave(std::string& outJson) const
{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("ModelUID", m_modelUID, a);

    Value pathVal; pathVal.SetString(m_modelPath.c_str(), a);
    doc.AddMember("ModelPath", pathVal, a);

    Value entries(kArrayType);
    for (const auto& e : m_entries)
    {
        Value ev(kObjectType);
        ev.AddMember("meshUID", e.meshUID, a);
        ev.AddMember("materialUID", e.materialUID, a);
        entries.PushBack(ev, a);
    }
    doc.AddMember("Entries", entries, a);

    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentMesh::onLoad(const std::string& jsonStr)
{
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;

    if (doc.HasMember("ModelPath") && doc["ModelPath"].IsString())
    {
        std::string path = doc["ModelPath"].GetString();
        if (!path.empty())
            loadModel(path.c_str());
    }

    if (doc.HasMember("Entries") && doc["Entries"].IsArray())
    {
        const auto& arr = doc["Entries"].GetArray();
        for (int i = 0; i < (int)arr.Size() && i < (int)m_entries.size(); ++i)
        {
            UID savedMat = arr[i]["materialUID"].GetUint64();
            if (savedMat != m_entries[i].materialUID)
                overrideMaterial(i, savedMat);
        }
    }
}

void ComponentMesh::computeLocalAABB()
{
    m_localAABBMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    m_localAABBMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    m_hasAABB = false;

    for (const auto& e : m_entries)
    {
        if (!e.meshRes || !e.meshRes->getMesh()) continue;
        Mesh* mesh = e.meshRes->getMesh();
        if (!mesh->hasAABB()) continue;
        m_localAABBMin = Vector3::Min(m_localAABBMin, mesh->getAABBMin());
        m_localAABBMax = Vector3::Max(m_localAABBMax, mesh->getAABBMax());
        m_hasAABB = true;
    }

    if (m_proceduralModel)
    {
        for (const auto& mesh : m_proceduralModel->getMeshes())
        {
            if (!mesh || !mesh->hasAABB()) continue;
            m_localAABBMin = Vector3::Min(m_localAABBMin, mesh->getAABBMin());
            m_localAABBMax = Vector3::Max(m_localAABBMax, mesh->getAABBMax());
            m_hasAABB = true;
        }
    }
}

void ComponentMesh::getWorldAABB(Vector3& outMin, Vector3& outMax) const
{
    auto* t = owner->getTransform();
    Matrix world = t ? t->getGlobalMatrix() : Matrix::Identity;

    const Vector3& mn = m_localAABBMin;
    const Vector3& mx = m_localAABBMax;

    Vector3 corners[8] =
    {
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},
        {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},
    };

    outMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    outMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (auto& c : corners)
    {
        Vector3 wc = Vector3::Transform(c, world);
        outMin = Vector3::Min(outMin, wc);
        outMax = Vector3::Max(outMax, wc);
    }
}