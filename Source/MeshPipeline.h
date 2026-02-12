#pragma once
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class MeshPipeline
{
public:
    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

private:
    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3D12PipelineState> pso;
};
