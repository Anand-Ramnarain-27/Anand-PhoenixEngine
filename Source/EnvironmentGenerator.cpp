#include "Globals.h"
#include "EnvironmentGenerator.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

std::unique_ptr<EnvironmentMap>
EnvironmentGenerator::loadCubemap(const std::string& file)
{
    auto* d3d12 = app->getD3D12();
    auto* resources = app->getResources();
    auto* shaderDesc = app->getShaderDescriptors();

    if (!d3d12 || !resources || !shaderDesc) return nullptr;

    auto env = std::make_unique<EnvironmentMap>();

    env->cubemap = resources->createTextureFromFile(file, true /*generateMips*/);
    if (!env->cubemap)
    {
        LOG("EnvironmentGenerator: failed to load cubemap '%s'", file.c_str());
        return nullptr;
    }

    env->srvTable = shaderDesc->allocTable("SkyboxCubemap");
    if (!env->srvTable.isValid())
    {
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

    LOG("EnvironmentGenerator: starting IBL pre-computation...");

    ID3D12Device* device = d3d12->getDevice();

    ComPtr<ID3D12CommandAllocator>    bakeAlloc;
    ComPtr<ID3D12GraphicsCommandList> bakeCmd;

    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&bakeAlloc));
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        bakeAlloc.Get(), nullptr, IID_PPV_ARGS(&bakeCmd));

    ID3D12DescriptorHeap* heaps[] = { shaderDesc->getHeap() };
    bakeCmd->SetDescriptorHeaps(1, heaps);

    bool ok = m_iblGenerator.generate(device, bakeCmd.Get(), *env);

    if (ok)
    {
        ID3D12CommandList* lists[] = { bakeCmd.Get() };
        d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);
        d3d12->flush();  
        m_iblGenerator.releasePipelines();

        LOG("EnvironmentGenerator: IBL pre-computation done.");
    }
    else
    {
        LOG("EnvironmentGenerator: IBL pre-computation FAILED.");
    }

    return env;
}