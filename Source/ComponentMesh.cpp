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
#include <d3dx12.h>

using namespace rapidjson;

ComponentMesh::ComponentMesh(GameObject* owner)
    : Component(owner)
{
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
    if (!m_model) return;
    m_materialBuffers.clear();
    for (const auto& mat : m_model->getMaterials())
        m_materialBuffers.push_back(makeMaterialBuffer(mat->getData()));
}

bool ComponentMesh::loadModel(const char* filePath)
{
    m_model = app->getResourceCache()->getOrLoadModel(filePath);
    if (!m_model) { LOG("ComponentMesh: Failed to load model: %s", filePath); return false; }
    m_modelFilePath = filePath;
    rebuildMaterialBuffers();
    return true;
}

void ComponentMesh::setModel(std::unique_ptr<Model> model)
{
    m_model = std::shared_ptr<Model>(std::move(model));
    m_modelFilePath.clear();
    rebuildMaterialBuffers();
}

void ComponentMesh::render(ID3D12GraphicsCommandList* cmd)
{
    if (!m_model) return;

    Matrix world = (m_model->getModelMatrix() * owner->getTransform()->getGlobalMatrix()).Transpose();
    cmd->SetGraphicsRoot32BitConstants(1, 16, &world, 0);

    const auto& meshes = m_model->getMeshes();
    const auto& materials = m_model->getMaterials();

    for (size_t i = 0; i < meshes.size(); ++i)
    {
        int mi = meshes[i]->getMaterialIndex();
        if (mi >= 0 && mi < (int)materials.size())
        {
            if (mi < (int)m_materialBuffers.size())
                cmd->SetGraphicsRootConstantBufferView(3, m_materialBuffers[mi]->GetGPUVirtualAddress());
            if (materials[mi]->hasTexture())
                cmd->SetGraphicsRootDescriptorTable(4, materials[mi]->getTextureGPUHandle());
        }
        meshes[i]->draw(cmd);
    }
}

void ComponentMesh::onSave(std::string& outJson) const
{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("HasModel", m_model != nullptr, a);
    if (m_model && !m_modelFilePath.empty())
    {
        Value path; path.SetString(m_modelFilePath.c_str(), a);
        doc.AddMember("ModelPath", path, a);
    }
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentMesh::onLoad(const std::string& jsonStr)
{
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) { LOG("ComponentMesh: JSON parse error"); return; }
    if (doc.HasMember("HasModel") && doc["HasModel"].GetBool() && doc.HasMember("ModelPath"))
        loadModel(doc["ModelPath"].GetString());
}