#include "Globals.h"
#include "SkyboxRenderer.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ReadData.h"
#include <d3dx12.h>

bool SkyboxRenderer::init(ID3D12Device* device)
{
    createCube();
    return createRootSignature(device) && createPSO(device);
}

void SkyboxRenderer::createCube()
{
    std::vector<Mesh::Vertex> vertices =
    {
        {{-1,-1,-1},{0,0},{0,0,0}},
        {{-1, 1,-1},{0,0},{0,0,0}},
        {{ 1, 1,-1},{0,0},{0,0,0}},
        {{ 1,-1,-1},{0,0},{0,0,0}},
        {{-1,-1, 1},{0,0},{0,0,0}},
        {{-1, 1, 1},{0,0},{0,0,0}},
        {{ 1, 1, 1},{0,0},{0,0,0}},
        {{ 1,-1, 1},{0,0},{0,0,0}},
    };

    std::vector<uint32_t> indices =
    {
        0,1,2, 0,2,3,   // -Z face
        4,6,5, 4,7,6,   // +Z face
        4,5,1, 4,1,0,   // -X face
        3,2,6, 3,6,7,   // +X face
        1,5,6, 1,6,2,   // +Y face
        4,0,3, 4,3,7    // -Y face
    };

    cube = std::make_unique<Mesh>();
    cube->setData(vertices, indices, -1);
}

bool SkyboxRenderer::createRootSignature(ID3D12Device* device)
{

    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER params[2];
    params[0].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); 
    params[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.MipLODBias = 0.0f;
    staticSampler.MaxAnisotropy = 1;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    staticSampler.MinLOD = 0.0f;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0; 
    staticSampler.RegisterSpace = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(
        _countof(params), params,
        1, &staticSampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(
        &rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);

    if (FAILED(hr))
    {
        if (error)
            LOG("SkyboxRenderer RS error: %s", (char*)error->GetBufferPointer());
        return false;
    }

    return SUCCEEDED(device->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&rootSig)));
}

bool SkyboxRenderer::createPSO(ID3D12Device* device)
{
    auto vs = DX::ReadData(L"SkyboxVS.cso");
    auto ps = DX::ReadData(L"SkyboxPS.cso");

    if (vs.empty() || ps.empty())
    {
        LOG("SkyboxRenderer: Failed to read shader bytecode");
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSig.Get();
    desc.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;

    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; 
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
    if (FAILED(hr))
    {
        LOG("SkyboxRenderer: CreateGraphicsPipelineState failed 0x%08X", hr);
        return false;
    }
    return true;
}

void SkyboxRenderer::render(
    ID3D12GraphicsCommandList* cmd,
    const EnvironmentMap& env,
    const Matrix& view,
    const Matrix& projection)
{
    if (!env.isValid()) return;

    cmd->SetPipelineState(pso.Get());
    cmd->SetGraphicsRootSignature(rootSig.Get());

    Matrix viewNoTranslation = view;
    viewNoTranslation._41 = 0.0f;
    viewNoTranslation._42 = 0.0f;
    viewNoTranslation._43 = 0.0f;

    Matrix viewProj = (viewNoTranslation * projection).Transpose();

    cmd->SetGraphicsRoot32BitConstants(0, 16, &viewProj, 0);
    cmd->SetGraphicsRootDescriptorTable(1, env.gpuHandle);

    cube->draw(cmd);
}