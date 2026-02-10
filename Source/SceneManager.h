#pragma once

#include <memory>
#include <d3d12.h>

class IScene;
class ModuleCamera;

class SceneManager
{
public:
    SceneManager() = default;
    ~SceneManager();

    // ------------------------------------------------------------
    // Scene control
    // ------------------------------------------------------------

    void setScene(std::unique_ptr<IScene> scene, ID3D12Device* device);
    void clearScene();

    IScene* getActiveScene() const { return activeScene.get(); }

    // ------------------------------------------------------------
    // Frame hooks
    // ------------------------------------------------------------

    void update(float deltaTime);
    void render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t width, uint32_t height);

    void onViewportResized(uint32_t width, uint32_t height);
private:
    std::unique_ptr<IScene> activeScene;
};
