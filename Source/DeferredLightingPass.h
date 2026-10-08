#pragma once
// Deferred lighting: lights the G-buffer with tiled light lists, shadows and IBL.

#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class DeferredLightingPipeline {
public:
    static constexpr UINT SLOT_PERFRAME_CB = 0;
    static constexpr UINT SLOT_DIR_LIGHTS = 1;
    static constexpr UINT SLOT_POINT_LIGHTS = 2;
    static constexpr UINT SLOT_SPOT_LIGHTS = 3;
    static constexpr UINT SLOT_IRRADIANCE = 4;
    static constexpr UINT SLOT_PREFILTER = 5;
    static constexpr UINT SLOT_BRDF_LUT = 6;
    static constexpr UINT SLOT_GBUF_ALBEDO = 7;
    static constexpr UINT SLOT_GBUF_NORMAL = 8;
    static constexpr UINT SLOT_GBUF_EMISSIVE = 9;
    static constexpr UINT SLOT_GBUF_DEPTH = 10;
    static constexpr UINT SLOT_POINT_INDICES = 11;
    static constexpr UINT SLOT_SPOT_INDICES = 12;
    static constexpr UINT SLOT_SHADOW_MAP = 13;
    static constexpr UINT SLOT_SHADOW_MOMENTS = 14;
    static constexpr UINT SLOT_SPOT_SHADOW = 15;
    static constexpr UINT SLOT_POINT_SHADOW = 16;
    static constexpr UINT SLOT_GPU_VP = 17;
    static constexpr UINT SLOT_SAMPLER = 18;

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
#include "LightCullingPass.h"
#include "MeshPipeline.h"
#include "ShaderTableDesc.h"
#include "ShadowMapPass.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
using Microsoft::WRL::ComPtr;

class GBufferPass;
class EnvironmentSystem;
struct FrameLightData;

/// Full-screen pass that shades every G-buffer pixel; point and spot lights come from the tile lists
/// LightCullingPass builds.
class DeferredLightingPass {
public:
    struct CbPerFrame {
        uint32_t dirLightCount;
        uint32_t pointLightCount;
        uint32_t spotLightCount;
        uint32_t envRoughnessLevels;
        Vector3 cameraPosition;
        uint32_t framePad;
        Matrix invViewProj;
        uint32_t viewportWidth;
        uint32_t viewportHeight;
        uint32_t pad0, pad1;
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

    static constexpr int NUM_VIEWPORTS = 2;
    // Buffers the CPU writes every frame exist once per (frame in flight, viewport): the CPU records up to
    // FRAMES_IN_FLIGHT-1 frames ahead of the GPU, so with a single copy per viewport the next frame's
    // writes land in data the GPU is still reading (seen as Game view frames drawn with the wrong
    // matrices or lighting). GPU-written resources stay one per viewport - commands run in order.
    static constexpr int NUM_UPLOAD_SLOTS = NUM_VIEWPORTS * FRAMES_IN_FLIGHT;

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd,
                GBufferPass& gbufferPass,
                const FrameLightData& lights,
                const Vector3& cameraPos,
                const Matrix& view,
                const Matrix& projection,
                const Matrix& invViewProj,
                const EnvironmentSystem* env,
                uint32_t width, uint32_t height,
                int viewportIndex,
                const ShadowRenderData& shadow);

private:
    // Index into the NUM_UPLOAD_SLOTS arrays for this frame's copy of `viewportIndex`'s buffers.
    static int uploadSlot(int viewportIndex);
    bool createUploadBuffers(ID3D12Device* device);
    bool createLightSRVs();
    bool createFallbackIBL(ID3D12Device* device);

    void uploadLights(const FrameLightData& lights, int viewportIndex);
    void uploadPerFrameCB(const FrameLightData& lights, const Vector3& cameraPos,
                          const Matrix& invViewProj, uint32_t envRoughLevels,
                          uint32_t width, uint32_t height, int viewportIndex,
                          const ShadowRenderData& shadow);

    DeferredLightingPipeline m_pipeline;
    LightCullingPass m_lightCulling;

    ComPtr<ID3D12Resource> m_perFrameCB[NUM_UPLOAD_SLOTS];
    void* m_perFrameMapped[NUM_UPLOAD_SLOTS] = {};

    ComPtr<ID3D12Resource> m_dirLightBuf[NUM_UPLOAD_SLOTS];
    ComPtr<ID3D12Resource> m_pointLightBuf[NUM_UPLOAD_SLOTS];
    ComPtr<ID3D12Resource> m_spotLightBuf[NUM_UPLOAD_SLOTS];
    void* m_dirLightMapped[NUM_UPLOAD_SLOTS] = {};
    void* m_pointLightMapped[NUM_UPLOAD_SLOTS] = {};
    void* m_spotLightMapped[NUM_UPLOAD_SLOTS] = {};

    ShaderTableDesc m_dirLightSRV[NUM_UPLOAD_SLOTS];
    ShaderTableDesc m_pointLightSRV[NUM_UPLOAD_SLOTS];
    ShaderTableDesc m_spotLightSRV[NUM_UPLOAD_SLOTS];

    ComPtr<ID3D12Resource> m_fallbackCube;
    ComPtr<ID3D12Resource> m_fallbackTex2D;
    ShaderTableDesc m_fallbackIrradianceSRV;
    ShaderTableDesc m_fallbackPrefilterSRV;
    ShaderTableDesc m_fallbackBRDFSRV;
    ShaderTableDesc m_fallbackShadowSRV;
};

