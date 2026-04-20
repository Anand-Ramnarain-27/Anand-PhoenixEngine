#include "Globals.h"
#include "GBufferPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleGPUResources.h"
#include "ModuleRingBuffer.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "Mesh.h"
#include "MeshEntry.h"
#include "Material.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "ReadData.h"
#include <d3dx12.h>
#include <algorithm>

static constexpr UINT SLOT_MVP = 0;
static constexpr UINT SLOT_PERFRAME = 1;
static constexpr UINT SLOT_INSTANCE = 2;
static constexpr UINT SLOT_TEXTURES = 3;  
static constexpr UINT SLOT_SAMPLER = 4;

bool GBufferPass::createRootSignature(ID3D12Device* device) {
    CD3DX12_DESCRIPTOR_RANGE texRange;
    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0); // t0..t4

    CD3DX12_DESCRIPTOR_RANGE sampRange;
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
        ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[5];
    params[SLOT_MVP].InitAsConstantBufferView(0, 0,
        D3D12_SHADER_VISIBILITY_VERTEX);
    params[SLOT_PERFRAME].InitAsConstantBufferView(1, 0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_INSTANCE].InitAsConstantBufferView(2, 0,
        D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_TEXTURES].InitAsDescriptorTable(1, &texRange,
        D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER].InitAsDescriptorTable(1, &sampRange,
        D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(5, params, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&desc,
        D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) {
        LOG("GBufferPass: root sig failed: %s",
            err ? (char*)err->GetBufferPointer() : "?");
        return false;
    }
    return SUCCEEDED(device->CreateRootSignature(0,
        blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSig)));
}

bool GBufferPass::createPSO(ID3D12Device* device) {
    auto vs = DX::ReadData(L"PBRForwardVS.cso");  
    auto ps = DX::ReadData(L"GBufferPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };

    desc.NumRenderTargets = 3;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;   
    desc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;  
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.FrontCounterClockwise = TRUE;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = UINT_MAX;

    HRESULT hr = device->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)) {
        LOG("GBufferPass: PSO creation failed 0x%08X", hr);
        return false;
    }
    return true;
}

bool GBufferPass::init(ID3D12Device* device) {
    if (!createRootSignature(device)) return false;
    if (!createPSO(device))           return false;
    if (!createUploadBuffers(device)) return false;
    if (!createFallbackTextures(device)) return false;
    if (!createMatTableRing())        return false;
    LOG("GBufferPass: init OK");
    return true;
}

bool GBufferPass::createUploadBuffers(ID3D12Device* device) {
    const UINT mvpSz = cbAlign(sizeof(MeshPipeline::CbMVP));
    const UINT instSz = cbAlign(sizeof(MeshPipeline::CbPerInstance));

    auto makeUpload = [&](UINT64 bytes, void** mapped, const wchar_t* name)
        -> ComPtr<ID3D12Resource> {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ComPtr<ID3D12Resource> buf;
        device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE,
            &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&buf));
        buf->SetName(name);
        if (mapped) buf->Map(0, nullptr, mapped);
        return buf;
        };

    m_mvpRing = makeUpload((UINT64)mvpSz * MAX_INSTANCES,
        &m_mvpMapped, L"GBufPass_MVP");
    m_instanceRing = makeUpload((UINT64)instSz * MAX_INSTANCES,
        &m_instanceMapped, L"GBufPass_Inst");

    return m_mvpRing && m_instanceRing;
}

bool GBufferPass::createFallbackTextures(ID3D12Device* device) {
    D3D12_RESOURCE_DESC td = {};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = td.Height = 1;
    td.DepthOrArraySize = 1;
    td.MipLevels = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc = { 1, 0 };

    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&m_fallbackTex2D));

    if (FAILED(hr)) {
        LOG("GBufferPass: fallback Texture2D failed 0x%08X", hr);
        return false;
    }
    m_fallbackTex2D->SetName(L"GBufPass_Fallback2D");
    return true;
}

bool GBufferPass::createMatTableRing() {
    auto* sd = app->getShaderDescriptors();
    m_matRing.reserve(MAX_INSTANCES);

    for (UINT i = 0; i < MAX_INSTANCES; ++i) {
        ShaderTableDesc t = sd->allocTable("GBufPass_MatTex");
        if (!t.isValid()) {
            LOG("GBufferPass: mat ring alloc failed at index %u", i);
            return false;
        }

        for (UINT slot = 0; slot < 5; ++slot) {
            D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
            sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sv.Texture2D.MipLevels = 1;
            t.createSRV(m_fallbackTex2D.Get(), slot, &sv);
        }
        m_matRing.push_back(std::move(t));
    }
    return true;
}

void GBufferPass::writePerDrawCBs(const MeshEntry& entry, const Matrix& viewProj,
    UINT slot,
    D3D12_GPU_VIRTUAL_ADDRESS& outMvpVA,
    D3D12_GPU_VIRTUAL_ADDRESS& outInstVA) {

    const UINT mvpSz = cbAlign(sizeof(MeshPipeline::CbMVP));
    const UINT instSz = cbAlign(sizeof(MeshPipeline::CbPerInstance));

    Matrix world;
    memcpy(&world, entry.worldMatrix, sizeof(float) * 16);

    {
        MeshPipeline::CbMVP mvp;
        mvp.mvp = (world * viewProj).Transpose();
        memcpy(static_cast<char*>(m_mvpMapped) + (UINT64)slot * mvpSz, &mvp, sizeof(mvp));
    }

    {
        MeshPipeline::CbPerInstance inst = {};
        inst.modelMatrix = world.Transpose();

        Matrix inv;
        world.Invert(inv);
        inst.normalMatrix = inv;

        const Material* mat = nullptr;
        if (entry.materialRes) mat = entry.materialRes->getMaterial();
        else if (entry.material) mat = entry.material;

        if (mat) {
            const Material::Data& d = mat->getData();
            inst.material.baseColor = d.baseColor;
            inst.material.metallicFactor = d.metallic;
            inst.material.roughnessFactor = d.roughness;
            inst.material.normalScale = d.normalStrength;
            inst.material.occlusionStrength = d.aoStrength;
            inst.material.emissiveFactor = d.emissiveFactor;
            inst.material.alphaCutoff = d.alphaCutoff;
            inst.material.flags = d.flags;
        }

        memcpy(static_cast<char*>(m_instanceMapped) + (UINT64)slot * instSz, &inst, sizeof(inst));
    }

    outMvpVA = m_mvpRing->GetGPUVirtualAddress() + (UINT64)slot * mvpSz;
    outInstVA = m_instanceRing->GetGPUVirtualAddress() + (UINT64)slot * instSz;
}

void GBufferPass::render(
    ID3D12GraphicsCommandList* cmd,
    const std::vector<MeshEntry*>& meshes,
    const Vector3& cameraPos,
    const Matrix& viewProj,
    GBuffer& gbuffer,
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
    uint32_t width, uint32_t height) {

    if (meshes.empty() || !gbuffer.isValid()) return;

    auto* sd = app->getShaderDescriptors();
    auto* samplers = app->getSamplerHeap();

    gbuffer.transitionAll(cmd,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[3];
    gbuffer.getRTVHandles(rtvs);
    cmd->OMSetRenderTargets(3, rtvs, FALSE, &dsvHandle);

    float clear[4] = { 0.f, 0.f, 0.f, 0.f };
    cmd->ClearRenderTargetView(rtvs[0], clear, 0, nullptr);
    cmd->ClearRenderTargetView(rtvs[1], clear, 0, nullptr);
    cmd->ClearRenderTargetView(rtvs[2], clear, 0, nullptr);
    // NOTE: Depth is cleared by the viewport RenderTexture::beginRender

    D3D12_VIEWPORT vp{ 0.f, 0.f, float(width), float(height), 0.f, 1.f };
    D3D12_RECT sc{ 0, 0, LONG(width), LONG(height) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    ID3D12DescriptorHeap* heaps[] = { sd->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);
    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    cmd->SetGraphicsRootDescriptorTable(SLOT_SAMPLER,
        samplers->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    // Reuse MeshPipeline::CbPerFrame with zero light counts
    MeshPipeline::CbPerFrame frame{};
    frame.cameraPosition = cameraPos;
    // Upload inline via ring buffer
    auto frameAddr = app->getRingBuffer()->allocate(frame);
    cmd->SetGraphicsRootConstantBufferView(SLOT_PERFRAME, frameAddr);

    UINT slot = 0;
    for (MeshEntry* entry : meshes) {
        if (!entry) continue;
        Mesh* mesh = entry->meshRes ? entry->meshRes->getMesh()
            : entry->mesh;
        if (!mesh || slot >= MAX_INSTANCES) break;

        D3D12_GPU_VIRTUAL_ADDRESS mvpVA, instVA;
        writePerDrawCBs(*entry, viewProj, slot, mvpVA, instVA);

        cmd->SetGraphicsRootConstantBufferView(SLOT_MVP, mvpVA);
        cmd->SetGraphicsRootConstantBufferView(SLOT_INSTANCE, instVA);

        // Bind material textures (or fallbacks)
        ShaderTableDesc& tbl = m_matRing[slot];
        const Material* mat = entry->materialRes
            ? entry->materialRes->getMaterial() : entry->material;
        bindMaterialTextures(tbl, mat);
        cmd->SetGraphicsRootDescriptorTable(SLOT_TEXTURES, tbl.getGPUHandle(0));

        mesh->draw(cmd);
        ++slot;
    }

    gbuffer.transitionAll(cmd,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GBufferPass::bindMaterialTextures(ShaderTableDesc& tbl, const Material* mat) {
    // Reset all slots to fallback
    auto writeFallback = [&](UINT slot) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        tbl.createSRV(m_fallbackTex2D.Get(), slot, &sv);
        };
    auto writeTex = [&](UINT slot, ID3D12Resource* tex) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = tex->GetDesc().Format;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = tex->GetDesc().MipLevels;
        tbl.createSRV(tex, slot, &sv);
        };

    for (UINT i = 0; i < 5; ++i) writeFallback(i);
    if (!mat) return;
    if (mat->hasTexture() && mat->getBaseColorResource())  writeTex(0, mat->getBaseColorResource());
    if (mat->hasMetalRoughMap() && mat->getMetalRoughResource()) writeTex(1, mat->getMetalRoughResource());
    if (mat->hasNormalMap() && mat->getNormalMapResource())  writeTex(2, mat->getNormalMapResource());
    if (mat->hasAOMap() && mat->getAOMapResource())      writeTex(3, mat->getAOMapResource());
    if (mat->hasEmissive() && mat->getEmissiveResource())   writeTex(4, mat->getEmissiveResource());
}
