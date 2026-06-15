#include "Globals.h"
#include "LightCullingPipeline.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool LightCullingPipeline::init(ID3D12Device* device){
    return createRootSignature(device) && createPSO(device);
}

bool LightCullingPipeline::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE depthRange; depthRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE pointRange; pointRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE spotRange; spotRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    CD3DX12_DESCRIPTOR_RANGE pointUAV; pointUAV .Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE spotUAV; spotUAV .Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

    CD3DX12_ROOT_PARAMETER params[6];
    params[SLOT_CB ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_DEPTH ].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_POINT_LIGHTS].InitAsDescriptorTable(1, &pointRange, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_SPOT_LIGHTS ].InitAsDescriptorTable(1, &spotRange, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_POINT_UAV ].InitAsDescriptorTable(1, &pointUAV, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_SPOT_UAV ].InitAsDescriptorTable(1, &spotUAV, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("LightCullingPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){ LOG("LightCullingPipeline: CreateRootSignature failed 0x%08X", hr); return false; }
    return true;
}

bool LightCullingPipeline::createPSO(ID3D12Device* device){
    auto cs = DX::ReadData(L"LightCullingCS.cso");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.CS = { cs.data(), cs.size() };

    HRESULT hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){ LOG("LightCullingPipeline: CreateComputePipelineState failed 0x%08X", hr); return false; }
    return true;
}
