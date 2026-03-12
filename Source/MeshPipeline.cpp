#include "Globals.h"
#include "MeshPipeline.h"
#include "EnvironmentSystem.h"
#include "EnvironmentMap.h"
#include "Mesh.h"
#include "ReadData.h"
#include <d3dx12.h>

bool MeshPipeline::init(ID3D12Device* device)
{
    return createRootSignature(device) && createPSO(device);
}

// ?????????????????????????????????????????????????????????????????????????????
bool MeshPipeline::createRootSignature(ID3D12Device* device)
{
    // One descriptor range per texture register (t0 … t7).
    // Each range has NumDescriptors=1 and its own base register so they can
    // each live in a separate heap slot.
    CD3DX12_DESCRIPTOR_RANGE albedoRange, samplerRange,
        irradianceRange, prefilterRange, brdfRange,
        normalRange, aoRange, emissiveRange, metalRoughRange;

    albedoRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);   // t0
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0); // s0..sN
    irradianceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);   // t1
    prefilterRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);   // t2
    brdfRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);   // t3
    normalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);   // t4
    aoRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);   // t5
    emissiveRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);   // t6
    metalRoughRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);   // t7

    CD3DX12_ROOT_PARAMETER params[13];

    // Slot 0 – VP matrix (16 × 32-bit constants ? b0, VS only)
    params[SLOT_VP].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    // Slot 1 – World matrix (16 × 32-bit constants ? b1, VS only)
    params[SLOT_WORLD].InitAsConstants(16, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    // Slot 2 – LightCB (CBV ? b2, visible to both VS and PS)
    params[SLOT_LIGHT_CB].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL);
    // Slot 3 – MaterialCB (CBV ? b3, PS only)
    params[SLOT_MATERIAL_CB].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // Texture slots (all PS-only descriptor tables)
    params[SLOT_ALBEDO_TEX].InitAsDescriptorTable(1, &albedoRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_IRRADIANCE].InitAsDescriptorTable(1, &irradianceRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_PREFILTER].InitAsDescriptorTable(1, &prefilterRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_BRDF_LUT].InitAsDescriptorTable(1, &brdfRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_NORMAL_TEX].InitAsDescriptorTable(1, &normalRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_AO_TEX].InitAsDescriptorTable(1, &aoRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_EMISSIVE_TEX].InitAsDescriptorTable(1, &emissiveRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_METALROUGH_TEX].InitAsDescriptorTable(1, &metalRoughRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    if (FAILED(D3D12SerializeRootSignature(&desc,
        D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error)))
    {
        if (error) OutputDebugStringA((char*)error->GetBufferPointer());
        return false;
    }

    return SUCCEEDED(device->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSig)));
}

// ?????????????????????????????????????????????????????????????????????????????
bool MeshPipeline::createPSO(ID3D12Device* device)
{
    auto vs = DX::ReadData(L"MeshVS.cso");
    auto ps = DX::ReadData(L"MeshPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
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

    return SUCCEEDED(device->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&m_pso)));
}

// ?????????????????????????????????????????????????????????????????????????????
void MeshPipeline::bindIBL(ID3D12GraphicsCommandList* cmd,
    const EnvironmentSystem* env) const
{
    if (!env || !env->hasIBL()) return;

    const EnvironmentMap* map = env->getEnvironmentMap();
    cmd->SetGraphicsRootDescriptorTable(SLOT_IRRADIANCE, map->getIrradianceGPU());
    cmd->SetGraphicsRootDescriptorTable(SLOT_PREFILTER, map->getPrefilteredGPU());
    cmd->SetGraphicsRootDescriptorTable(SLOT_BRDF_LUT, map->getBRDFLUTGPU());
}