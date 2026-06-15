#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class BillboardPipeline {
public:
    static constexpr UINT SLOT_CB = 0;
    static constexpr UINT SLOT_TEXTURE = 1;
    static constexpr UINT SLOT_SAMPLER = 2;

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12PipelineState* getAdditivePSO() const { return m_additivePso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
    ComPtr<ID3D12PipelineState> m_additivePso;
};
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <unordered_map>
using Microsoft::WRL::ComPtr;

class RenderTexture;
class GBufferPass;

struct BillboardInstanceCB {
    Matrix viewProj;
    Vector4 centerHalfWidth;
    Vector4 rightHalfHeight;
    Vector4 up;
    Vector4 tint;
    Vector4 frameRectA;
    Vector4 frameRectB;
    Vector4 blendFactor;
};

struct BillboardInstance {
    BillboardInstanceCB cb;
    std::string texturePath;
    bool additive = false;
};

class BillboardPass {
public:
    static constexpr UINT MAX_BILLBOARDS = 512;

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<BillboardInstance>& billboards,
                uint32_t width, uint32_t height);

private:
    bool createUploadBuffer(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);
    D3D12_GPU_DESCRIPTOR_HANDLE getOrLoadTexture(const std::string& path);

    BillboardPipeline m_pipeline;

    ComPtr<ID3D12Resource> m_cbRing;
    void* m_cbMapped = nullptr;

    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc m_fallbackSRV;

    struct CachedTexture {
        ComPtr<ID3D12Resource> resource;
        ShaderTableDesc srv;
    };
    std::unordered_map<std::string, CachedTexture> m_textureCache;
};

