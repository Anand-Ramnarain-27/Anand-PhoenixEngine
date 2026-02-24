#include "Globals.h"
#include "EnvironmentSystem.h"

bool EnvironmentSystem::init(ID3D12Device* device)
{
    return renderer.init(device);
}

void EnvironmentSystem::load(const std::string& file)
{
    environment = generator.LoadCubemap(file);
}

void EnvironmentSystem::render(
    ID3D12GraphicsCommandList* cmd,
    const Matrix& view,
    const Matrix& projection)
{
    if (environment)
        renderer.render(cmd, *environment, view, projection);
}