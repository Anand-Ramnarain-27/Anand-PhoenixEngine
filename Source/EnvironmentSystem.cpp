#include "Globals.h"
#include "EnvironmentSystem.h"

bool EnvironmentSystem::init(
    ID3D12Device* device,
    DXGI_FORMAT   rtvFormat,
    DXGI_FORMAT   dsvFormat,
    bool          useMSAA)
{
    return m_renderer.init(device, useMSAA);
}

void EnvironmentSystem::load(const std::string& file)
{
    m_environment = m_generator.loadCubemap(file);
}

void EnvironmentSystem::render(
    ID3D12GraphicsCommandList* cmd,
    const Matrix& view,
    const Matrix& projection)
{
    if (m_environment && m_environment->isValid())
        m_renderer.render(cmd, *m_environment, view, projection);
}