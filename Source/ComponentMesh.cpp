#include "Globals.h"
#include "ComponentMesh.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Model.h"

ComponentMesh::ComponentMesh(GameObject* owner)
    : Component(owner)
{
}

ComponentMesh::~ComponentMesh() = default;

bool ComponentMesh::loadModel(const char* filePath)
{
    m_model = std::make_unique<Model>();
    return m_model->load(filePath);
}

void ComponentMesh::render(ID3D12GraphicsCommandList* cmd)
{
    if (!m_model)
        return;

    // Get the world transform from the owning GameObject
    Matrix worldMatrix = owner->getTransform()->getGlobalMatrix();

    // Apply any model-specific transform (like scale)
    worldMatrix = m_model->getModelMatrix() * worldMatrix;

    // Draw with the combined transform
    m_model->draw(cmd, worldMatrix);
}