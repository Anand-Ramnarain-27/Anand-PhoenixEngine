#pragma once
#include "LightCullingPipeline.h"
#include "MeshRenderPass.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class GBufferPass;

// Tile-based light culling compute pass.
// Executed after the GBuffer geometry pass (depth buffer is filled).
// Outputs per-tile point-light and spot-light index lists consumed by
// the deferred lighting pass.
class LightCullingPass {
public:
    static constexpr UINT TILE_SIZE = 16;
    static constexpr UINT MAX_LIGHTS_PER_TILE= 64;

    struct CbCulling {
        uint32_t numPointLights;
        uint32_t numSpotLights;
        uint32_t viewportWidth;
        uint32_t viewportHeight;
        Matrix projection;
        Matrix view;
    };

    // cull() and the SRV getters run once per viewport per frame (Scene=0, Game=1) —
    // all per-frame mutable state below is duplicated per viewport so the second
    // viewport's upload doesn't clobber the first's data before the GPU executes
    // either pass (see DeferredLightingPass for the matching fix).
    static constexpr int NUM_VIEWPORTS = 2;

    bool init(ID3D12Device* device);

    // Dispatches the culling compute shader.
    // After this call the index-list buffers are in SRV (PIXEL_SHADER_RESOURCE) state.
    void cull(ID3D12GraphicsCommandList* cmd,
              GBufferPass& gbufferPass,
              const FrameLightData& lights,
              const Matrix& view,
              const Matrix& projection,
              uint32_t width, uint32_t height,
              int viewportIndex);

    // SRV handles for the resulting per-tile light lists (bind into the lighting PS).
    D3D12_GPU_DESCRIPTOR_HANDLE getPointListSRV(int viewportIndex) const { return m_pointListSRV[viewportIndex].getGPUHandle(0); }
    D3D12_GPU_DESCRIPTOR_HANDLE getSpotListSRV(int viewportIndex) const { return m_spotListSRV[viewportIndex] .getGPUHandle(0); }

    uint32_t getNumTilesX(uint32_t w) const { return (w + TILE_SIZE - 1) / TILE_SIZE; }
    uint32_t getNumTilesY(uint32_t h) const { return (h + TILE_SIZE - 1) / TILE_SIZE; }

private:
    bool createBuffers(ID3D12Device* device);
    bool createDescriptors();
    bool createUploadBuffers(ID3D12Device* device);
    void uploadCB(const FrameLightData& lights, const Matrix& view, const Matrix& projection,
                  uint32_t w, uint32_t h);

    LightCullingPipeline m_pipeline;

    // GPU-side UAV/SRV list buffers (DEFAULT heap, 1 buffer = all tiles × MAX_LIGHTS_PER_TILE)
    ComPtr<ID3D12Resource> m_pointListBuf[NUM_VIEWPORTS];
    ComPtr<ID3D12Resource> m_spotListBuf[NUM_VIEWPORTS];
    UINT m_allocatedTiles[NUM_VIEWPORTS] = {};

    ShaderTableDesc m_pointListUAV[NUM_VIEWPORTS]; // UAV for compute write
    ShaderTableDesc m_spotListUAV[NUM_VIEWPORTS];
    ShaderTableDesc m_pointListSRV[NUM_VIEWPORTS]; // SRV for lighting PS read
    ShaderTableDesc m_spotListSRV[NUM_VIEWPORTS];

    // Upload-heap light data used by the culling CB
    ComPtr<ID3D12Resource> m_pointLightBuf[NUM_VIEWPORTS];
    ComPtr<ID3D12Resource> m_spotLightBuf[NUM_VIEWPORTS];
    void* m_pointLightMapped[NUM_VIEWPORTS] = {};
    void* m_spotLightMapped[NUM_VIEWPORTS] = {};

    ShaderTableDesc m_pointLightSRV[NUM_VIEWPORTS]; // SRV for light array in compute shader
    ShaderTableDesc m_spotLightSRV[NUM_VIEWPORTS];

    ComPtr<ID3D12Resource> m_cb[NUM_VIEWPORTS];
    void* m_cbMapped[NUM_VIEWPORTS] = {};

    uint32_t m_lastWidth[NUM_VIEWPORTS] = {};
    uint32_t m_lastHeight[NUM_VIEWPORTS] = {};
};
