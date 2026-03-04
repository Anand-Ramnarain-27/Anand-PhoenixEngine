#include "Globals.h"
#include "ComponentMesh.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Material.h"
#include "Application.h"
#include "ModuleGPUResources.h"
#include "ModuleAssets.h"
#include "ModuleResources.h"
#include "ModuleCamera.h"
#include "ResourceMesh.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <d3dx12.h>

using namespace rapidjson;

ComponentMesh::ComponentMesh(GameObject* owner)
    : Component(owner)
{
}

ComponentMesh::~ComponentMesh()
{
    if (m_resource)
        app->getResources()->ReleaseResource(m_resource);
}

Model* ComponentMesh::getModel() const
{
    if (m_resource) return m_resource->GetModel();
    return m_proceduralModel.get();
}

static ComPtr<ID3D12Resource> makeMaterialBuffer(const Material::Data& data)
{
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(Material::Data) + 255) & ~255);

    ComPtr<ID3D12Resource> buffer;
    app->getD3D12()->getDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));

    void* mapped = nullptr;
    buffer->Map(0, nullptr, &mapped);
    memcpy(mapped, &data, sizeof(Material::Data));
    buffer->Unmap(0, nullptr);
    buffer->SetName(L"MaterialCB");
    return buffer;
}

void ComponentMesh::rebuildMaterialBuffers()
{
    Model* model = getModel();
    if (!model) return;

    m_materialBuffers.clear();
    for (const auto& mat : model->getMaterials())
        m_materialBuffers.push_back(makeMaterialBuffer(mat->getData()));
}

bool ComponentMesh::loadModel(const char* filePath)
{
    if (m_resource)
    {
        app->getResources()->ReleaseResource(m_resource);
        m_resource = nullptr;
        m_modelUID = 0;
    }
    m_proceduralModel.reset();

    m_modelUID = app->getAssets()->findUID(filePath);
    if (m_modelUID == 0)
    {
        LOG("ComponentMesh: No UID for '%s' — import it first", filePath);
        return false;
    }

    m_resource = static_cast<ResourceMesh*>(
        app->getResources()->RequestResource(m_modelUID));

    if (!m_resource)
    {
        LOG("ComponentMesh: RequestResource failed uid=%llu", m_modelUID);
        return false;
    }

    rebuildMaterialBuffers();
    computeLocalAABB();
    return true;
}

void ComponentMesh::setModel(std::unique_ptr<Model> model)
{
    // release any resource-managed model first
    if (m_resource)
    {
        app->getResources()->ReleaseResource(m_resource);
        m_resource = nullptr;
        m_modelUID = 0;
    }

    m_proceduralModel = std::shared_ptr<Model>(std::move(model));
    rebuildMaterialBuffers();
    computeLocalAABB();
}

void ComponentMesh::render(ID3D12GraphicsCommandList* cmd)
{
    Model* model = getModel();
    if (!model) return;

    if (m_hasAABB)
    {
        Vector3 worldMin, worldMax;
        getWorldAABB(worldMin, worldMax);
        if (!app->getCamera()->isVisible(worldMin, worldMax))
            return;
    }

    Matrix world = (model->getModelMatrix() * owner->getTransform()->getGlobalMatrix()).Transpose();
    cmd->SetGraphicsRoot32BitConstants(1, 16, &world, 0);

    const auto& meshes = model->getMeshes();
    const auto& materials = model->getMaterials();

    for (size_t i = 0; i < meshes.size(); ++i)
    {
        int mi = meshes[i]->getMaterialIndex();
        if (mi >= 0 && mi < (int)materials.size())
        {
            if (mi < (int)m_materialBuffers.size())
                cmd->SetGraphicsRootConstantBufferView(
                    3, m_materialBuffers[mi]->GetGPUVirtualAddress());
            if (materials[mi]->hasTexture())
                cmd->SetGraphicsRootDescriptorTable(
                    4, materials[mi]->getTextureGPUHandle());
        }
        meshes[i]->draw(cmd);
    }
}

void ComponentMesh::onSave(std::string& outJson) const
{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();

    doc.AddMember("ModelUID", m_modelUID, a);

    std::string path = m_modelUID
        ? app->getAssets()->getPathFromUID(m_modelUID)
        : std::string();

    Value v; v.SetString(path.c_str(), a);
    doc.AddMember("ModelPath", v, a);

    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentMesh::onLoad(const std::string& jsonStr)
{
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) { LOG("ComponentMesh: JSON parse error"); return; }

    if (doc.HasMember("ModelPath") && doc["ModelPath"].IsString())
    {
        std::string path = doc["ModelPath"].GetString();
        if (!path.empty())
            loadModel(path.c_str());
    }
}

void ComponentMesh::computeLocalAABB()
{
    Model* model = getModel();
    if (!model) return;

    m_localAABBMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    m_localAABBMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    m_hasAABB = false;

    for (const auto& mesh : model->getMeshes())
    {
        if (!mesh || !mesh->hasAABB()) continue;
        m_localAABBMin = Vector3::Min(m_localAABBMin, mesh->getAABBMin());
        m_localAABBMax = Vector3::Max(m_localAABBMax, mesh->getAABBMax());
        m_hasAABB = true;
    }
}

void ComponentMesh::getWorldAABB(Vector3& outMin, Vector3& outMax) const
{
    Model* model = getModel();
    auto* t = owner->getTransform();
    Matrix world = t ? t->getGlobalMatrix() : Matrix::Identity;
    if (model) world = model->getModelMatrix() * world;

    const Vector3& mn = m_localAABBMin;
    const Vector3& mx = m_localAABBMax;

    Vector3 corners[8] =
    {
        { mn.x, mn.y, mn.z }, { mx.x, mn.y, mn.z },
        { mn.x, mx.y, mn.z }, { mx.x, mx.y, mn.z },
        { mn.x, mn.y, mx.z }, { mx.x, mn.y, mx.z },
        { mn.x, mx.y, mx.z }, { mx.x, mx.y, mx.z },
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