#include "Globals.h"
#include "MeshRenderPass.h"
#include "EnvironmentSystem.h"
#include "ModuleSamplerHeap.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "Material.h"
#include "Mesh.h"
#include "Application.h"

bool MeshRenderPass::init(ID3D12Device* device){

    if (!m_pipeline.init(device)) {

        LOG("MeshRenderPass: failed to initialise MeshPipeline");
        return false;
    }
    LOG("MeshRenderPass: initialised OK");
    return true;
}

void MeshRenderPass::render(ID3D12GraphicsCommandList* cmd, const std::vector<MeshEntry*>& meshes, D3D12_GPU_VIRTUAL_ADDRESS lightCBAddr, const float viewProj[16], const EnvironmentSystem* env, ModuleSamplerHeap* samplerHeap){

    if (meshes.empty()) return;

    cmd->SetPipelineState(m_pipeline.getPSO());
    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    cmd->SetGraphicsRoot32BitConstants(MeshPipeline::SLOT_VP, 16, viewProj, 0);
    cmd->SetGraphicsRootConstantBufferView(MeshPipeline::SLOT_LIGHT_CB, lightCBAddr);

    if (samplerHeap)
        cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_SAMPLER, samplerHeap->getGPUHandle(m_pipeline.getSamplerType()));

    m_pipeline.bindIBL(cmd, env);

    for (MeshEntry* entry : meshes){

        if (!entry) continue;

        Mesh* mesh = nullptr;
        if (entry->meshRes)
            mesh = entry->meshRes->getMesh();  
        else
            mesh = entry->mesh;               

        if (!mesh) continue;

        cmd->SetGraphicsRoot32BitConstants(MeshPipeline::SLOT_WORLD, 16, entry->worldMatrix, 0);

        if (entry->materialCB){

            cmd->SetGraphicsRootConstantBufferView(
                MeshPipeline::SLOT_MATERIAL_CB,
                entry->materialCB->GetGPUVirtualAddress());
        }

        const Material* mat = nullptr;
        if (entry->materialRes)
            mat = entry->materialRes->getMaterial(); 
        else if (entry->material)
            mat = entry->material;                 

        if (mat) {

            if (mat->hasTexture())
                cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_ALBEDO_TEX, mat->getTextureGPUHandle());

            if (mat->hasNormalMap())
                cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_NORMAL_TEX, mat->getNormalMapGPUHandle());

            if (mat->hasAOMap())
                cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_AO_TEX, mat->getAOMapGPUHandle());

            if (mat->hasEmissive())
                cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_EMISSIVE_TEX, mat->getEmissiveGPUHandle());
        }

        mesh->draw(cmd);
    }
}