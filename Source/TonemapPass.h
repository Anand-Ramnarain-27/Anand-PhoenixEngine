#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class RenderTexture;

class TonemapPass {
public:
    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd, RenderTexture* hdrSource, RenderTexture* ldrDest);

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
