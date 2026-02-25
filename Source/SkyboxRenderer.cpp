#include "Globals.h"
#include "SkyboxRenderer.h"
#include "SkyboxCube.h"
#include "EnvironmentMap.h"
#include <d3dx12.h>
#include <vector>
#include <fstream>

static std::vector<char> ReadBinary(const std::wstring& filename)
{
    std::ifstream file(filename, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
}

SkyboxRenderer::SkyboxRenderer() {}
SkyboxRenderer::~SkyboxRenderer() {}

bool SkyboxRenderer::initialize(ID3D12Device* device,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat,
    bool useMSAA)
{
    m_cube = std::make_unique<SkyboxCube>();

    if (!createRootSignature(device))
        return false;

    if (!createPipelineState(device, rtvFormat, dsvFormat, useMSAA))
        return false;

    return true;
}

bool SkyboxRenderer::createRootSignature(ID3D12Device* device)
{
    CD3DX12_DESCRIPTOR_RANGE textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE samplerRange;
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    CD3DX12_ROOT_PARAMETER params[3];

    params[0].InitAsConstantBufferView(0); // b0
    params[1].InitAsDescriptorTable(1, &textureRange);
    params[2].InitAsDescriptorTable(1, &samplerRange);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params);

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    if (FAILED(D3D12SerializeRootSignature(&desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &blob,
        &error)))
        return false;

    return SUCCEEDED(device->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));
}

bool SkyboxRenderer::createPipelineState(ID3D12Device* device,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat,
    bool useMSAA)
{
    auto vs = ReadBinary(L"SkyboxVS.cso");
    auto ps = ReadBinary(L"SkyboxPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.InputLayout = m_cube->getInputLayout();
    psoDesc.VS = { vs.data(), vs.size() };
    psoDesc.PS = { ps.data(), ps.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.NumRenderTargets = 1;
    psoDesc.DSVFormat = dsvFormat;
    psoDesc.SampleDesc.Count = useMSAA ? 4 : 1;
    psoDesc.SampleMask = UINT_MAX;

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    return SUCCEEDED(device->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&m_pipelineState)));
}

void SkyboxRenderer::render(
    ID3D12GraphicsCommandList* cmdList,
    const EnvironmentMap& environment,
    const Matrix& view,
    const Matrix& projection)
{
    if (!environment.isValid())
        return;

    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());

    // Remove camera translation
    Matrix viewNoTranslation = view;
    viewNoTranslation._41 = 0.0f;
    viewNoTranslation._42 = 0.0f;
    viewNoTranslation._43 = 0.0f;

    SkyboxCB cb;
    cb.viewProj = XMMatrixTranspose(viewNoTranslation * projection);

    cmdList->SetGraphicsRoot32BitConstants(
        0,
        sizeof(SkyboxCB) / 4,
        &cb,
        0);

    cmdList->SetGraphicsRootDescriptorTable(1, environment.gpuHandle);

    m_cube->draw(cmdList);
}