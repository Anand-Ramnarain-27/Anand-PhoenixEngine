#include "Globals.h"
#include "ComponentMesh.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Model.h"
#include "Material.h"
#include "Application.h"
#include "ModuleResources.h"

ComponentMesh::ComponentMesh(GameObject* owner)
    : Component(owner)
{
}

ComponentMesh::~ComponentMesh() = default;

bool ComponentMesh::loadModel(const char* filePath)
{
    m_model = std::make_unique<Model>();

    if (!m_model->load(filePath))
    {
        return false;
    }

    // Create constant buffers for materials
    ModuleResources* resources = app->getResources();
    const auto& materials = m_model->getMaterials();

    m_materialBuffers.clear();

    for (const auto& material : materials)
    {
        const auto& materialData = material->getData();

        UINT bufferSize = sizeof(Material::Data);
        UINT alignedSize = (bufferSize + 255) & ~255;  // Align to 256 bytes

        ComPtr<ID3D12Resource> buffer = resources->createDefaultBuffer(
            &materialData,
            alignedSize,
            "MaterialCB"
        );

        m_materialBuffers.push_back(buffer);
    }

    return true;
}

void ComponentMesh::render(ID3D12GraphicsCommandList* cmd)
{
    if (!m_model)
        return;

    // Get the world transform from the owning GameObject
    Matrix worldMatrix = owner->getTransform()->getGlobalMatrix();

    // Apply model-specific transform
    worldMatrix = m_model->getModelMatrix() * worldMatrix;

    // Bind world matrix
    Matrix world = worldMatrix.Transpose();
    cmd->SetGraphicsRoot32BitConstants(1, 16, &world, 0);

    // Draw each mesh with its material
    const auto& meshes = m_model->getMeshes();
    const auto& materials = m_model->getMaterials();

    for (size_t i = 0; i < meshes.size(); ++i)
    {
        const auto& mesh = meshes[i];
        int materialIndex = mesh->getMaterialIndex();

        if (materialIndex >= 0 && materialIndex < (int)materials.size())
        {
            const auto& material = materials[materialIndex];

            // Bind material constant buffer
            if (materialIndex < (int)m_materialBuffers.size())
            {
                cmd->SetGraphicsRootConstantBufferView(2,
                    m_materialBuffers[materialIndex]->GetGPUVirtualAddress());
            }

            // Bind texture if available
            if (material->hasTexture())
            {
                cmd->SetGraphicsRootDescriptorTable(3, material->getTextureGPUHandle());
            }
        }

        mesh->draw(cmd);
    }
}