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

    void render(ID3D12GraphicsCommandList* cmd,
        const Matrix& view,
        const Matrix& projection);

    const EnvironmentMap* getEnvironmentMap() const { return m_environment.get(); }

    bool hasIBL() const { return m_environment && m_environment->hasIBL(); }

private:
    EnvironmentGenerator              m_generator;
    SkyboxRenderer                    m_renderer;
    std::unique_ptr<EnvironmentMap>   m_environment;
};