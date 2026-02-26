#include "Globals.h"
#include "EnvironmentSystem.h"

bool EnvironmentSystem::init(
    ID3D12Device* device,
    DXGI_FORMAT,
    DXGI_FORMAT,
    bool useMSAA)
{
    return renderer.init(device, useMSAA);
}

void EnvironmentSystem::load(const std::string& file)
{
    environment = generator.loadCubemap(file);
}

void EnvironmentSystem::render(
    ID3D12GraphicsCommandList* cmd,
    const Matrix& view,
    const Matrix& projection)
{
    if (environment && environment->isValid())
        renderer.render(cmd, *environment, view, projection);
}