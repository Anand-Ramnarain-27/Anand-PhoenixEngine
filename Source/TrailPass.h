#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class TrailPipeline {
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
#include "ComponentTrail.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <unordered_map>
using Microsoft::WRL::ComPtr;

struct TrailInstanceCB {
    Matrix viewProj;
    Vector4 tint;
};

struct TrailInstance {
    std::vector<ComponentTrail::TrailVertex> vertices;
    Vector4 tint = Vector4(1.f, 1.f, 1.f, 1.f);
    std::string texturePath;
    bool additive = false;
    Vector3 sortPos;
    int layer = 0;
};

class TrailPass {
public:
    static constexpr UINT MAX_TRAIL_VERTICES = 1u << 15;
    static constexpr UINT MAX_TRAILS = 64;

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<TrailInstance>& trails,
                const Matrix& viewProj,
                uint32_t width, uint32_t height);

private:
    bool createBuffers(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);
    D3D12_GPU_DESCRIPTOR_HANDLE getOrLoadTexture(const std::string& path);

    TrailPipeline m_pipeline;

    static constexpr UINT kCbAlign = 256u;

    ComPtr<ID3D12Resource> m_vbRing;
    void* m_vbMapped = nullptr;
    UINT m_vbStride = 0;

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

