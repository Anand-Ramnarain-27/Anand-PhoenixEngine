#include "Globals.h"
#include "ComponentMesh.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Model.h"
#include "Material.h"
#include "Application.h"
#include "ModuleResources.h"

#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ComponentMesh::ComponentMesh(GameObject* owner)
    : Component(owner)
{
}

ComponentMesh::~ComponentMesh() = default;

bool ComponentMesh::loadModel(const char* filePath)
{
    ResourceCache* cache = app->getResourceCache();
    m_model = cache->getOrLoadModel(filePath);

    if (!m_model)
    {
        LOG("Failed to load model: %s", filePath);
        return false;
    }

    m_modelFilePath = filePath;

    ModuleResources* resources = app->getResources();
    const auto& materials = m_model->getMaterials();

    m_materialBuffers.clear();

    for (const auto& material : materials)
    {
        const auto& materialData = material->getData();

        UINT bufferSize = sizeof(Material::Data);
        UINT alignedSize = (bufferSize + 255) & ~255;

        ComPtr<ID3D12Resource> buffer;
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(Material::Data) + 255) & ~255);

        app->getD3D12()->getDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&buffer));

        void* mapped = nullptr;
        buffer->Map(0, nullptr, &mapped);
        memcpy(mapped, &materialData, sizeof(Material::Data));
        buffer->Unmap(0, nullptr);
        buffer->SetName(L"MaterialCB");

        m_materialBuffers.push_back(buffer);
    }

    return true;
}

void ComponentMesh::render(ID3D12GraphicsCommandList* cmd)
{
    if (!m_model)
        return;

    Matrix worldMatrix = owner->getTransform()->getGlobalMatrix();

    worldMatrix = m_model->getModelMatrix() * worldMatrix;

    Matrix world = worldMatrix.Transpose();
    cmd->SetGraphicsRoot32BitConstants(1, 16, &world, 0);

    const auto& meshes = m_model->getMeshes();
    const auto& materials = m_model->getMaterials();

    for (size_t i = 0; i < meshes.size(); ++i)
    {
        const auto& mesh = meshes[i];
        int materialIndex = mesh->getMaterialIndex();

        if (materialIndex >= 0 && materialIndex < (int)materials.size())
        {
            const auto& material = materials[materialIndex];

            if (materialIndex < (int)m_materialBuffers.size())
            {
                cmd->SetGraphicsRootConstantBufferView(3, m_materialBuffers[materialIndex]->GetGPUVirtualAddress());
            }

            if (material->hasTexture())
            {
                cmd->SetGraphicsRootDescriptorTable(4, material->getTextureGPUHandle());
            }
        }
        mesh->draw(cmd);
    }
}

void ComponentMesh::onSave(std::string& outJson) const
{
    Document doc;
    doc.SetObject();
    Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("HasModel", (m_model != nullptr), allocator);

    if (m_model && !m_modelFilePath.empty()) {
        Value pathVal;
        pathVal.SetString(m_modelFilePath.c_str(), allocator);
        doc.AddMember("ModelPath", pathVal, allocator);
    }

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    outJson = buffer.GetString();
}

void ComponentMesh::onLoad(const std::string& jsonStr)
{
    Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError()) {
        LOG("ComponentMesh: JSON parse error");
        return;
    }

    if (doc["HasModel"].GetBool() && doc.HasMember("ModelPath")) {
        std::string modelPath = doc["ModelPath"].GetString();
        LOG("ComponentMesh: Loading model from: %s", modelPath.c_str());
        loadModel(modelPath.c_str());
    }
}

void ComponentMesh::rebuildMaterialBuffers()
{
    if (!m_model) return;

    const auto& materials = m_model->getMaterials();
    m_materialBuffers.clear();

    for (const auto& material : materials)
    {
        const auto& materialData = material->getData();

        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(
            (sizeof(Material::Data) + 255) & ~255);

        ComPtr<ID3D12Resource> buffer;
        app->getD3D12()->getDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&buffer));

        void* mapped = nullptr;
        buffer->Map(0, nullptr, &mapped);
        memcpy(mapped, &materialData, sizeof(Material::Data));
        buffer->Unmap(0, nullptr);
        buffer->SetName(L"MaterialCB");

        m_materialBuffers.push_back(buffer);
    }
}