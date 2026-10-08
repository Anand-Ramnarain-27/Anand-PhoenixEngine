#pragma once
// Compute pass that bins point and spot lights into screen tiles.

#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class LightCullingPipeline {
public:
    static constexpr UINT SLOT_CB = 0;
    static constexpr UINT SLOT_DEPTH = 1;
    static constexpr UINT SLOT_POINT_LIGHTS= 2;
    static constexpr UINT SLOT_SPOT_LIGHTS = 3;
    static constexpr UINT SLOT_POINT_UAV = 4;
    static constexpr UINT SLOT_SPOT_UAV = 5;

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
#include "ForwardMeshPass.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class GBufferPass;

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
        uint32_t ignoreNearDepth;
        float cullPad0, cullPad1, cullPad2;
    };

    static constexpr int NUM_VIEWPORTS = 2;
    // Buffers the CPU writes every frame exist once per (frame in flight, viewport): the CPU records up to
    // FRAMES_IN_FLIGHT-1 frames ahead of the GPU, so with a single copy per viewport the next frame's
    // writes land in data the GPU is still reading (seen as Game view frames drawn with the wrong
    // matrices or lighting). GPU-written resources stay one per viewport - commands run in order.
    static constexpr int NUM_UPLOAD_SLOTS = NUM_VIEWPORTS * FRAMES_IN_FLIGHT;

    bool init(ID3D12Device* device);

    // ignoreNearDepth: skip the "light is closer than the nearest visible surface in this tile"
    // rejection. Deferred lighting needs that rejection (a light behind visible geometry can't
    // light it); volumetric fog fills the space in front of geometry too, so it needs the full list.
    void cull(ID3D12GraphicsCommandList* cmd,
              GBufferPass& gbufferPass,
              const FrameLightData& lights,
              const Matrix& view,
              const Matrix& projection,
              uint32_t width, uint32_t height,
              int viewportIndex,
              bool ignoreNearDepth = false);

    D3D12_GPU_DESCRIPTOR_HANDLE getPointListSRV(int viewportIndex) const { return m_pointListSRV[viewportIndex].getGPUHandle(0); }
    D3D12_GPU_DESCRIPTOR_HANDLE getSpotListSRV(int viewportIndex) const { return m_spotListSRV[viewportIndex] .getGPUHandle(0); }

    uint32_t getNumTilesX(uint32_t w) const { return (w + TILE_SIZE - 1) / TILE_SIZE; }
    uint32_t getNumTilesY(uint32_t h) const { return (h + TILE_SIZE - 1) / TILE_SIZE; }

private:
    // Index into the NUM_UPLOAD_SLOTS arrays for this frame's copy of `viewportIndex`'s buffers.
    static int uploadSlot(int viewportIndex);
    bool createBuffers(ID3D12Device* device);
    bool createDescriptors();
    bool createUploadBuffers(ID3D12Device* device);
    void uploadCB(const FrameLightData& lights, const Matrix& view, const Matrix& projection,
                  uint32_t w, uint32_t h);

    LightCullingPipeline m_pipeline;

    ComPtr<ID3D12Resource> m_pointListBuf[NUM_VIEWPORTS];
    ComPtr<ID3D12Resource> m_spotListBuf[NUM_VIEWPORTS];
    UINT m_allocatedTiles[NUM_VIEWPORTS] = {};

    ShaderTableDesc m_pointListUAV[NUM_VIEWPORTS];
    ShaderTableDesc m_spotListUAV[NUM_VIEWPORTS];
    ShaderTableDesc m_pointListSRV[NUM_VIEWPORTS];
    ShaderTableDesc m_spotListSRV[NUM_VIEWPORTS];

    ComPtr<ID3D12Resource> m_pointLightBuf[NUM_UPLOAD_SLOTS];
    ComPtr<ID3D12Resource> m_spotLightBuf[NUM_UPLOAD_SLOTS];
    void* m_pointLightMapped[NUM_UPLOAD_SLOTS] = {};
    void* m_spotLightMapped[NUM_UPLOAD_SLOTS] = {};

    ShaderTableDesc m_pointLightSRV[NUM_UPLOAD_SLOTS];
    ShaderTableDesc m_spotLightSRV[NUM_UPLOAD_SLOTS];

    ComPtr<ID3D12Resource> m_cb[NUM_UPLOAD_SLOTS];
    void* m_cbMapped[NUM_UPLOAD_SLOTS] = {};

    uint32_t m_lastWidth[NUM_VIEWPORTS] = {};
    uint32_t m_lastHeight[NUM_VIEWPORTS] = {};
};

