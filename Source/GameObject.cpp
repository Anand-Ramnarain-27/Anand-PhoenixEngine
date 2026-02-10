#include "Globals.h"
#include "GameObject.h"
#include "Component.h"
#include "ComponentTransform.h"

void GameObject::update(float dt)
{
    for (auto& c : components)
        c->update(dt);

    for (auto& child : children)
        child->update(dt);
}

void GameObject::render(
    ID3D12GraphicsCommandList* cmd,
    const ModuleCamera& camera,
    const Matrix& parentTransform)
{
    // Find transform component
    Matrix local = Matrix::Identity;
    for (auto& c : components)
    {
        if (auto* t = dynamic_cast<ComponentTransform*>(c.get()))
            local = t->getLocalMatrix();
    }

    worldTransform = parentTransform * local;

    for (auto& c : components)
        c->render(cmd, camera, worldTransform);

    for (auto& child : children)
        child->render(cmd, camera, worldTransform);
}
