#pragma once

#include "Component.h"
#include "ModuleD3D12.h"

class ComponentTransform final : public Component
{
public:
    explicit ComponentTransform(GameObject* owner);

    // TRS (lecture concept)
    Vector3 position{ 0,0,0 };
    Vector3 scale{ 1,1,1 };
    Quaternion rotation = Quaternion::Identity;

    const Matrix& getLocalMatrix();
    const Matrix& getGlobalMatrix();

    void markDirty();

private:
    void rebuildLocal();
    void rebuildGlobal();

    Matrix localMatrix = Matrix::Identity;
    Matrix globalMatrix = Matrix::Identity;

    bool dirty = true;
};
