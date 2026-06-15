#pragma once
#include "DecalPipeline.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
using Microsoft::WRL::ComPtr;

class GBufferPass;

struct DecalInstance {
    Matrix mvp;
    Matrix invModel;
    Matrix invViewProj;
    Vector4 colourOpacity;
};

class DecalPass {
public:
    static constexpr UINT MAX_DECALS = 64;

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd,
                GBufferPass& gbufferPass,
                const std::vector<DecalInstance>& decals,
                uint32_t width, uint32_t height);

private:
    bool createUnitBox(ID3D12Device* device);
    bool createUploadBuffers(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device, ID3D12GraphicsCommandList* cmd,
                                ComPtr<ID3D12Resource>& texUpload);

    DecalPipeline m_pipeline;

    ComPtr<ID3D12Resource> m_vb;
    ComPtr<ID3D12Resource> m_ib;
    D3D12_VERTEX_BUFFER_VIEW m_vbv = {};
    D3D12_INDEX_BUFFER_VIEW m_ibv = {};
    UINT m_indexCount = 0;

    ComPtr<ID3D12Resource> m_cbRing;
    void* m_cbMapped = nullptr;

    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc m_fallbackSRV;
};
