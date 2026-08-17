#include "Globals.h"
#include "BloomPass.h"
#include "Application.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "RenderTexture.h"
#include "EditorViewport.h"
#include "ReadData.h"
#include <d3dx12.h>

bool BloomPass::init(ID3D12Device* device){
    if (!createRootSignature(device)){
        LOG("BloomPass: root signature creation failed");
        return false;
    }

    m_extractPSO = createPSO(device, L"BloomExtractPS.cso", false);
    m_downsamplePSO = createPSO(device, L"KawaseDownsamplePS.cso", false);
    m_upsamplePSO = createPSO(device, L"KawaseUpsamplePS.cso", true);

    if (!m_extractPSO || !m_downsamplePSO || !m_upsamplePSO){
        LOG("BloomPass: PSO creation failed");
        return false;
    }

    LOG("BloomPass: init OK");
    return true;
}

bool BloomPass::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE sampRange;
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[3];
    params[0].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    params[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[2].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))){
        LOG("BloomPass: root signature serialise failed: %s", err ? (char*)err->GetBufferPointer() : "unknown error");
        return false;
    }

    return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig)));
}

ComPtr<ID3D12PipelineState> BloomPass::createPSO(ID3D12Device* device, const wchar_t* psFile, bool additiveBlend){
    auto vs = DX::ReadData(L"FullScreenVS.cso");
    auto ps = DX::ReadData(psFile);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.InputLayout = { nullptr, 0 };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.RTVFormats[0] = kSceneColorFormat;
    desc.NumRenderTargets = 1;
    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    if (additiveBlend){
        desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }

    ComPtr<ID3D12PipelineState> pso;
    if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)))) return nullptr;
    return pso;
}

void BloomPass::drawFullscreen(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso,
                               RenderTexture* input, RenderTexture* output, float threshold){
    output->beginRender(cmd, false);

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(pso);
    cmd->SetGraphicsRoot32BitConstants(0, 1, &threshold, 0);
    cmd->SetGraphicsRootDescriptorTable(1, input->getSrvHandle());
    cmd->SetGraphicsRootDescriptorTable(2, app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);

    output->endRender(cmd);
}

void BloomPass::render(ID3D12GraphicsCommandList* cmd, RenderTexture* hdrSource, EditorViewport& viewport, float threshold){
    RenderTexture* mip0 = viewport.bloomMips[0].get();
    RenderTexture* mip1 = viewport.bloomMips[1].get();
    RenderTexture* mip2 = viewport.bloomMips[2].get();
    if (!hdrSource || !hdrSource->isValid() || !mip0->isValid() || !mip1->isValid() || !mip2->isValid()) return;

    BEGIN_EVENT(cmd, L"Bloom");

    drawFullscreen(cmd, m_extractPSO.Get(), hdrSource, mip0, threshold);
    drawFullscreen(cmd, m_downsamplePSO.Get(), mip0, mip1, 0.f);
    drawFullscreen(cmd, m_downsamplePSO.Get(), mip1, mip2, 0.f);
    drawFullscreen(cmd, m_upsamplePSO.Get(), mip2, mip1, 0.f);
    drawFullscreen(cmd, m_upsamplePSO.Get(), mip1, mip0, 0.f);

    END_EVENT(cmd);
}
