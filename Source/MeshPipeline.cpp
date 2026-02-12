#include "Globals.h"
#include "MeshPipeline.h"
#include "Mesh.h"
#include <d3dx12.h>

#include "ReadData.h"

bool MeshPipeline::init(ID3D12Device* device)
{
    if (!createRootSignature(device))
        return false;

    if (!createPSO(device))
        return false;

    return true;
}

bool MeshPipeline::createRootSignature(ID3D12Device* device)
{
    CD3DX12_ROOT_PARAMETER params[1];
    params[0].InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(1, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;

    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error)))
    {
        return false;
    }

    return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig))
    );
}

bool MeshPipeline::createPSO(ID3D12Device* device)
{
    auto vs = DX::ReadData(L"MeshVS.cso");
    auto ps = DX::ReadData(L"MeshPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

    desc.pRootSignature = rootSig.Get();
    desc.InputLayout =
    {
        Mesh::InputLayout,
        Mesh::InputLayoutCount
    };
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.NumRenderTargets = 1;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    return SUCCEEDED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso))
    );
}


