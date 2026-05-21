#pragma once
#include "Component.h"
#include "ModuleD3D12.h"

class ComponentTransform final : public Component {
public:
    explicit ComponentTransform(GameObject* owner);

    Vector3 position = { 0, 0, 0 };
    Vector3 scale = { 1, 1, 1 };
    Quaternion rotation = Quaternion::Identity;

    const Matrix& getGlobalMatrix();
    void markDirty();
    // Called top-down each frame before rendering to eagerly propagate world matrices.
    // parentWorld is the already-recomputed parent globalMatrix.
    void updateWorldMatrix(const Matrix& parentWorld);
    bool isDirty() const { return dirty; }
    // Called by ComponentAnimation each frame to push animated local+world into
    // the cached matrices without going through the dirty-flag path.
    void setWorldMatrixDirect(const Matrix& local, const Matrix& world);

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