#pragma once
#include "ShaderTableDesc.h"
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class RenderTexture;
class GBufferPass;

struct VolumetricFogSettings {
    bool enabled = false;
    uint32_t numSteps = 32;
    float extinctionCoeff = 0.15f;
    float noiseAmount = 0.5f;
    float fogIntensity = 1.0f;
    float maxOpacity = 1.0f;
};

class VolumetricFogPass {
public:
    static constexpr int NUM_VIEWPORTS = 2;

    bool init(ID3D12Device* device);

    // Ray-marches transmittance into an intermediate texture, then composites it onto (input -> output).
    // Returns the RenderTexture the result was written to (either output, or input if disabled/invalid).
    RenderTexture* render(ID3D12GraphicsCommandList* cmd,
                          RenderTexture* input,
                          RenderTexture* output,
                          GBufferPass& gbufferPass,
                          const Vector3& cameraPos,
                          const Matrix& invViewProj,
                          float elapsedTime,
                          const VolumetricFogSettings& settings,
                          int viewportIndex);

private:
    struct CbCompute {
        Matrix invViewProj;
        Vector3 cameraPosition;
        float time;
        uint32_t viewportWidth;
        uint32_t viewportHeight;
        uint32_t numSteps;
        float extinctionCoeff;
        float noiseAmount;
        float fogIntensity;
        float framePad0;
        float framePad1;
    };

    struct CbComposite {
        float maxOpacity;
        float pad0, pad1, pad2;
    };

    bool createComputeRootSignature(ID3D12Device* device);
    bool createComputePSO(ID3D12Device* device);
    bool createCompositeRootSignature(ID3D12Device* device);
    bool createCompositePSO(ID3D12Device* device);
    bool createUploadBuffers(ID3D12Device* device);
    bool ensureFogTexture(uint32_t width, uint32_t height, int viewportIndex);

    ID3D12Device* m_device = nullptr;

    ComPtr<ID3D12RootSignature> m_computeRootSig;
    ComPtr<ID3D12PipelineState> m_computePSO;
    ComPtr<ID3D12RootSignature> m_compositeRootSig;
    ComPtr<ID3D12PipelineState> m_compositePSO;

    ComPtr<ID3D12Resource> m_computeCB[NUM_VIEWPORTS];
    void* m_computeMapped[NUM_VIEWPORTS] = {};
    ComPtr<ID3D12Resource> m_compositeCB[NUM_VIEWPORTS];
    void* m_compositeMapped[NUM_VIEWPORTS] = {};

    ComPtr<ID3D12Resource> m_fogTex[NUM_VIEWPORTS];
    ShaderTableDesc m_fogSrv[NUM_VIEWPORTS];
    ShaderTableDesc m_fogUav[NUM_VIEWPORTS];
    uint32_t m_fogTexWidth[NUM_VIEWPORTS] = {};
    uint32_t m_fogTexHeight[NUM_VIEWPORTS] = {};
};
