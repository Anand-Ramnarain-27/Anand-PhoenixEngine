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
        Vector3  cameraPosition;
        uint32_t framePad;
        Matrix   invViewProj;
        uint32_t viewportWidth;
        uint32_t viewportHeight;
        uint32_t pad0, pad1;
    };

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd,
                GBufferPass& gbufferPass,
                const FrameLightData& lights,
                const Vector3& cameraPos,
                const Matrix& view,
                const Matrix& projection,
                const Matrix& invViewProj,
                const EnvironmentSystem* env,
                uint32_t width, uint32_t height);

private:
    bool createUploadBuffers(ID3D12Device* device);
    bool createLightSRVs();
    bool createFallbackIBL(ID3D12Device* device);

    void uploadLights(const FrameLightData& lights);
    void uploadPerFrameCB(const FrameLightData& lights, const Vector3& cameraPos,
                          const Matrix& invViewProj, uint32_t envRoughLevels,
                          uint32_t width, uint32_t height);

    DeferredLightingPipeline m_pipeline;
    LightCullingPass m_lightCulling;

    ComPtr<ID3D12Resource> m_perFrameCB;
    void* m_perFrameMapped = nullptr;

    ComPtr<ID3D12Resource> m_dirLightBuf;
    ComPtr<ID3D12Resource> m_pointLightBuf;
    ComPtr<ID3D12Resource> m_spotLightBuf;
    void* m_dirLightMapped = nullptr;
    void* m_pointLightMapped = nullptr;
    void* m_spotLightMapped = nullptr;

    ShaderTableDesc m_dirLightSRV;
    ShaderTableDesc m_pointLightSRV;
    ShaderTableDesc m_spotLightSRV;

    ComPtr<ID3D12Resource> m_fallbackCube;
    ComPtr<ID3D12Resource> m_fallbackTex2D;
    ShaderTableDesc m_fallbackIrradianceSRV;
    ShaderTableDesc m_fallbackPrefilterSRV;
    ShaderTableDesc m_fallbackBRDFSRV;
};
