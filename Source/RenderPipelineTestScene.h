#pragma once

#include "IScene.h"

#include <wrl.h>

using Microsoft::WRL::ComPtr;

class RenderPipelineTestScene final : public IScene
{
public:
    RenderPipelineTestScene() = default;
    ~RenderPipelineTestScene() override = default;

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
    float m_time = 0.0f;
};
