#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <string>
#include "ShaderTableDesc.h"
#include "FaceCB.h"

using Microsoft::WRL::ComPtr;

class EnvironmentMap;

class HDRToCubemapPass {
public:
    HDRToCubemapPass();
    ~HDRToCubemapPass();

    bool loadHDRTexture(ID3D12Device* device, const std::string& hdrFile, EnvironmentMap& env);
    bool createCubemapResource(ID3D12Device* device, EnvironmentMap& env, uint32_t cubeFaceSize);
    bool recordConversion(ID3D12GraphicsCommandList* cmd, EnvironmentMap& env);
    bool recordMipLevel(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env, uint32_t mipIndex);
    bool finaliseSRV(EnvironmentMap& env);

    uint32_t getNumMips() const { return m_numMips; }

private:
    bool createConversionPipeline(ID3D12Device* device);
    bool createMipPipeline(ID3D12Device* device);
    bool ensureGeometry(ID3D12Device* device);

    void renderFace(ID3D12GraphicsCommandList* cmd, ID3D12Resource* target, uint32_t faceIndex, uint32_t mipLevel, uint32_t totalMips, uint32_t baseFaceSize, D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV, ID3D12RootSignature* rs, ID3D12PipelineState* pso, DXGI_FORMAT rtvFmt);
    void blitMipFace(ID3D12GraphicsCommandList* cmd, ID3D12Resource* cubemap, uint32_t faceIndex, uint32_t dstMip, uint32_t totalMips, uint32_t baseFaceSize, ShaderTableDesc& mipTable);

    ComPtr<ID3D12RootSignature> m_convRS;
    ComPtr<ID3D12PipelineState> m_convPSO;

    ComPtr<ID3D12RootSignature> m_mipRS;
    ComPtr<ID3D12PipelineState> m_mipPSO;

    ComPtr<ID3D12Resource> m_cubeVB;
    D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
    bool m_geometryReady = false;

    ComPtr<ID3D12Resource> m_hdrTex;
    ShaderTableDesc m_hdrSRVTable;

    uint32_t m_cubeFaceSize = 0;
    uint32_t m_numMips = 0;
};