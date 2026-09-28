#include "Globals.h"
#include "XRayPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleRTDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "GBufferPass.h"
#include "MeshEntry.h"
#include "ResourceMesh.h"
#include "Mesh.h"
#include "RenderTexture.h"
#include "ReadData.h"
#include <d3dx12.h>
#include <algorithm>

namespace {
    constexpr DXGI_FORMAT kMaskFormat = DXGI_FORMAT_R8G8_UNORM;

    // Mask root signature: slots 0..3 mirror GBufferPipeline so the GBuffer's per-draw CBVs and material
    // tables can be rebound as-is.
    constexpr UINT MASK_SLOT_MVP_CB = 0;
    constexpr UINT MASK_SLOT_INSTANCE_CB = 1;
    constexpr UINT MASK_SLOT_MAT_TEXTURES = 2;
    constexpr UINT MASK_SLOT_SAMPLER = 3;
    constexpr UINT MASK_SLOT_DEPTH = 4;       // t5
    constexpr UINT MASK_SLOT_CONSTANTS = 5;   // b3: groupValue, depthBias

    // Composite constants (b0). Layout must match XRayCompositeCB in XRayCompositePS.hlsl.
    struct CompositeConstants {
        Vector4 groupColor[XRayPass::kMaxGroups];    // rgb, a unused
        Vector4 groupParams[XRayPass::kMaxGroups];   // x fillAlpha, y outlineWidth
        uint32_t groupCount = 0;
        uint32_t pad[3] = {};
    };
    constexpr UINT kCompositeConstantCount = sizeof(CompositeConstants) / 4;

    // Depths come from the same VS and the same CB contents as the GBuffer draw, so a visible fragment matches
    // the stored depth exactly; the bias only has to absorb rasterisation noise.
    constexpr float kDepthBias = 2e-6f;

    bool serializeRootSig(ID3D12Device* device, const CD3DX12_ROOT_SIGNATURE_DESC& desc,
                          ComPtr<ID3D12RootSignature>& out, const char* who){
        ComPtr<ID3DBlob> blob, err;
        if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))){
            LOG("XRayPass: %s root signature serialise failed: %s", who, err ? (char*)err->GetBufferPointer() : "unknown error");
            return false;
        }
        return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&out)));
    }
}

bool XRayPass::init(ID3D12Device* device){
    if (!createMaskPipeline(device)){
        LOG("XRayPass: mask pipeline creation failed");
        return false;
    }
    if (!createCompositePipeline(device)){
        LOG("XRayPass: composite pipeline creation failed");
        return false;
    }
    LOG("XRayPass: init OK");
    return true;
}

void XRayPass::cleanUp(){
    for (int i = 0; i < NUM_VIEWPORTS; ++i) releaseMask(i);
}

bool XRayPass::createMaskPipeline(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE matRange;
    matRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);
    CD3DX12_DESCRIPTOR_RANGE samplerRange;
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);
    CD3DX12_DESCRIPTOR_RANGE depthRange;
    depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);

    CD3DX12_ROOT_PARAMETER params[6];
    params[MASK_SLOT_MVP_CB].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    params[MASK_SLOT_INSTANCE_CB].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);
    params[MASK_SLOT_MAT_TEXTURES].InitAsDescriptorTable(1, &matRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[MASK_SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[MASK_SLOT_DEPTH].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[MASK_SLOT_CONSTANTS].InitAsConstants(2, 3, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    if (!serializeRootSig(device, desc, m_maskRootSig, "mask")) return false;

    // Same VS binary as the GBuffer pass: identical SV_Position.z for identical inputs.
    auto vs = DX::ReadData(L"GBufferVS.cso");
    auto ps = DX::ReadData(L"XRayMaskPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = m_maskRootSig.Get();
    pd.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
    pd.VS = { vs.data(), vs.size() };
    pd.PS = { ps.data(), ps.size() };
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1;
    pd.RTVFormats[0] = kMaskFormat;
    pd.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pd.SampleDesc = { 1, 0 };
    pd.SampleMask = UINT_MAX;

    // Must match GBufferPipeline's rasterizer so the same triangles (and so the same depths) survive.
    pd.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pd.RasterizerState.FrontCounterClockwise = TRUE;

    pd.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    auto& rt = pd.BlendState.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_ONE;
    rt.DestBlend = D3D12_BLEND_ONE;
    rt.BlendOp = D3D12_BLEND_OP_MAX;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_ONE;
    rt.BlendOpAlpha = D3D12_BLEND_OP_MAX;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN;

    pd.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pd.DepthStencilState.DepthEnable = FALSE;
    pd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    if (FAILED(device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&m_maskPso)))) return false;
    m_maskPso->SetName(L"XRayMaskPSO");

    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    if (FAILED(device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&m_maskPsoDoubleSided)))) return false;
    m_maskPsoDoubleSided->SetName(L"XRayMaskPSO_DoubleSided");
    return true;
}

bool XRayPass::createCompositePipeline(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE maskRange;
    maskRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER params[2];
    params[0].InitAsConstants(kCompositeConstantCount, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    params[1].InitAsDescriptorTable(1, &maskRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
    if (!serializeRootSig(device, desc, m_compositeRootSig, "composite")) return false;

    auto vs = DX::ReadData(L"FullScreenVS.cso");
    auto ps = DX::ReadData(L"XRayCompositePS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = m_compositeRootSig.Get();
    pd.VS = { vs.data(), vs.size() };
    pd.PS = { ps.data(), ps.size() };
    pd.InputLayout = { nullptr, 0 };
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1;
    pd.RTVFormats[0] = kSceneColorFormat;
    pd.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pd.SampleDesc = { 1, 0 };
    pd.SampleMask = UINT_MAX;
    pd.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pd.DepthStencilState.DepthEnable = FALSE;
    pd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    pd.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    auto& rt = pd.BlendState.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ZERO;   // keep the scene's alpha
    rt.DestBlendAlpha = D3D12_BLEND_ONE;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    if (FAILED(device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&m_compositePso)))) return false;
    m_compositePso->SetName(L"XRayCompositePSO");
    return true;
}

void XRayPass::releaseMask(int viewportIndex){
    Mask& m = m_masks[viewportIndex];
    // The GPU may still be reading last frame's mask: retire it through the deferred-free queue.
    if (m.texture) app->getGPUResources()->deferRelease(m.texture);
    m.texture.Reset();
    m.rtv.reset();
    m.srv.reset();
    m.state = D3D12_RESOURCE_STATE_COMMON;
    m.width = m.height = 0;
}

void XRayPass::ensureMask(int viewportIndex, uint32_t width, uint32_t height){
    Mask& m = m_masks[viewportIndex];
    if (m.texture && m.width == width && m.height == height) return;
    releaseMask(viewportIndex);

    m.texture = app->getGPUResources()->createRenderTarget(kMaskFormat, width, height, 1,
                                                           Vector4(0.f, 0.f, 0.f, 0.f), "XRay_Mask");
    if (!m.texture) return;
    m.state = D3D12_RESOURCE_STATE_COMMON;
    m.rtv = app->getRTDescriptors()->create(m.texture.Get());
    m.srv = app->getShaderDescriptors()->allocTable("XRay_MaskSRV");
    if (m.srv.isValid()) m.srv.createTexture2DSRV(m.texture.Get(), 0, kMaskFormat);
    m.width = width;
    m.height = height;
}

void XRayPass::render(ID3D12GraphicsCommandList* cmd, GBufferPass& gbufferPass,
                      const std::vector<MeshEntry*>& xrayMeshes,
                      const XRayGroupStyle* groups, int groupCount,
                      RenderTexture* outputRT, uint32_t width, uint32_t height, int viewportIndex){
    if (xrayMeshes.empty() || !groups || groupCount <= 0 || !outputRT || !outputRT->isValid()) return;
    if (width == 0 || height == 0) return;
    GBuffer& gbuffer = gbufferPass.getGBuffer();
    if (!gbuffer.isValid() || !gbuffer.isDepthReadable()) return;

    viewportIndex = (viewportIndex >= 0 && viewportIndex < NUM_VIEWPORTS) ? viewportIndex : 0;
    ensureMask(viewportIndex, width, height);
    Mask& mask = m_masks[viewportIndex];
    if (!mask.texture || !mask.srv.isValid()) return;

    auto* samplerHeap = app->getSamplerHeap();
    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), samplerHeap->getHeap() };
    const D3D12_VIEWPORT vp = { 0.f, 0.f, float(width), float(height), 0.f, 1.f };
    const D3D12_RECT sc = { 0, 0, LONG(width), LONG(height) };

    // --- Mask ---------------------------------------------------------------------------------------
    BEGIN_EVENT(cmd, L"XRay Mask");

    if (mask.state != D3D12_RESOURCE_STATE_RENDER_TARGET){
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(mask.texture.Get(), mask.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &b);
        mask.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE maskRtv = mask.rtv.getCPUHandle();
    const float clear[4] = { 0.f, 0.f, 0.f, 0.f };
    cmd->OMSetRenderTargets(1, &maskRtv, FALSE, nullptr);
    cmd->ClearRenderTargetView(maskRtv, clear, 0, nullptr);
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetDescriptorHeaps(2, heaps);
    cmd->SetGraphicsRootSignature(m_maskRootSig.Get());
    cmd->SetPipelineState(m_maskPso.Get());
    cmd->SetGraphicsRootDescriptorTable(MASK_SLOT_SAMPLER, samplerHeap->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));
    cmd->SetGraphicsRootDescriptorTable(MASK_SLOT_DEPTH, gbuffer.getDepthSrvHandle());

    ID3D12PipelineState* boundPso = m_maskPso.Get();
    for (MeshEntry* e : xrayMeshes){
        if (!e || e->xrayGroup == 0 || e->xrayGroup > groupCount) continue;
        if (e->gbMvpVA == 0 || e->gbInstVA == 0) continue;   // not drawn into the GBuffer this frame
        Mesh* mesh = e->meshRes ? e->meshRes->getMesh() : e->mesh;
        if (!mesh) continue;

        ID3D12PipelineState* pso = e->gbDoubleSided ? m_maskPsoDoubleSided.Get() : m_maskPso.Get();
        if (pso != boundPso){ cmd->SetPipelineState(pso); boundPso = pso; }

        const float consts[2] = { float(e->xrayGroup) / 255.0f, kDepthBias };
        cmd->SetGraphicsRoot32BitConstants(MASK_SLOT_CONSTANTS, 2, consts, 0);
        cmd->SetGraphicsRootConstantBufferView(MASK_SLOT_MVP_CB, e->gbMvpVA);
        cmd->SetGraphicsRootConstantBufferView(MASK_SLOT_INSTANCE_CB, e->gbInstVA);
        cmd->SetGraphicsRootDescriptorTable(MASK_SLOT_MAT_TEXTURES, e->gbMatTable);

        if (e->skinnedVA != 0) mesh->drawSkinned(cmd, e->skinnedVA);
        else mesh->draw(cmd);
    }

    {
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(mask.texture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &b);
        mask.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    END_EVENT(cmd);

    // --- Composite ----------------------------------------------------------------------------------
    BEGIN_EVENT(cmd, L"XRay Composite");

    CompositeConstants cc;
    cc.groupCount = (uint32_t)std::min(groupCount, kMaxGroups);
    for (uint32_t i = 0; i < cc.groupCount; ++i){
        cc.groupColor[i] = Vector4(groups[i].color.x, groups[i].color.y, groups[i].color.z, 1.0f);
        cc.groupParams[i] = Vector4(groups[i].fillAlpha, groups[i].outlineWidth, 0.f, 0.f);
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE outRtv = outputRT->getRtvHandle();
    cmd->OMSetRenderTargets(1, &outRtv, FALSE, nullptr);
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetGraphicsRootSignature(m_compositeRootSig.Get());
    cmd->SetPipelineState(m_compositePso.Get());
    cmd->SetGraphicsRoot32BitConstants(0, kCompositeConstantCount, &cc, 0);
    cmd->SetGraphicsRootDescriptorTable(1, mask.srv.getGPUHandle(0));
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);

    END_EVENT(cmd);

    // Leave the scene target bound the way the forward/particle passes do, for anything drawn after us.
    const D3D12_CPU_DESCRIPTOR_HANDLE roDsv = gbuffer.getReadOnlyDsvHandle();
    cmd->OMSetRenderTargets(1, &outRtv, FALSE, &roDsv);
}
