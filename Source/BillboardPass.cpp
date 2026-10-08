#include "Globals.h"
#include "BillboardPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleEditor.h"
#include "EffectTextureLoader.h"
#include <imgui.h>
#include <d3dx12.h>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }
}

bool BillboardPass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)){
        PHX_LOG(Render, Error, "BillboardPass: pipeline init failed");
        return false;
    }
    if (!createUploadBuffer(device)) return false;
    if (!createFallbackTexture(device)) return false;

    PHX_LOG(Render, Info, "BillboardPass: init OK");
    return true;
}

bool BillboardPass::createUploadBuffer(ID3D12Device* device){
    const UINT stride = cbAlign(sizeof(BillboardInstanceCB));
    // One slice per frame in flight (the CPU records up to FRAMES_IN_FLIGHT-1 frames ahead of the GPU); each slice
    // holds both views of a frame (Scene View + Game View).
    const UINT64 total = (UINT64)stride * SLOTS_PER_FRAME * FRAMES_IN_FLIGHT;
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer(total);
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr, IID_PPV_ARGS(&m_cbRing));
    if (FAILED(hr)){ PHX_LOG(Render, Error, "BillboardPass: CB ring alloc failed 0x%08X", hr); return false; }
    m_cbRing->SetName(L"Billboard_CBRing");
    m_cbRing->Map(0, nullptr, &m_cbMapped);
    return true;
}

bool BillboardPass::createFallbackTexture(ID3D12Device* device){
    (void)device;
    const uint32_t white = 0xFFFFFFFFu;
    m_fallbackTex = app->getGPUResources()->createRawTexture2D(&white, sizeof(white), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!m_fallbackTex){
        PHX_LOG(Render, Error, "BillboardPass: fallback texture creation failed");
        return false;
    }
    m_fallbackTex->SetName(L"Billboard_FallbackTex");

    auto* sd = app->getShaderDescriptors();
    m_fallbackSRV = sd->allocTable("Billboard_FallbackSRV");
    if (!m_fallbackSRV.isValid()){
        PHX_LOG(Render, Error, "BillboardPass: fallback SRV alloc failed");
        return false;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
    sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    m_fallbackSRV.createSRV(m_fallbackTex.Get(), 0, &sv);
    return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE BillboardPass::getOrLoadTexture(const std::string& path){
    if (path.empty())
        return m_fallbackSRV.getGPUHandle(0);

    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end())
        return it->second.srv.getGPUHandle(0);

    std::string resolvedPath = path;
    ComPtr<ID3D12Resource> tex = loadEffectTexture(path, &resolvedPath);
    if (!tex){
        PHX_LOG(Render, Error, "BillboardPass: failed to load texture '%s', using fallback", path.c_str());
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }

    ShaderTableDesc srv = app->getShaderDescriptors()->allocTable(("Billboard_SRV_" + path).c_str());
    if (!srv.isValid()){
        PHX_LOG(Render, Error, "BillboardPass: SRV alloc failed for '%s', using fallback", path.c_str());
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }
    srv.createTexture2DSRV(tex.Get(), 0);

    const D3D12_RESOURCE_DESC rd = tex->GetDesc();
    PHX_LOG(Render, Verbose, "BillboardPass: loaded '%s'%s%s%s (%llux%u, fmt %d)", resolvedPath.c_str(),
            resolvedPath != path ? " (resolved from '" : "", resolvedPath != path ? path.c_str() : "",
            resolvedPath != path ? "')" : "", (unsigned long long)rd.Width, rd.Height, (int)rd.Format);

    auto handle = srv.getGPUHandle(0);
    m_textureCache.emplace(path, CachedTexture{ std::move(tex), std::move(srv) });
    return handle;
}

void BillboardPass::render(ID3D12GraphicsCommandList* cmd,
                            const std::vector<BillboardInstance>& billboards,
                            uint32_t width, uint32_t height){
    if (billboards.empty() || width == 0 || height == 0) return;

    BEGIN_EVENT(cmd, L"Billboard Pass");

    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    ID3D12DescriptorHeap* heaps[] = {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplerHeap()->getHeap()
    };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootDescriptorTable(BillboardPipeline::SLOT_SAMPLER,
        app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->IASetIndexBuffer(nullptr);

    const UINT cbStride = cbAlign(sizeof(BillboardInstanceCB));
    const UINT remaining = (m_frameCBCursor < SLOTS_PER_FRAME) ? SLOTS_PER_FRAME - m_frameCBCursor : 0u;
    const UINT count = std::min({ (UINT)billboards.size(), MAX_BILLBOARDS, remaining });
    const UINT64 frameBase = (UINT64)app->getD3D12()->getCurrentBackBufferIdx() * SLOTS_PER_FRAME;

    // 0 = alpha, 1 = additive, 2 = premultiplied
    int blendBound = 0;
    cmd->SetPipelineState(m_pipeline.getPSO());

    for (UINT i = 0; i < count; ++i){
        const BillboardInstance& bb = billboards[i];

        const int blend = bb.premultiplied ? 2 : (bb.additive ? 1 : 0);
        if (blend != blendBound){
            blendBound = blend;
            cmd->SetPipelineState(blend == 2 ? m_pipeline.getPremultipliedPSO()
                                 : blend == 1 ? m_pipeline.getAdditivePSO() : m_pipeline.getPSO());
        }

        const UINT64 slot = frameBase + m_frameCBCursor + i;
        void* dst = reinterpret_cast<uint8_t*>(m_cbMapped) + slot * cbStride;
        memcpy(dst, &bb.cb, sizeof(BillboardInstanceCB));

        D3D12_GPU_VIRTUAL_ADDRESS cbVA = m_cbRing->GetGPUVirtualAddress() + slot * cbStride;
        cmd->SetGraphicsRootConstantBufferView(BillboardPipeline::SLOT_CB, cbVA);
        cmd->SetGraphicsRootDescriptorTable(BillboardPipeline::SLOT_TEXTURE,
                                             getOrLoadTexture(bb.texturePath));

        cmd->DrawInstanced(4, 1, 0, 0);
    }

    m_frameCBCursor += count;

    END_EVENT(cmd);
}

#include "Globals.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool BillboardPipeline::init(ID3D12Device* device){
    return createRootSignature(device) && createPSO(device);
}

bool BillboardPipeline::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE texRange; texRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                             ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[3];
    params[SLOT_CB ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_TEXTURE].InitAsDescriptorTable(1, &texRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        PHX_LOG(Render, Error, "BillboardPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){ PHX_LOG(Render, Error, "BillboardPipeline: CreateRootSignature failed 0x%08X", hr); return false; }
    return true;
}

bool BillboardPipeline::createPSO(ID3D12Device* device){
    auto vs = DX::ReadData(L"BillboardVS.cso");
    auto ps = DX::ReadData(L"BillboardPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.InputLayout = { nullptr, 0 };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = kSceneColorFormat;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    auto& rt = desc.BlendState.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_ZERO;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){ PHX_LOG(Render, Error, "BillboardPipeline: CreateGraphicsPipelineState failed 0x%08X", hr); return false; }

    auto& art = desc.BlendState.RenderTarget[0];
    art.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    art.DestBlend = D3D12_BLEND_ONE;
    hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_additivePso));
    if (FAILED(hr)){ PHX_LOG(Render, Error, "BillboardPipeline: CreateGraphicsPipelineState (additive) failed 0x%08X", hr); return false; }

    // Premultiplied: colour adds, alpha only occludes. A particle fading its tint alpha to 0 turns from a normal
    // alpha blend into a pure additive glow with the same texture.
    art.SrcBlend = D3D12_BLEND_ONE;
    art.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_premultipliedPso));
    if (FAILED(hr)){ PHX_LOG(Render, Error, "BillboardPipeline: CreateGraphicsPipelineState (premultiplied) failed 0x%08X", hr); return false; }

    return true;
}
