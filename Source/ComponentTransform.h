#pragma once
// Position, rotation and scale of a GameObject, with cached local and world matrices.

#include "Component.h"
#include "ModuleD3D12.h"

/// Every GameObject has exactly one. Edit position / rotation / scale directly, then call markDirty() so the
/// cached matrices (and the children's) rebuild. Layout mirrored in GameScript's EngineDecls.
class ComponentTransform final : public Component {
public:
    explicit ComponentTransform(GameObject* owner);

    Vector3 position = { 0, 0, 0 };
    Vector3 scale = { 1, 1, 1 };
    Quaternion rotation = Quaternion::Identity;

    const Matrix& getLocalMatrix();
    /// World matrix (parent chain applied); rebuilt lazily after markDirty().
    const Matrix& getGlobalMatrix();
    /// Invalidates this transform and every descendant's world matrix.
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
