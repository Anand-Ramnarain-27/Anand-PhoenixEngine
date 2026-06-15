#include "Globals.h"
#include "DecalPass.h"
#include "GBufferPass.h"
#include "GBuffer.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include <d3dx12.h>
#include <algorithm>
#include <cstring>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }

    static const float kBoxVerts[] = {
        -0.5f, -0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f,
         0.5f, 0.5f, 0.5f,
         0.5f, -0.5f, 0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, 0.5f, -0.5f,
         0.5f, 0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
    };

    static const uint16_t kBoxIndices[] = {
        0,2,1, 0,3,2,
        4,5,6, 4,6,7,
        1,6,5, 1,2,6,
        0,4,7, 0,7,3,
        0,1,5, 0,5,4,
        3,6,2, 3,7,6,
    };

    ComPtr<ID3D12Resource> uploadBuffer(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd,
                                         const void* data, UINT64 bytes, const wchar_t* name,
                                         D3D12_RESOURCE_STATES finalState,
                                         ComPtr<ID3D12Resource>& uploadHeap){
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ComPtr<ID3D12Resource> buf;
        dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&buf));
        buf->SetName(name);

        auto uhp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto ubd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        dev->CreateCommittedResource(&uhp, D3D12_HEAP_FLAG_NONE, &ubd,
                                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadHeap));
        void* mapped;
        uploadHeap->Map(0, nullptr, &mapped);
        memcpy(mapped, data, (size_t)bytes);
        uploadHeap->Unmap(0, nullptr);

        cmd->CopyBufferRegion(buf.Get(), 0, uploadHeap.Get(), 0, bytes);
        auto bar = CD3DX12_RESOURCE_BARRIER::Transition(buf.Get(),
                       D3D12_RESOURCE_STATE_COPY_DEST, finalState);
        cmd->ResourceBarrier(1, &bar);
        return buf;
    }
}

bool DecalPass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)){
        LOG("DecalPass: pipeline init failed");
        return false;
    }
    if (!createUploadBuffers(device)) return false;

    auto* d3d = app->getD3D12();
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> cmd;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr,
                               IID_PPV_ARGS(&cmd));

    ComPtr<ID3D12Resource> vbUpload, ibUpload;
    m_vb = uploadBuffer(device, cmd.Get(), kBoxVerts, sizeof(kBoxVerts),
                         L"Decal_VB", D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, vbUpload);
    m_ib = uploadBuffer(device, cmd.Get(), kBoxIndices, sizeof(kBoxIndices),
                         L"Decal_IB", D3D12_RESOURCE_STATE_INDEX_BUFFER, ibUpload);

    ComPtr<ID3D12Resource> texUpload;
    if (!createFallbackTexture(device, cmd.Get(), texUpload)) return false;

    m_vbv.BufferLocation = m_vb->GetGPUVirtualAddress();
    m_vbv.SizeInBytes = sizeof(kBoxVerts);
    m_vbv.StrideInBytes = 3 * sizeof(float);

    m_ibv.BufferLocation = m_ib->GetGPUVirtualAddress();
    m_ibv.SizeInBytes = sizeof(kBoxIndices);
    m_ibv.Format = DXGI_FORMAT_R16_UINT;
    m_indexCount = _countof(kBoxIndices);

    cmd->Close();
    ID3D12CommandList* lists[] = { cmd.Get() };
    d3d->getDrawCommandQueue()->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    d3d->getDrawCommandQueue()->Signal(fence.Get(), 1);
    HANDLE evt = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    fence->SetEventOnCompletion(1, evt);
    WaitForSingleObject(evt, INFINITE);
    CloseHandle(evt);

    LOG("DecalPass: init OK");
    return true;
}

bool DecalPass::createUploadBuffers(ID3D12Device* device){
    const UINT stride = cbAlign(sizeof(DecalInstance));
    const UINT64 total = stride * MAX_DECALS;
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer(total);
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr, IID_PPV_ARGS(&m_cbRing));
    if (FAILED(hr)){ LOG("DecalPass: CB ring alloc failed 0x%08X", hr); return false; }
    m_cbRing->SetName(L"Decal_CBRing");
    m_cbRing->Map(0, nullptr, &m_cbMapped);
    return true;
}

bool DecalPass::createFallbackTexture(ID3D12Device* device, ID3D12GraphicsCommandList* cmd,
                                       ComPtr<ID3D12Resource>& texUpload){
    D3D12_RESOURCE_DESC td = {};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = td.Height = 1;
    td.DepthOrArraySize = 1;
    td.MipLevels = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc = { 1, 0 };
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
                                     D3D12_RESOURCE_STATE_COPY_DEST,
                                     nullptr, IID_PPV_ARGS(&m_fallbackTex));
    m_fallbackTex->SetName(L"Decal_FallbackTex");

    UINT64 uploadSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    device->GetCopyableFootprints(&td, 0, 1, 0, &footprint, nullptr, nullptr, &uploadSize);

    auto uhp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto ubd = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    device->CreateCommittedResource(&uhp, D3D12_HEAP_FLAG_NONE, &ubd,
                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&texUpload));
    void* mapped = nullptr;
    texUpload->Map(0, nullptr, &mapped);
    const uint32_t whitePixel = 0xFFFFFFFFu;
    memcpy(static_cast<uint8_t*>(mapped) + footprint.Offset, &whitePixel, sizeof(whitePixel));
    texUpload->Unmap(0, nullptr);

    CD3DX12_TEXTURE_COPY_LOCATION dst(m_fallbackTex.Get(), 0);
    CD3DX12_TEXTURE_COPY_LOCATION src(texUpload.Get(), footprint);
    cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_fallbackTex.Get(),
                     D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &toSRV);

    auto* sd = app->getShaderDescriptors();
    m_fallbackSRV = sd->allocTable("Decal_FallbackSRV");
    if (!m_fallbackSRV.isValid()){
        LOG("DecalPass: fallback SRV alloc failed");
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

void DecalPass::render(ID3D12GraphicsCommandList* cmd,
                        GBufferPass& gbufferPass,
                        const std::vector<DecalInstance>& decals,
                        uint32_t width, uint32_t height){
    if (decals.empty() || width == 0 || height == 0) return;

    GBuffer& gb = gbufferPass.getGBuffer();
    if (!gb.isValid()) return;

    BEGIN_EVENT(cmd, L"Decal Pass");

    {
        CD3DX12_RESOURCE_BARRIER barriers[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(gb.getColorTexture(GBuffer::Albedo),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET),
            CD3DX12_RESOURCE_BARRIER::Transition(gb.getColorTexture(GBuffer::NormalMetalRough),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET)
        };
        cmd->ResourceBarrier(2, barriers);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {
        gb.getRtvHandle(GBuffer::Albedo),
        gb.getRtvHandle(GBuffer::NormalMetalRough)
    };
    D3D12_CPU_DESCRIPTOR_HANDLE roDsv = gb.getReadOnlyDsvHandle();
    cmd->OMSetRenderTargets(2, rtvHandles, FALSE, &roDsv);

    D3D12_VIEWPORT vp = { 0.f, 0.f, float(width), float(height), 0.f, 1.f };
    D3D12_RECT sc = { 0, 0, LONG(width), LONG(height) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetPipelineState(m_pipeline.getPSO());
    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    ID3D12DescriptorHeap* heaps[] = {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplerHeap()->getHeap()
    };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootDescriptorTable(DecalPipeline::SLOT_DEPTH, gb.getDepthSrvHandle());

    cmd->SetGraphicsRootDescriptorTable(DecalPipeline::SLOT_SAMPLER,
        app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbv);
    cmd->IASetIndexBuffer(&m_ibv);

    const UINT cbStride = cbAlign(sizeof(DecalInstance));
    const UINT maxDecals = std::min((UINT)decals.size(), MAX_DECALS);

    for (UINT i = 0; i < maxDecals; ++i){
        void* dst = reinterpret_cast<uint8_t*>(m_cbMapped) + i * cbStride;
        memcpy(dst, &decals[i], sizeof(DecalInstance));

        D3D12_GPU_VIRTUAL_ADDRESS cbVA =
            m_cbRing->GetGPUVirtualAddress() + i * cbStride;
        cmd->SetGraphicsRootConstantBufferView(DecalPipeline::SLOT_CB, cbVA);

        cmd->SetGraphicsRootDescriptorTable(DecalPipeline::SLOT_ALBEDO,
                                             m_fallbackSRV.getGPUHandle(0));

        cmd->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }

    {
        CD3DX12_RESOURCE_BARRIER barriers[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(gb.getColorTexture(GBuffer::Albedo),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(gb.getColorTexture(GBuffer::NormalMetalRough),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };
        cmd->ResourceBarrier(2, barriers);
    }

    END_EVENT(cmd);
}

#include "Globals.h"
#include "GBuffer.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool DecalPipeline::init(ID3D12Device* device){
    return createRootSignature(device) && createPSO(device);
}

bool DecalPipeline::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE depthRange; depthRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE albedoRange; albedoRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                             ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[4];
    params[SLOT_CB ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_DEPTH ].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_ALBEDO ].InitAsDescriptorTable(1, &albedoRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("DecalPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){ LOG("DecalPipeline: CreateRootSignature failed 0x%08X", hr); return false; }
    return true;
}

bool DecalPipeline::createPSO(ID3D12Device* device){
    auto vs = DX::ReadData(L"DecalVS.cso");
    auto ps = DX::ReadData(L"DecalPS.cso");

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.InputLayout = { layout, _countof(layout) };
    desc.PrimitiveTopologyType= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    desc.NumRenderTargets = 2;
    desc.RTVFormats[0] = GBuffer::kAlbedoFormat;
    desc.RTVFormats[1] = GBuffer::kNormalMetalRoughFormat;
    desc.DSVFormat = GBuffer::kDepthFormat;
    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.BlendState.IndependentBlendEnable = TRUE;
    desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.BlendState.RenderTarget[1].RenderTargetWriteMask = 0;

    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){ LOG("DecalPipeline: CreateGraphicsPipelineState failed 0x%08X", hr); return false; }
    return true;
}
