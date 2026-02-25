#pragma once

#include <memory>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

using namespace Microsoft::WRL;
using namespace DirectX;

class SkyboxCube;

class SkyboxRenderer
{
public:
    SkyboxRenderer();
    ~SkyboxRenderer();

    bool initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, bool useMSAA);
    void render(
        ID3D12GraphicsCommandList* cmdList,
        const EnvironmentMap& environment,
        const Matrix& view,
        const Matrix& projection);

private:

    struct SkyboxCB
    {
        XMMATRIX viewProj;
    };

    bool createRootSignature(ID3D12Device* device);
    bool createPipelineState(ID3D12Device* device, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, bool useMSAA);

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    std::unique_ptr<SkyboxCube> m_cube;
};