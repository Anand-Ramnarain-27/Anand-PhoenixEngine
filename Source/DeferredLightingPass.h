#pragma once
#include "DeferredLightingPipeline.h"
#include "LightCullingPass.h"
#include "MeshPipeline.h"
#include "ShaderTableDesc.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
using Microsoft::WRL::ComPtr;

class GBufferPass;
class EnvironmentSystem;
struct FrameLightData;

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
    };

    static constexpr int NUM_VIEWPORTS = 2;

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
                int viewportIndex);

private:
    bool createUploadBuffers(ID3D12Device* device);
    bool createLightSRVs();
    bool createFallbackIBL(ID3D12Device* device);

    void uploadLights(const FrameLightData& lights, int viewportIndex);
    void uploadPerFrameCB(const FrameLightData& lights, const Vector3& cameraPos,
                          const Matrix& invViewProj, uint32_t envRoughLevels,
                          uint32_t width, uint32_t height, int viewportIndex);

    DeferredLightingPipeline m_pipeline;
    LightCullingPass m_lightCulling;

    ComPtr<ID3D12Resource> m_perFrameCB[NUM_VIEWPORTS];
    void* m_perFrameMapped[NUM_VIEWPORTS] = {};

    ComPtr<ID3D12Resource> m_dirLightBuf[NUM_VIEWPORTS];
    ComPtr<ID3D12Resource> m_pointLightBuf[NUM_VIEWPORTS];
    ComPtr<ID3D12Resource> m_spotLightBuf[NUM_VIEWPORTS];
    void* m_dirLightMapped[NUM_VIEWPORTS] = {};
    void* m_pointLightMapped[NUM_VIEWPORTS] = {};
    void* m_spotLightMapped[NUM_VIEWPORTS] = {};

    ShaderTableDesc m_dirLightSRV[NUM_VIEWPORTS];
    ShaderTableDesc m_pointLightSRV[NUM_VIEWPORTS];
    ShaderTableDesc m_spotLightSRV[NUM_VIEWPORTS];

    ComPtr<ID3D12Resource> m_fallbackCube;
    ComPtr<ID3D12Resource> m_fallbackTex2D;
    ShaderTableDesc m_fallbackIrradianceSRV;
    ShaderTableDesc m_fallbackPrefilterSRV;
    ShaderTableDesc m_fallbackBRDFSRV;
};
