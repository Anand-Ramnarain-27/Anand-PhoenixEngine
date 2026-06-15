#pragma once
#include "Globals.h"
#include "Module.h"
#include "ModuleD3D12.h"
#include "Application.h"
#include "ModuleRTDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <d3dx12.h>
#include <wrl.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

struct alignas(256) FaceCB {
	DirectX::XMFLOAT4X4 vp;
	int flipX;
	int flipZ;
	float roughness;
	float pad[1];
};

namespace FaceProjection {
    struct FaceDesc {
        DirectX::XMFLOAT3 front;
        DirectX::XMFLOAT3 up;
    };

    inline const std::array<FaceDesc, 6>& faces(){
        static const std::array<FaceDesc, 6> kFaces =
        { {
            { { 1, 0, 0 }, { 0, 1, 0 } },
            { { -1, 0, 0 }, { 0, 1, 0 } },
            { { 0, 1, 0 }, { 0, 0, -1 } },
            { { 0, -1, 0 }, { 0, 0, 1 } },
            { { 0, 0, 1 }, { 0, 1, 0 } },
            { { 0, 0, -1 }, { 0, 1, 0 } },
        } };
        return kFaces;
    }

    inline DirectX::XMMATRIX viewProj(uint32_t faceIndex){
        using namespace DirectX;
        static const XMFLOAT3 fronts[6] = {
            { 1, 0, 0}, {-1, 0, 0},
            { 0, 1, 0}, { 0,-1, 0},
            { 0, 0, 1}, { 0, 0,-1}
        };
        static const XMFLOAT3 ups[6] = {
            {0, 1, 0}, {0, 1, 0},
            {0, 0, -1}, {0, 0,1},
            {0, 1, 0}, {0, 1, 0}
        };

        XMVECTOR eye = XMVectorZero();
        XMVECTOR at = XMLoadFloat3(&fronts[faceIndex]);
        XMVECTOR up = XMLoadFloat3(&ups[faceIndex]);
        XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 100.0f);
        return view * proj;
    }

    inline bool needsFlipZ(uint32_t){ return false; }
    inline bool needsFlipX(uint32_t){ return false; }
}

namespace CubeGeometry {
    static const float kCubeVerts[] = {
        -1, 1, -1, -1, -1, -1, 1, -1, -1,
         1, -1, -1, 1, 1, -1, -1, 1, -1,
        -1, -1, 1, -1, -1, -1, -1, 1, -1,
        -1, 1, -1, -1, 1, 1, -1, -1, 1,
         1, -1, -1, 1, -1, 1, 1, 1, 1,
         1, 1, 1, 1, 1, -1, 1, -1, -1,
        -1, -1, 1, -1, 1, 1, 1, 1, 1,
         1, 1, 1, 1, -1, 1, -1, -1, 1,
        -1, 1, -1, 1, 1, -1, 1, 1, 1,
         1, 1, 1, -1, 1, 1, -1, 1, -1,
        -1, -1, -1, -1, -1, 1, 1, -1, -1,
         1, -1, -1, -1, -1, 1, 1, -1, 1
    };

    static const uint32_t kCubeVertexCount = 36;
    static const uint32_t kCubeVertexStride = sizeof(float) * 3;
    static const uint32_t kCubeVertexSize = sizeof(kCubeVerts);
}

namespace CubemapPipelineBuilder {
    inline bool buildCubeFacePipeline(ID3D12Device* device, const wchar_t* psCsoPath, DXGI_FORMAT rtvFmt, ComPtr<ID3D12RootSignature>& outRS, ComPtr<ID3D12PipelineState>& outPSO){
        CD3DX12_ROOT_PARAMETER params[4];
        params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
        params[1].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_DESCRIPTOR_RANGE srvRange;
        srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        params[2].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_DESCRIPTOR_RANGE sampRange;
        sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);
        params[3].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init(4, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> blob, err;
        if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))){
            LOG("CubemapPipelineBuilder: root signature serialise failed: %s", err ? (char*)err->GetBufferPointer() : "unknown error");
            return false;
        }

        if (FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&outRS)))){
            LOG("CubemapPipelineBuilder: CreateRootSignature failed");
            return false;
        }

        auto vs = DX::ReadData(L"SkyboxVS.cso");
        auto ps = DX::ReadData(psCsoPath);

        D3D12_INPUT_ELEMENT_DESC layout = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = outRS.Get();
        psoDesc.VS = { vs.data(), vs.size() };
        psoDesc.PS = { ps.data(), ps.size() };
        psoDesc.InputLayout = { &layout, 1 };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.RTVFormats[0] = rtvFmt;
        psoDesc.NumRenderTargets = 1;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&outPSO)))){
            LOG("CubemapPipelineBuilder: PSO creation failed for '%ls'", psCsoPath);
            return false;
        }

        return true;
    }

    inline bool buildBRDFPipeline(ID3D12Device* device, ComPtr<ID3D12RootSignature>& outRS, ComPtr<ID3D12PipelineState>& outPSO){

        CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> blob, err;
        if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))){
            LOG("CubemapPipelineBuilder: BRDF root signature serialise failed");
            return false;
        }

        if (FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&outRS))))
            return false;

        auto vs = DX::ReadData(L"FullScreenVS.cso");
        auto ps = DX::ReadData(L"EnvironmentBRDFPS.cso");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = outRS.Get();
        psoDesc.VS = { vs.data(), vs.size() };
        psoDesc.PS = { ps.data(), ps.size() };
        psoDesc.InputLayout = { nullptr, 0 };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
        psoDesc.NumRenderTargets = 1;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&outPSO)))){
            LOG("CubemapPipelineBuilder: BRDF PSO creation failed");
            return false;
        }

        return true;
    }

}

class CubeFaceRenderer {
public:
    struct RenderParams {
        ID3D12GraphicsCommandList* cmd;
        ID3D12Resource* target;
        uint32_t faceIndex;
        uint32_t mipLevel;
        uint32_t totalMips;
        uint32_t baseFaceSize;
        float roughness;
        ID3D12RootSignature* rootSig;
        ID3D12PipelineState* pso;
        D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV;
        DXGI_FORMAT rtvFormat;
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress;
        D3D12_GPU_DESCRIPTOR_HANDLE samplerHandle;
    };

    static void RenderFace(const RenderParams& params){
        auto* rtDescs = app->getRTDescriptors();
        auto* samplers = app->getSamplerHeap();
        uint32_t mipSize = std::max(1u, params.baseFaceSize >> params.mipLevel);

        RenderTargetDesc rtv = rtDescs->create(params.target, params.faceIndex, params.mipLevel, params.rtvFormat);
        if (!rtv) return;

        UINT subRes = D3D12CalcSubresource(params.mipLevel, params.faceIndex, 0, params.totalMips, 6);
        auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(params.target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, subRes);
        params.cmd->ResourceBarrier(1, &toRT);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtv.getCPUHandle();
        params.cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        float clear[4] = { 0, 0, 0, 1 };
        params.cmd->ClearRenderTargetView(rtvHandle, clear, 0, nullptr);

        D3D12_VIEWPORT vp = { 0, 0, float(mipSize), float(mipSize), 0, 1 };
        D3D12_RECT sc = { 0, 0, LONG(mipSize), LONG(mipSize) };
        params.cmd->RSSetViewports(1, &vp);
        params.cmd->RSSetScissorRects(1, &sc);

        ID3D12DescriptorHeap* heaps[] = {app->getShaderDescriptors()->getHeap(), samplers->getHeap()};
        params.cmd->SetDescriptorHeaps(2, heaps);
        params.cmd->SetGraphicsRootSignature(params.rootSig);
        params.cmd->SetPipelineState(params.pso);
        params.cmd->SetGraphicsRootConstantBufferView(0, params.cbAddress);
        params.cmd->SetGraphicsRootDescriptorTable(1, params.sourceSRV);
        params.cmd->SetGraphicsRootDescriptorTable(2, params.samplerHandle);

        auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(params.target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, subRes);
        params.cmd->ResourceBarrier(1, &toSRV);
    }
};
