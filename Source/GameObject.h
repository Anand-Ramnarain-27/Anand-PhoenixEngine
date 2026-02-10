#pragma once
#include "ModuleD3D12.h"

class Component;
class ModuleCamera;

class GameObject
{
public:
    GameObject(const std::string& name);
    ~GameObject();

    // Hierarchy
    void setParent(GameObject* parent);
    void addChild(std::unique_ptr<GameObject> child);

    // Components
    template<typename T, typename... Args>
    T* addComponent(Args&&... args);

    // Engine
    void update(float dt);
    void render(
        ID3D12GraphicsCommandList* cmd,
        const ModuleCamera& camera,
        const Matrix& parentTransform
    );

    const Matrix& getWorldTransform() const { return worldTransform; }

private:
    std::string name;
    GameObject* parent = nullptr;
    std::vector<std::unique_ptr<GameObject>> children;
    std::vector<std::unique_ptr<Component>> components;

    Matrix localTransform = Matrix::Identity;
    Matrix worldTransform = Matrix::Identity;
};
