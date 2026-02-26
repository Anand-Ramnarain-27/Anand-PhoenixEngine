#pragma once

#include <wrl.h>
#include <d3d12.h>
#include "EnvironmentMap.h"

using Microsoft::WRL::ComPtr;

class SkyboxRenderer
{
public:
    bool init(ID3D12Device* device, bool useMSAA = false);

    void render(
        ID3D12GraphicsCommandList* cmd,
        const EnvironmentMap& env,
        const Matrix& view,
        const Matrix& projection);

private:

    struct SkyboxCB
    {
        Matrix vp;
    };

    bool createRootSignature(ID3D12Device* device);
    bool createPipeline(ID3D12Device* device, bool useMSAA);
    bool createGeometry(ID3D12Device* device);
    bool createConstantBuffer(ID3D12Device* device);

private:

    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pso;

    ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vbView{};

    ComPtr<ID3D12Resource> constantBuffer;
    SkyboxCB* cbData = nullptr;

    UINT vertexCount = 0;
};