#pragma once
#include <memory>
#include "EnvironmentGenerator.h"
#include "SkyboxRenderer.h"

class EnvironmentSystem
{
public:
    bool init(ID3D12Device* device,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        bool useMSAA);

    void load(const std::string& file);

    void render(
        ID3D12GraphicsCommandList* cmd,
        const Matrix& view,
        const Matrix& projection);

private:
    EnvironmentGenerator generator;
    SkyboxRenderer renderer;

    std::unique_ptr<EnvironmentMap> environment;
};