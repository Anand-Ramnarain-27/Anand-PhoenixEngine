#pragma once

#include "MeshEntry.h"
#include "ShaderTableDesc.h"
#include "DepthStencilDesc.h"
#include "RenderTargetDesc.h"
#include "ShadowMath.h"
#include <SimpleMath.h>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX::SimpleMath;

class GBuffer;

struct ShadowRenderData {
    bool enabled = false;
    int cascadeCount = 1;
    Matrix lightViewProj[ShadowMath::kMaxCascades];
    float cascadeSplit[ShadowMath::kMaxCascades] = {};
    Vector3 lightDir = { 0.f, -1.f, 0.f };
    float bias = 0.0015f;
    float normalBias = 0.0f;
    float pcfRadius = 1.0f;
    int mode = 0;
    bool debugTint = false;
    float expK = 16.0f;
    float lightBleed = 0.2f;
    float ambientStrength = 0.5f;
    uint32_t resolution = 2048;
    D3D12_GPU_DESCRIPTOR_HANDLE srv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE momentSrv = {};
    bool gpuMode = false;
    D3D12_GPU_VIRTUAL_ADDRESS gpuVpVA = 0;

    bool spotEnabled = false;
    Matrix spotViewProj;
    Vector3 spotPos = { 0.f, 0.f, 0.f };
    float spotBias = 0.0020f;
    float spotPcfRadius = 1.0f;
    uint32_t spotResolution = 1024;
    D3D12_GPU_DESCRIPTOR_HANDLE spotSrv = {};

    bool pointEnabled = false;
    Vector3 pointPos = { 0.f, 0.f, 0.f };
    float pointRange = 10.0f;
    float pointBias = 0.01f;
    uint32_t pointResolution = 1024;
    D3D12_GPU_DESCRIPTOR_HANDLE pointSrv = {};
};

class ShadowMapPipeline {
public:
    static constexpr UINT SLOT_MVP_CB = 0;

    bool init(ID3D12Device* device);
    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};

class ShadowMomentsPipeline {
public:
    static constexpr UINT SLOT_MVP_CB = 0;
    static constexpr UINT SLOT_MOMENT_CONSTS = 1;

    bool init(ID3D12Device* device);
    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};

class ShadowCubePipeline {
public:
    static constexpr UINT SLOT_MVP_CB = 0;

    bool init(ID3D12Device* device);
    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};

class ShadowReducePipeline {
public:
    static constexpr UINT SLOT_CB = 0;
    static constexpr UINT SLOT_INPUT = 1;
    static constexpr UINT SLOT_OUTPUT = 2;

    bool init(ID3D12Device* device);
    ID3D12PipelineState* getInitPSO() const { return m_initPso.Get(); }
    ID3D12PipelineState* getReducePSO() const { return m_reducePso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_initPso;
    ComPtr<ID3D12PipelineState> m_reducePso;
};

class ShadowLightMatrixPipeline {
public:
    static constexpr UINT SLOT_CB = 0;
    static constexpr UINT SLOT_INPUT = 1;
    static constexpr UINT SLOT_OUTPUT = 2;

    bool init(ID3D12Device* device);
    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};

class ShadowDepthGpuPipeline {
public:
    static constexpr UINT SLOT_WORLD_CB = 0;
    static constexpr UINT SLOT_VP_CB = 1;

    bool init(ID3D12Device* device);
    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};

class ShadowBlurPipeline {
public:
    static constexpr UINT SLOT_BLUR_CONSTS = 0;
    static constexpr UINT SLOT_INPUT = 1;
    static constexpr UINT SLOT_OUTPUT = 2;

    bool init(ID3D12Device* device);
    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};

class ShadowMapPass {
public:
    bool init(ID3D12Device* device);

    void beginFrame(){ m_ringCursor = 0; m_cubeCursor = 0; }

    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<MeshEntry*>& meshes,
                const Matrix* viewProjs, int cascadeCount,
                uint32_t resolution, int mode, float expK, float lightBleed);

    void renderSpot(ID3D12GraphicsCommandList* cmd, const std::vector<MeshEntry*>& meshes,
                    const Matrix& spotViewProj, uint32_t resolution);
    void renderPoint(ID3D12GraphicsCommandList* cmd, const std::vector<MeshEntry*>& meshes,
                     const Matrix faceViewProj[6], const Vector3& lightPos,
                     float range, uint32_t resolution);

    D3D12_GPU_DESCRIPTOR_HANDLE getSrvHandle() const { return m_srvTable.getGPUHandle(0); }
    D3D12_GPU_DESCRIPTOR_HANDLE getMomentsSrvHandle() const { return m_momentSrv.getGPUHandle(0); }
    bool hasMoments() const { return m_momentTex != nullptr; }
    D3D12_GPU_DESCRIPTOR_HANDLE getSpotSrvHandle() const { return m_spotSrv.getGPUHandle(0); }
    bool hasSpot() const { return m_spotDepth != nullptr; }
    uint32_t getSpotResolution() const { return m_spotRes; }
    D3D12_GPU_DESCRIPTOR_HANDLE getPointSrvHandle() const { return m_pointSrv.getGPUHandle(0); }
    bool hasPoint() const { return m_pointCube != nullptr; }
    uint32_t getResolution() const { return m_resolution; }

    bool computeGpuLightMatrix(ID3D12GraphicsCommandList* cmd, GBuffer& gbuffer,
                               const Matrix& invViewProj, const Vector3& lightDir,
                               float sunDistance);
    void renderDirectionalGpu(ID3D12GraphicsCommandList* cmd,
                              const std::vector<MeshEntry*>& meshes, uint32_t resolution);
    D3D12_GPU_VIRTUAL_ADDRESS getGpuVpVA() const {
        return m_vpBuffer ? m_vpBuffer->GetGPUVirtualAddress() : 0;
    }
    bool gpuVpReady() const { return m_vpReady; }

private:
    bool ensureResources(uint32_t resolution, int cascadeCount);
    bool ensureMomentResources();
    bool ensureSpotResources(uint32_t resolution);
    bool ensurePointResources(uint32_t resolution);
    bool ensureReduceResources(uint32_t depthW, uint32_t depthH);
    void renderDepth(ID3D12GraphicsCommandList* cmd, const std::vector<MeshEntry*>& meshes,
                     const Matrix* viewProjs, int cascadeCount);
    void renderMoments(ID3D12GraphicsCommandList* cmd, const std::vector<MeshEntry*>& meshes,
                       const Matrix* viewProjs, int cascadeCount, int mode, float expK);
    void blurMoments(ID3D12GraphicsCommandList* cmd);

    ShadowMapPipeline m_pipeline;
    ShadowMomentsPipeline m_momentPipeline;
    ShadowCubePipeline m_cubePipeline;
    ShadowBlurPipeline m_blurPipeline;
    ShadowReducePipeline m_reducePipeline;
    ShadowLightMatrixPipeline m_lightMatrixPipeline;
    ShadowDepthGpuPipeline m_depthGpuPipeline;

    ComPtr<ID3D12Resource> m_depthTexture;
    DepthStencilDesc m_dsvSlice[ShadowMath::kMaxCascades];
    ShaderTableDesc m_srvTable;
    uint32_t m_resolution = 0;
    int m_cascadeCount = 0;
    bool m_readable = false;

    ComPtr<ID3D12Resource> m_momentTex;
    ComPtr<ID3D12Resource> m_momentBlur;
    RenderTargetDesc m_momentRtv[ShadowMath::kMaxCascades];
    ShaderTableDesc m_momentSrv;
    ShaderTableDesc m_momentUav;
    ShaderTableDesc m_blurSrv;
    ShaderTableDesc m_blurUav;
    bool m_momentReadable = false;

    ComPtr<ID3D12Resource> m_spotDepth;
    DepthStencilDesc m_spotDsv;
    ShaderTableDesc m_spotSrv;
    uint32_t m_spotRes = 0;
    bool m_spotReadable = false;

    ComPtr<ID3D12Resource> m_pointCube;
    ComPtr<ID3D12Resource> m_pointDepth;
    RenderTargetDesc m_pointRtv[6];
    DepthStencilDesc m_pointDsv;
    ShaderTableDesc m_pointSrv;
    uint32_t m_pointRes = 0;
    bool m_pointReadable = false;

    static constexpr UINT MAX_DRAWS = 4096;
    ComPtr<ID3D12Resource> m_mvpRing;
    void* m_mvpMapped = nullptr;

    ComPtr<ID3D12Resource> m_cubeRing;
    void* m_cubeMapped = nullptr;

    UINT m_ringCursor = 0;
    UINT m_cubeCursor = 0;

    ComPtr<ID3D12Resource> m_reduceA;
    ComPtr<ID3D12Resource> m_reduceB;
    ShaderTableDesc m_reduceSrvA, m_reduceUavA, m_reduceSrvB, m_reduceUavB;
    uint32_t m_reduceW = 0, m_reduceH = 0;
    ComPtr<ID3D12Resource> m_vpBuffer;
    ShaderTableDesc m_vpUav;
    bool m_vpReady = false;
    ComPtr<ID3D12Resource> m_lightMatrixCB;
    void* m_lightMatrixMapped = nullptr;
};
