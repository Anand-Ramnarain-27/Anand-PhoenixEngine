#include "Globals.h"
#include "GBufferPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "Material.h"
#include "Mesh.h"
#include <d3dx12.h>
#include <algorithm>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }

    ComPtr<ID3D12Resource> makeUploadBuf(ID3D12Device* device, UINT64 bytes, void** mapped,
                                          const wchar_t* name){
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ComPtr<ID3D12Resource> buf;
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, IID_PPV_ARGS(&buf));
        if (FAILED(hr)){
            LOG("GBufferPass: upload buf failed 0x%08X", hr);
            return nullptr;
        }
        buf->SetName(name);
        if (mapped) buf->Map(0, nullptr, mapped);
        return buf;
    }

    void writeTex2DSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* tex){
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = tex->GetDesc().Format;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = tex->GetDesc().MipLevels;
        table.createSRV(tex, slot, &sv);
    }

    void writeFallbackSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* fallback){
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        table.createSRV(fallback, slot, &sv);
    }

    MeshPipeline::GpuMaterial toGpuMaterial(const Material* mat){
        MeshPipeline::GpuMaterial gm;
        if (!mat) return gm;
        const Material::Data& d = mat->getData();
        gm.baseColor = d.baseColor;
        gm.metallicFactor = d.metallic;
        gm.roughnessFactor = d.roughness;
        gm.normalScale = d.normalStrength;
        gm.occlusionStrength = d.aoStrength;
        gm.emissiveFactor = d.emissiveFactor;
        gm.alphaCutoff = d.alphaCutoff;
        gm.flags = d.flags;
        gm.padding = 0;
        return gm;
    }
}

bool GBufferPass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)){
        LOG("GBufferPass: pipeline init failed");
        return false;
    }
    if (!createUploadBuffers(device)) return false;
    if (!createFallbackTexture(device)) return false;
    if (!createMatTableRing()) return false;
    LOG("GBufferPass: init OK");
    return true;
}

bool GBufferPass::createUploadBuffers(ID3D12Device* device){
    const UINT mvpSz = cbAlign(sizeof(MeshPipeline::CbMVP));
    const UINT instSz = cbAlign(sizeof(MeshPipeline::CbPerInstance));

    for (int i = 0; i < NUM_VIEWPORTS; ++i){
        m_mvpRing[i] = makeUploadBuf(device, (UINT64)mvpSz * MAX_INSTANCES,
                                      &m_mvpMapped[i], L"GBufferPass_MVPRing");
        if (!m_mvpRing[i]) return false;

        m_instanceRing[i] = makeUploadBuf(device, (UINT64)instSz * MAX_INSTANCES,
                                           &m_instanceMapped[i], L"GBufferPass_InstanceRing");
        if (!m_instanceRing[i]) return false;
    }

    return true;
}

bool GBufferPass::createFallbackTexture(ID3D12Device* device){
    D3D12_RESOURCE_DESC td = {};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = td.Height = 1;
    td.DepthOrArraySize = 1;
    td.MipLevels = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc = { 1, 0 };
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
                                                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                  nullptr, IID_PPV_ARGS(&m_fallbackTex));
    if (FAILED(hr)){
        LOG("GBufferPass: fallback texture failed 0x%08X", hr);
        return false;
    }
    m_fallbackTex->SetName(L"GBufferPass_FallbackTex");
    return true;
}

bool GBufferPass::createMatTableRing(){
    auto* sd = app->getShaderDescriptors();
    for (int v = 0; v < NUM_VIEWPORTS; ++v){
        m_matRing[v].reserve(MAX_INSTANCES);
        for (UINT i = 0; i < MAX_INSTANCES; ++i){
            ShaderTableDesc t = sd->allocTable("GBufferPass_MatTex");
            if (!t.isValid()){
                LOG("GBufferPass: mat ring alloc failed at %u", i);
                return false;
            }
            writeFallbackSRV(t, 0, m_fallbackTex.Get());
            writeFallbackSRV(t, 1, m_fallbackTex.Get());
            writeFallbackSRV(t, 2, m_fallbackTex.Get());
            writeFallbackSRV(t, 3, m_fallbackTex.Get());
            writeFallbackSRV(t, 4, m_fallbackTex.Get());
            m_matRing[v].push_back(std::move(t));
        }
    }
    return true;
}

void GBufferPass::writePerDrawCBs(const MeshEntry& entry, const Matrix& viewProj,
                                   UINT slot, int viewportIndex,
                                   D3D12_GPU_VIRTUAL_ADDRESS& outMvpVA,
                                   D3D12_GPU_VIRTUAL_ADDRESS& outInstVA){
    const UINT mvpSz = cbAlign(sizeof(MeshPipeline::CbMVP));
    const UINT instSz = cbAlign(sizeof(MeshPipeline::CbPerInstance));

    Matrix world;
    memcpy(&world, entry.worldMatrix, sizeof(float) * 16);

    {
        MeshPipeline::CbMVP mvp;
        mvp.mvp = (world * viewProj).Transpose();
        memcpy(static_cast<char*>(m_mvpMapped[viewportIndex]) + (UINT64)slot * mvpSz, &mvp, sizeof(mvp));
    }

    {
        MeshPipeline::CbPerInstance inst = {};
        inst.modelMatrix = world.Transpose();
        Matrix inv;
        world.Invert(inv);
        inst.normalMatrix = inv;

        const Material* mat = entry.instanceMaterial.get();
        if (!mat) mat = entry.material;
        if (!mat && entry.materialRes) mat = entry.materialRes->getMaterial();
        inst.material = toGpuMaterial(mat);

        memcpy(static_cast<char*>(m_instanceMapped[viewportIndex]) + (UINT64)slot * instSz, &inst, sizeof(inst));
    }

    outMvpVA = m_mvpRing[viewportIndex]->GetGPUVirtualAddress() + (UINT64)slot * mvpSz;
    outInstVA = m_instanceRing[viewportIndex]->GetGPUVirtualAddress() + (UINT64)slot * instSz;
}

void GBufferPass::render(ID3D12GraphicsCommandList* cmd,
                          const std::vector<MeshEntry*>& meshes,
                          const Matrix& viewProj,
                          uint32_t width, uint32_t height,
                          int viewportIndex){
    if (width == 0 || height == 0) return;

    viewportIndex = (viewportIndex >= 0 && viewportIndex < NUM_VIEWPORTS) ? viewportIndex : 0;
    m_activeIndex = viewportIndex;
    GBuffer& gbuffer = m_gbuffer[viewportIndex];

    gbuffer.resize(width, height);

    BEGIN_EVENT(cmd, L"GBuffer Geometry Pass");

    gbuffer.beginGeomPass(cmd);

    if (!meshes.empty()){
        cmd->SetPipelineState(m_pipeline.getPSO());
        cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

        auto* samplerHeap = app->getSamplerHeap();
        ID3D12DescriptorHeap* heaps[] = {
            app->getShaderDescriptors()->getHeap(),
            samplerHeap->getHeap()
        };
        cmd->SetDescriptorHeaps(2, heaps);
        cmd->SetGraphicsRootDescriptorTable(GBufferPipeline::SLOT_SAMPLER,
                                             samplerHeap->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

        UINT slot = 0;
        for (MeshEntry* entry : meshes){
            if (!entry) continue;
            Mesh* mesh = entry->meshRes ? entry->meshRes->getMesh() : entry->mesh;
            if (!mesh) continue;
            if (slot >= MAX_INSTANCES){
                LOG("GBufferPass: MAX_INSTANCES exceeded");
                break;
            }

            D3D12_GPU_VIRTUAL_ADDRESS mvpVA, instVA;
            writePerDrawCBs(*entry, viewProj, slot, viewportIndex, mvpVA, instVA);

            cmd->SetGraphicsRootConstantBufferView(GBufferPipeline::SLOT_MVP_CB, mvpVA);
            cmd->SetGraphicsRootConstantBufferView(GBufferPipeline::SLOT_INSTANCE_CB, instVA);

            ShaderTableDesc& matTable = m_matRing[viewportIndex][slot];
            const Material* mat = entry->instanceMaterial.get();
            if (!mat) mat = entry->material;
            if (!mat && entry->materialRes) mat = entry->materialRes->getMaterial();

            writeFallbackSRV(matTable, 0, m_fallbackTex.Get());
            writeFallbackSRV(matTable, 1, m_fallbackTex.Get());
            writeFallbackSRV(matTable, 2, m_fallbackTex.Get());
            writeFallbackSRV(matTable, 3, m_fallbackTex.Get());
            writeFallbackSRV(matTable, 4, m_fallbackTex.Get());

            if (mat){
                if (mat->hasTexture() && mat->getBaseColorResource()) writeTex2DSRV(matTable, 0, mat->getBaseColorResource());
                if (mat->hasMetalRoughMap()&& mat->getMetalRoughResource()) writeTex2DSRV(matTable, 1, mat->getMetalRoughResource());
                if (mat->hasNormalMap() && mat->getNormalMapResource()) writeTex2DSRV(matTable, 2, mat->getNormalMapResource());
                if (mat->hasAOMap() && mat->getAOMapResource()) writeTex2DSRV(matTable, 3, mat->getAOMapResource());
                if (mat->hasEmissive() && mat->getEmissiveResource()) writeTex2DSRV(matTable, 4, mat->getEmissiveResource());
            }

            cmd->SetGraphicsRootDescriptorTable(GBufferPipeline::SLOT_MAT_TEXTURES,
                                                 matTable.getGPUHandle(0));
            if (entry->skinnedVA != 0)
                mesh->drawSkinned(cmd, entry->skinnedVA);
            else
                mesh->draw(cmd);
            ++slot;
        }
    }

    gbuffer.endGeomPass(cmd);

    END_EVENT(cmd);
}
