#include "Globals.h"
#include "EnvironmentGenerator.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"

static bool flushAndReopen(ModuleD3D12* d3d12, ID3D12Device* device, ComPtr<ID3D12CommandAllocator>& alloc, ComPtr<ID3D12GraphicsCommandList>& cmd, ModuleShaderDescriptors* shaderDesc, const char* stage) {

    if (FAILED(cmd->Close())) { 
        LOG("EnvironmentGenerator: cmd->Close() failed at '%s'", stage); 
        return false; 
    }

    ID3D12CommandList* lists[] = { cmd.Get() };
    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);
    d3d12->flush();

    if (FAILED(device->GetDeviceRemovedReason())) { 
        LOG("EnvironmentGenerator: device removed after '%s'!", stage); 
        return false; 
    }

    if (FAILED(alloc->Reset())) 
        return false;

    if (FAILED(cmd->Reset(alloc.Get(), nullptr))) 
        return false;

    ID3D12DescriptorHeap* heaps[] = { shaderDesc->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);

    return true;
}

static bool makeCmd(ID3D12Device* device, ModuleShaderDescriptors* shaderDesc, ComPtr<ID3D12CommandAllocator>& alloc, ComPtr<ID3D12GraphicsCommandList>& cmd) {

    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)))) 
        return false;

    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd)))) 
        return false;

    ID3D12DescriptorHeap* heaps[] = { shaderDesc->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);

    return true;
}

std::unique_ptr<EnvironmentMap> EnvironmentGenerator::loadCubemap(const std::string& file) {

    auto* d3d12 = app->getD3D12();
    auto* resources = app->getGPUResources();
    auto* shaderDesc = app->getShaderDescriptors();

    if (!d3d12 || !resources || !shaderDesc) 
        return nullptr;

    auto env = std::make_unique<EnvironmentMap>();
    env->cubemap = resources->createTextureFromFile(file, true);

    if (!env->cubemap) { 
        LOG("EnvironmentGenerator: failed to load cubemap '%s'", file.c_str()); 
        return nullptr; 
    }

    env->srvTable = shaderDesc->allocTable("SkyboxCubemap");

    if (!env->srvTable.isValid()) { 
        LOG("EnvironmentGenerator: failed to alloc SRV table"); 
        return nullptr; 
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = env->cubemap->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.TextureCube.MipLevels = env->cubemap->GetDesc().MipLevels;
    srvDesc.TextureCube.MostDetailedMip = 0;

    env->srvTable.createSRV(env->cubemap.Get(), 0, &srvDesc);
    LOG("EnvironmentGenerator: cubemap loaded '%s'", file.c_str());

    return bakeIBL(d3d12, shaderDesc, std::move(env));
}

std::unique_ptr<EnvironmentMap> EnvironmentGenerator::loadHDR(const std::string& hdrFile, uint32_t cubeFaceSize) {

    auto* d3d12 = app->getD3D12();
    auto* resources = app->getGPUResources();
    auto* shaderDesc = app->getShaderDescriptors();

    if (!d3d12 || !resources || !shaderDesc) 
        return nullptr;

    LOG("EnvironmentGenerator: loading HDR '%s' (faceSize=%u)...", hdrFile.c_str(), cubeFaceSize);

    ID3D12Device* device = d3d12->getDevice();
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> cmd;

    if (!makeCmd(device, shaderDesc, alloc, cmd)) 
        return nullptr;

    auto env = std::make_unique<EnvironmentMap>();

    if (!m_hdrConverter.loadHDRTexture(device, hdrFile, *env)) 
        return nullptr;

    if (!m_hdrConverter.createCubemapResource(device, *env, cubeFaceSize)) 
        return nullptr;

    const uint32_t numMips = m_hdrConverter.getNumMips();

    if (!m_hdrConverter.recordConversion(cmd.Get(), *env)) 
        return nullptr;

    if (!flushAndReopen(d3d12, device, alloc, cmd, shaderDesc, "HDR conversion mip0")) 
        return nullptr;

    LOG("EnvironmentGenerator: mip 0 done.");

    for (uint32_t mip = 1; mip < numMips; ++mip) {

        if (!m_hdrConverter.recordMipLevel(device, cmd.Get(), *env, mip)) 
            return nullptr;

        if (!flushAndReopen(d3d12, device, alloc, cmd, shaderDesc, "HDR mip blit")) 
            return nullptr;
    }

    LOG("EnvironmentGenerator: mip chain done (%u levels).", numMips);

    if (!m_hdrConverter.finaliseSRV(*env)) 
        return nullptr;

    if (!m_iblGenerator.prepareResources(device, *env)) { 
        LOG("EnvironmentGenerator: IBL prepareResources FAILED"); 
        return nullptr; 
    }

    if (!m_iblGenerator.bakeIrradiance(device, cmd.Get(), *env)) 
        return nullptr;

    if (!flushAndReopen(d3d12, device, alloc, cmd, shaderDesc, "irradiance")) 
        return nullptr;

    LOG("EnvironmentGenerator: irradiance done.");

    for (uint32_t mip = 0; mip < EnvironmentMap::NUM_ROUGHNESS_LEVELS; ++mip) {
        if (!m_iblGenerator.bakePrefilter(device, cmd.Get(), *env, mip)) 
            return nullptr;

        if (!flushAndReopen(d3d12, device, alloc, cmd, shaderDesc, "prefilter")) 
            return nullptr;
    }
    LOG("EnvironmentGenerator: prefilter done.");

    if (!m_iblGenerator.bakeBRDFLut(device, cmd.Get(), *env)) 
        return nullptr;

    if (FAILED(cmd->Close())) { 
        LOG("EnvironmentGenerator: final cmd->Close() failed"); 
        return nullptr; 
    }

    ID3D12CommandList* lists[] = { cmd.Get() };

    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);
    d3d12->flush();

    if (FAILED(device->GetDeviceRemovedReason())) { 
        LOG("EnvironmentGenerator: device removed after BRDF LUT bake!"); 
        return nullptr; 
    }

    LOG("EnvironmentGenerator: BRDF LUT done.");

    if (!m_iblGenerator.finaliseSRVs(*env)) 
        return nullptr;

    m_iblGenerator.releasePipelines();
    LOG("EnvironmentGenerator: HDR load + IBL bake complete.");
    return env;
}

std::unique_ptr<EnvironmentMap> EnvironmentGenerator::bakeIBL(ModuleD3D12* d3d12, ModuleShaderDescriptors* shaderDesc, std::unique_ptr<EnvironmentMap> env) {

    ID3D12Device* device = d3d12->getDevice();
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> cmd;

    if (!makeCmd(device, shaderDesc, alloc, cmd)) 
        return nullptr;

    LOG("EnvironmentGenerator: starting IBL pre-computation...");

    if (!m_iblGenerator.generate(device, cmd.Get(), *env)) { 
        LOG("EnvironmentGenerator: IBL pre-computation FAILED."); 
        return nullptr; 
    }

    cmd->Close();

    ID3D12CommandList* lists[] = { cmd.Get() };
    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);
    d3d12->flush();

    m_iblGenerator.releasePipelines();
    LOG("EnvironmentGenerator: IBL pre-computation done.");
    return env;
}