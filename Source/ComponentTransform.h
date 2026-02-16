#pragma once

#include "Component.h"
#include "ModuleD3D12.h"

class ComponentTransform final : public Component
{
public:
    explicit ComponentTransform(GameObject* owner);

    Vector3 position{ 0,0,0 };
    Vector3 scale{ 1,1,1 };
    Quaternion rotation = Quaternion::Identity;

    const Matrix& getLocalMatrix();
    const Matrix& getGlobalMatrix();

    void markDirty();

    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Transform; }

private:
    void rebuildLocal();
    void rebuildGlobal();

    Matrix localMatrix = Matrix::Identity;
    Matrix globalMatrix = Matrix::Identity;

    bool dirty = true;
};