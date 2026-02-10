#pragma once

#include "IScene.h"
#include <wrl.h>

class GameObject;

class RenderPipelineTestScene final : public IScene
{
public:
    RenderPipelineTestScene();
    ~RenderPipelineTestScene() override;

    const char* getName() const override;
    const char* getDescription() const override;

    bool initialize(ID3D12Device* device) override;
    void update(float deltaTime) override;
    void render(
        ID3D12GraphicsCommandList* cmd,
        const ModuleCamera& camera,
        uint32_t viewportWidth,
        uint32_t viewportHeight
    ) override;
    void shutdown() override;

private:
    std::unique_ptr<GameObject> parent;
    std::unique_ptr<GameObject> child;
    float m_time = 0.0f;
};
