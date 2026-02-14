#pragma once

#include "IScene.h"
#include <memory>

class GameObject;
class ModuleScene;

class RenderPipelineTestScene : public IScene
{
public:
    RenderPipelineTestScene();
    ~RenderPipelineTestScene() override;

    const char* getName() const override;
    const char* getDescription() const override;

    bool initialize(ID3D12Device* device) override;
    void update(float deltaTime) override;
    void render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t viewportWidth, uint32_t viewportHeight) override;
    void shutdown() override;

    ModuleScene* getModuleScene() override { return scene.get(); }

private:
    std::unique_ptr<ModuleScene> scene;

    GameObject* parent = nullptr;
    GameObject* child = nullptr;
    GameObject* duckObject = nullptr; 
    GameObject* secondModel = nullptr;

    float m_time = 0.0f;

    // REMOVE THIS: std::unique_ptr<Model> testModel;
};