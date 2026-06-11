#pragma once

#include "GBuffer.h"
#include "GBufferPipeline.h"
#include "MeshEntry.h"
#include "MeshPipeline.h"
#include "ShaderTableDesc.h"
#include <vector>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class GBufferPass {
public:
    GBufferPass() = default;
    ~GBufferPass() = default;

    bool init(ID3D12Device* device);

    // Renders all meshes into the GBuffer (handles SRV->RTV->SRV transitions internally).
    // After this call the GBuffer color textures are in PIXEL_SHADER_RESOURCE state.
    // viewportIndex selects which of the per-viewport GBuffer instances to use (Scene=0,
    // Game=1) — renderSceneWithCamera runs once per viewport per frame with potentially
    // different sizes, and a single shared GBuffer would resize/release its resources
    // on every call, deferRelease()-ing textures still referenced by this same frame's
    // commands (D3D12 OBJECT_DELETED_WHILE_STILL_IN_USE).
    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<MeshEntry*>& meshes,
                const Matrix& viewProj,
                uint32_t width, uint32_t height,
                int viewportIndex);

    // Returns the GBuffer most recently sized/rendered by render() above.
    GBuffer& getGBuffer() { return m_gbuffer[m_activeIndex]; }
    GBufferPipeline& getPipeline() { return m_pipeline; }

    static constexpr int NUM_VIEWPORTS = 2;

private:
    bool createUploadBuffers(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);
    bool createMatTableRing();

    void writePerDrawCBs(const MeshEntry& entry, const Matrix& viewProj, UINT slot,
                         D3D12_GPU_VIRTUAL_ADDRESS& outMvpVA,
                         D3D12_GPU_VIRTUAL_ADDRESS& outInstVA);

    GBuffer m_gbuffer[NUM_VIEWPORTS];
    int m_activeIndex = 0;
    GBufferPipeline m_pipeline;

    static constexpr UINT MAX_INSTANCES = 512;

    ComPtr<ID3D12Resource> m_mvpRing;
    void* m_mvpMapped = nullptr;

    ComPtr<ID3D12Resource> m_instanceRing;
    void* m_instanceMapped = nullptr;

    ComPtr<ID3D12Resource> m_fallbackTex;
    std::vector<ShaderTableDesc> m_matRing;
};
