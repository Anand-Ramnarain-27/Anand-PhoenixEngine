#pragma once
#include <memory>

class GameObject;

class SceneGraph
{
public:
    SceneGraph();
    ~SceneGraph();

    GameObject* getRoot() const { return root.get(); }

    void update(float dt);
    void render(
        ID3D12GraphicsCommandList* cmd,
        const ModuleCamera& camera
    );

private:
    std::unique_ptr<GameObject> root;
};
