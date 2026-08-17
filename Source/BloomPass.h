#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class RenderTexture;
struct EditorViewport;

class BloomPass {
public:
    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd, RenderTexture* hdrSource, EditorViewport& viewport, float threshold);

private:
    bool createRootSignature(ID3D12Device* device);
    ComPtr<ID3D12PipelineState> createPSO(ID3D12Device* device, const wchar_t* psFile, bool additiveBlend);
    void drawFullscreen(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso,
                        RenderTexture* input, RenderTexture* output, float threshold);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_extractPSO;
    ComPtr<ID3D12PipelineState> m_downsamplePSO;
    ComPtr<ID3D12PipelineState> m_upsamplePSO;
};
