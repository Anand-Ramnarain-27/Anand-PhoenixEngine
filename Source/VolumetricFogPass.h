#pragma once
#include "ShaderTableDesc.h"
#include "ShadowMapPass.h"
#include "MeshPipeline.h"
#include "LightCullingPass.h"
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class RenderTexture;
class GBufferPass;
struct FrameLightData;

struct VolumetricFogSettings {
    bool enabled = false;
    uint32_t numSteps = 32;
    float extinctionCoeff = 0.15f;
    float noiseAmount = 0.5f;
    float fogIntensity = 1.0f;
    float anisotropyG = 0.3f;
    float maxOpacity = 1.0f;
    bool halfResolution = true;
    bool boundedRayLength = false;
};

class VolumetricFogPass {
public:
    static constexpr int NUM_VIEWPORTS = 2;

    static constexpr UINT SLOT_PERFRAME_CB = 0;
    static constexpr UINT SLOT_GBUF_DEPTH = 1;
    static constexpr UINT SLOT_OUTPUT = 2;
    static constexpr UINT SLOT_DIR_LIGHTS = 3;
    static constexpr UINT SLOT_POINT_LIGHTS = 4;
    static constexpr UINT SLOT_SPOT_LIGHTS = 5;
    static constexpr UINT SLOT_SHADOW_MAP = 6;
    static constexpr UINT SLOT_SHADOW_MOMENTS = 7;
    static constexpr UINT SLOT_SPOT_SHADOW = 8;
    static constexpr UINT SLOT_POINT_SHADOW = 9;
    static constexpr UINT SLOT_POINT_INDICES = 10;
    static constexpr UINT SLOT_SPOT_INDICES = 11;
    static constexpr UINT SLOT_GPU_VP = 12;
    static constexpr UINT SLOT_SAMPLER = 13;

    bool init(ID3D12Device* device);

    // Ray-marches transmittance + in-scattering into an intermediate texture (optionally at half
    // resolution, with the sample offset by Interleaved Gradient Noise to break up banding), then
    // composites it onto (input -> output) with a bilinear upsample. Returns the RenderTexture the
    // result was written to (either output, or input if disabled/invalid).
    RenderTexture* render(ID3D12GraphicsCommandList* cmd,
                          RenderTexture* input,
                          RenderTexture* output,
                          GBufferPass& gbufferPass,
                          const Vector3& cameraPos,
                          const Matrix& view,
                          const Matrix& projection,
                          const Matrix& invViewProj,
                          float elapsedTime,
                          const FrameLightData& lights,
                          const ShadowRenderData& shadow,
                          const VolumetricFogSettings& settings,
                          int viewportIndex);

private:
    struct CbCompute {
        Matrix invViewProj;
        Vector3 cameraPosition;
        float time;
        uint32_t viewportWidth;
        uint32_t viewportHeight;
        uint32_t fullViewportWidth;
        uint32_t fullViewportHeight;
        uint32_t numSteps;
        float extinctionCoeff;
        float noiseAmount;
        float fogIntensity;
        float anisotropyG;
        uint32_t frameIndex;
        uint32_t boundedRayLength;
        float framePad0;
        uint32_t dirLightCount;
        uint32_t pointLightCount;
        uint32_t spotLightCount;
        float framePad1;
        Matrix lightViewProj[ShadowMath::kMaxCascades];
        Vector4 shadowParams0;
        Vector4 shadowParams1;
        Vector4 shadowParams2;
        Vector3 shadowLightDir;
        float shadowPad;
        Matrix spotViewProj;
        Vector4 spotShadowParams;
        Vector4 spotShadowPos;
        Vector4 pointShadowParams;
        Vector4 pointShadowPos;
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
    bool createLightSRVs();
    bool createFallbackShadow(ID3D12Device* device);
    bool ensureFogTexture(uint32_t width, uint32_t height, int viewportIndex);

    void uploadLights(const FrameLightData& lights, int viewportIndex);

    ID3D12Device* m_device = nullptr;

    ComPtr<ID3D12RootSignature> m_computeRootSig;
    ComPtr<ID3D12PipelineState> m_computePSO;
    ComPtr<ID3D12RootSignature> m_compositeRootSig;
    ComPtr<ID3D12PipelineState> m_compositePSO;

    // Fog-safe tile light lists (no near-depth rejection), private to this pass so the main
    // deferred-lighting light-culling results (which DO reject near lights) stay untouched.
    LightCullingPass m_lightCulling;

    ComPtr<ID3D12Resource> m_computeCB[NUM_VIEWPORTS];
    void* m_computeMapped[NUM_VIEWPORTS] = {};
    ComPtr<ID3D12Resource> m_compositeCB[NUM_VIEWPORTS];
    void* m_compositeMapped[NUM_VIEWPORTS] = {};

    ComPtr<ID3D12Resource> m_dirLightBuf[NUM_VIEWPORTS];
    ComPtr<ID3D12Resource> m_pointLightBuf[NUM_VIEWPORTS];
    ComPtr<ID3D12Resource> m_spotLightBuf[NUM_VIEWPORTS];
    void* m_dirLightMapped[NUM_VIEWPORTS] = {};
    void* m_pointLightMapped[NUM_VIEWPORTS] = {};
    void* m_spotLightMapped[NUM_VIEWPORTS] = {};
    ShaderTableDesc m_dirLightSRV[NUM_VIEWPORTS];
    ShaderTableDesc m_pointLightSRV[NUM_VIEWPORTS];
    ShaderTableDesc m_spotLightSRV[NUM_VIEWPORTS];

    ComPtr<ID3D12Resource> m_fallbackTex2D;
    ShaderTableDesc m_fallbackArraySRV;  // Texture2DArray fallback (cascade shadow map / moments)
    ShaderTableDesc m_fallbackTex2DSRV;  // Texture2D fallback (spot shadow map)
    ShaderTableDesc m_fallbackCubeSRV;   // TextureCube fallback (point shadow map)

    ComPtr<ID3D12Resource> m_fogTex[NUM_VIEWPORTS];
    ShaderTableDesc m_fogSrv[NUM_VIEWPORTS];
    ShaderTableDesc m_fogUav[NUM_VIEWPORTS];
    uint32_t m_fogTexWidth[NUM_VIEWPORTS] = {};
    uint32_t m_fogTexHeight[NUM_VIEWPORTS] = {};

    uint32_t m_frameIndex = 0;
};
