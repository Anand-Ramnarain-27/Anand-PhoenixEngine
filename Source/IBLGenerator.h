#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

class EnvironmentMap;

class IBLGenerator
{
public:
    bool generate(ID3D12Device* device,
        ID3D12GraphicsCommandList* cmd,
        EnvironmentMap& env);

private:
    struct alignas(256) FaceCB
    {
        DirectX::XMFLOAT4X4 vp;    
        int   flipX;               
        int   flipZ;
        float roughness;          
        float _pad;
    };

    bool createCubemapResource(ID3D12Device* device, uint32_t size, uint32_t mips,
        DXGI_FORMAT fmt, const wchar_t* name,
        ComPtr<ID3D12Resource>& out);

    bool create2DResource(ID3D12Device* device, uint32_t size,
        DXGI_FORMAT fmt, const wchar_t* name,
        ComPtr<ID3D12Resource>& out);

    bool createConvPipeline(ID3D12Device* device,
        const wchar_t* psCsoPath, DXGI_FORMAT rtvFmt,
        ComPtr<ID3D12RootSignature>& outRS,
        ComPtr<ID3D12PipelineState>& outPSO);

    bool createBRDFPipeline(ID3D12Device* device,
        ComPtr<ID3D12RootSignature>& outRS,
        ComPtr<ID3D12PipelineState>& outPSO);

    void renderCubeFace(ID3D12Device* device, ID3D12GraphicsCommandList* cmd,
        ID3D12Resource* target, uint32_t faceIndex,
        uint32_t mipLevel, uint32_t totalMips,
        uint32_t baseFaceSize, float roughness,
        ID3D12RootSignature* rs, ID3D12PipelineState* pso,
        D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV,
        DXGI_FORMAT rtvFmt);

    bool ensureGeometry(ID3D12Device* device);
    bool ensureFaceCB(ID3D12Device* device);

    ComPtr<ID3D12Resource>   m_cubeVB;
    D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
    bool                     m_geometryReady = false;

    ComPtr<ID3D12Resource> m_faceCB;   
    FaceCB* m_faceCBPtr = nullptr;

    ComPtr<ID3D12RootSignature> m_irradianceRS, m_prefilterRS, m_brdfRS;
    ComPtr<ID3D12PipelineState> m_irradiancePSO, m_prefilterPSO, m_brdfPSO;

public:
    void releasePipelines()
    {
        m_irradianceRS.Reset(); m_irradiancePSO.Reset();
        m_prefilterRS.Reset();  m_prefilterPSO.Reset();
        m_brdfRS.Reset();       m_brdfPSO.Reset();
    }
};