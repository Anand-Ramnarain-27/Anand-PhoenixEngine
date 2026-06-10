#pragma once
#include "LightCullingPipeline.h"
#include "MeshRenderPass.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
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

    bool init(ID3D12Device* device);

    // Dispatches the culling compute shader.
    // After this call the index-list buffers are in SRV (PIXEL_SHADER_RESOURCE) state.
    void cull(ID3D12GraphicsCommandList* cmd,
              GBufferPass& gbufferPass,
              const FrameLightData& lights,
              const Matrix& view,
              const Matrix& projection,
              uint32_t width, uint32_t height);

    // SRV handles for the resulting per-tile light lists (bind into the lighting PS).
    D3D12_GPU_DESCRIPTOR_HANDLE getPointListSRV() const { return m_pointListSRV.getGPUHandle(0); }
    D3D12_GPU_DESCRIPTOR_HANDLE getSpotListSRV() const { return m_spotListSRV .getGPUHandle(0); }

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
    ComPtr<ID3D12Resource> m_pointListBuf;
    ComPtr<ID3D12Resource> m_spotListBuf;
    UINT m_allocatedTiles = 0;

    // Buffers retired by a grow resize. SceneView and GameView call cull() with
    // different viewport sizes within the SAME (not-yet-executed) command list,
    // so an old buffer may still be referenced by barriers/descriptors recorded
    // earlier this frame. Keep them alive here until the next cull() call, where
    // we flush the GPU (waiting for last frame's command list to fully complete)
    // before releasing them.
    std::vector<ComPtr<ID3D12Resource>> m_retiredBuffers;

    ShaderTableDesc m_pointListUAV; // UAV for compute write
    ShaderTableDesc m_spotListUAV;
    ShaderTableDesc m_pointListSRV; // SRV for lighting PS read
    ShaderTableDesc m_spotListSRV;

    // Upload-heap light data used by the culling CB
    ComPtr<ID3D12Resource> m_pointLightBuf;
    ComPtr<ID3D12Resource> m_spotLightBuf;
    void* m_pointLightMapped = nullptr;
    void* m_spotLightMapped = nullptr;

    ShaderTableDesc m_pointLightSRV; // SRV for light array in compute shader
    ShaderTableDesc m_spotLightSRV;

    ComPtr<ID3D12Resource> m_cb;
    void* m_cbMapped = nullptr;

    uint32_t m_lastWidth = 0;
    uint32_t m_lastHeight = 0;

    // True for the dispatch immediately following a (re)allocation — newly created
    // buffers start in UNORDERED_ACCESS state, so the PSR->UAV transition must be skipped once.
    bool m_tileBuffersFreshlyAllocated = false;
};
