#include "Globals.h"
#include "MeshRenderPass.h"
#include "EnvironmentSystem.h"
#include "ModuleSamplerHeap.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleGPUResources.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "Material.h"
#include "Mesh.h"
#include "Application.h"
#include <d3dx12.h>

bool MeshRenderPass::init(ID3D12Device* device, bool useMSAA)
{
    if (!m_pipeline.init(device, useMSAA))
    {
        LOG("MeshRenderPass: failed to initialise MeshPipeline");
        return false;
    }

    if (!createFallbackTexture(device))
    {
        LOG("MeshRenderPass: failed to create fallback texture");
        return false;
    }

    LOG("MeshRenderPass: initialised OK");
    return true;
}

bool MeshRenderPass::createFallbackTexture(ID3D12Device* device)
{
    // Create a 1x1 Texture2D in the default heap. Its contents don't matter
    // (the shader never reads it due to has* flag guards) - it just needs to be
    // a valid descriptor. D3D12 requires every root signature descriptor table
    // to point to a valid descriptor for the entire draw call, even for slots
    // the shader doesn't touch. Without this the validation layer breaks when
    // IBL turns on and more code paths execute, causing the black-mesh symptom.
    auto* shaderDesc = app->getShaderDescriptors();

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc = { 1, 0 };
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr, IID_PPV_ARGS(&m_fallbackTex))))
    {
        LOG("MeshRenderPass: CreateCommittedResource for fallback texture failed");
        return false;
    }
    m_fallbackTex->SetName(L"MeshPass_FallbackTex1x1");

    m_fallbackTable = shaderDesc->allocTable("MeshFallback");
    if (!m_fallbackTable.isValid())
    {
        LOG("MeshRenderPass: failed to alloc fallback SRV table");
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    m_fallbackTable.createSRV(m_fallbackTex.Get(), 0, &srvDesc);

    return true;
}

void MeshRenderPass::render(
    ID3D12GraphicsCommandList* cmd,
    const std::vector<MeshEntry*>& meshes,
    D3D12_GPU_VIRTUAL_ADDRESS       lightCBAddr,
    const float                     viewProj[16],
    const EnvironmentSystem* env,
    ModuleSamplerHeap* samplerHeap)
{
    if (meshes.empty()) return;

    cmd->SetPipelineState(m_pipeline.getPSO());
    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    // Per-frame constants
    cmd->SetGraphicsRoot32BitConstants(MeshPipeline::SLOT_VP, 16, viewProj, 0);
    cmd->SetGraphicsRootConstantBufferView(MeshPipeline::SLOT_LIGHT_CB, lightCBAddr);

    // Sampler heap - all 4 samplers (s0-s3) matching Samplers.hlsli
    if (samplerHeap)
        cmd->SetGraphicsRootDescriptorTable(
            MeshPipeline::SLOT_SAMPLER,
            samplerHeap->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    // IBL textures - irradiance (t1), prefiltered env (t2), BRDF LUT (t3)
    m_pipeline.bindIBL(cmd, env);

    // Pre-bind all optional texture slots to the fallback white 1x1 texture.
    // D3D12 requires all descriptor tables referenced in the root signature to
    // point to valid descriptors for the whole draw call, even if the shader
    // won't read them (the has* flags guard sampling). Without this the
    // validation layer fires a break as soon as a slot is first touched, which
    // is what causes the black-mesh symptom when IBL enables.
    D3D12_GPU_DESCRIPTOR_HANDLE fallback = m_fallbackTable.getGPUHandle(0);
    cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_ALBEDO_TEX, fallback);
    cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_NORMAL_TEX, fallback);
    cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_AO_TEX, fallback);
    cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_EMISSIVE_TEX, fallback);
    cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_METALROUGH_TEX, fallback);

    for (MeshEntry* entry : meshes)
    {
        if (!entry) continue;

        Mesh* mesh = nullptr;
        if (entry->meshRes) mesh = entry->meshRes->getMesh();
        else                mesh = entry->mesh;
        if (!mesh) continue;

        // World and normal matrix (b1, 32 floats = 2 matrices)
        Matrix worldMat;
        memcpy(&worldMat, entry->worldMatrix, sizeof(float) * 16);
        auto wc = MeshPipeline::makeWorldConstants(worldMat);
        cmd->SetGraphicsRoot32BitConstants(MeshPipeline::SLOT_WORLD, 32, &wc, 0);

        // Per-mesh material constant buffer (b3)
        if (entry->materialCB)
            cmd->SetGraphicsRootConstantBufferView(
                MeshPipeline::SLOT_MATERIAL_CB,
                entry->materialCB->GetGPUVirtualAddress());

        // Only update slots this mesh actually has - the rest stay on fallback
        const Material* mat = nullptr;
        if (entry->materialRes) mat = entry->materialRes->getMaterial();
        else if (entry->material) mat = entry->material;

        if (mat)
        {
            if (mat->hasTexture())
                cmd->SetGraphicsRootDescriptorTable(
                    MeshPipeline::SLOT_ALBEDO_TEX,
                    mat->getTextureGPUHandle());

            if (mat->hasNormalMap())
                cmd->SetGraphicsRootDescriptorTable(
                    MeshPipeline::SLOT_NORMAL_TEX,
                    mat->getNormalMapGPUHandle());

            if (mat->hasAOMap())
                cmd->SetGraphicsRootDescriptorTable(
                    MeshPipeline::SLOT_AO_TEX,
                    mat->getAOMapGPUHandle());

            if (mat->hasEmissive())
                cmd->SetGraphicsRootDescriptorTable(
                    MeshPipeline::SLOT_EMISSIVE_TEX,
                    mat->getEmissiveGPUHandle());

            if (mat->hasMetalRoughMap())
                cmd->SetGraphicsRootDescriptorTable(
                    MeshPipeline::SLOT_METALROUGH_TEX,
                    mat->getMetalRoughGPUHandle());
        }

        mesh->draw(cmd);
    }
}