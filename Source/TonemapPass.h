#pragma once
#include "ShaderTableDesc.h"
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class RenderTexture;
class ColorLUT;

struct TonemapParams {
    float exposure = 0.0f;
    float bloomIntensity = 0.6f;
    bool lutEnabled = false;
};

class TonemapPass {
public:
    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd, RenderTexture* hdrSource, RenderTexture* ldrDest,
                RenderTexture* bloomSource, ColorLUT* lut, const TonemapParams& params);

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);
    bool createFallbackTextures(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;

    ComPtr<ID3D12Resource> m_fallbackBloomTex;
    ShaderTableDesc m_fallbackBloomSRV;

    ComPtr<ID3D12Resource> m_fallbackLutTex;
    ShaderTableDesc m_fallbackLutSRV;
};
