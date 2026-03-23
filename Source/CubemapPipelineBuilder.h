#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include "ReadData.h"
#include "Globals.h"
#include "ModuleSamplerHeap.h"

using Microsoft::WRL::ComPtr;

namespace CubemapPipelineBuilder
{
    inline bool buildCubeFacePipeline(ID3D12Device* device, const wchar_t* psCsoPath, DXGI_FORMAT rtvFmt, ComPtr<ID3D12RootSignature>& outRS, ComPtr<ID3D12PipelineState>& outPSO) {
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
        if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) {
            LOG("CubemapPipelineBuilder: root signature serialise failed: %s", err ? (char*)err->GetBufferPointer() : "unknown error");
            return false;
        }

        if (FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&outRS)))) {
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

        if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&outPSO)))) {
            LOG("CubemapPipelineBuilder: PSO creation failed for '%ls'", psCsoPath);
            return false;
        }

        return true;
    }

    inline bool buildBRDFPipeline(ID3D12Device* device, ComPtr<ID3D12RootSignature>& outRS, ComPtr<ID3D12PipelineState>& outPSO) {
     
        CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> blob, err;
        if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) {
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

        if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&outPSO)))) {
            LOG("CubemapPipelineBuilder: BRDF PSO creation failed");
            return false;
        }

        return true;
    }

} 