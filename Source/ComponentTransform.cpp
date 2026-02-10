#include "Globals.h"
#include "ComponentTransform.h"
#include "GameObject.h"

ComponentTransform::ComponentTransform(GameObject* owner)
    : Component(owner)
{
}

void ComponentTransform::markDirty()
{
    dirty = true;
}

void ComponentTransform::rebuildLocal()
{
    localMatrix =
        Matrix::CreateScale(scale) *
        Matrix::CreateFromQuaternion(rotation) *
        Matrix::CreateTranslation(position);
}

void ComponentTransform::rebuildGlobal()
{
    if (auto parent = owner->getParent())
    {
        globalMatrix =
            parent->getTransform()->getGlobalMatrix() * localMatrix;
    }
    else
    {
        globalMatrix = localMatrix;
    }
}

const Matrix& ComponentTransform::getLocalMatrix()
{
    if (dirty)
    {
        rebuildLocal();
        dirty = false;
    }
    return localMatrix;
}

const Matrix& ComponentTransform::getGlobalMatrix()
{
    if (dirty)
    {
        rebuildLocal();
        rebuildGlobal();
        dirty = false;
    }
    return globalMatrix;
}
