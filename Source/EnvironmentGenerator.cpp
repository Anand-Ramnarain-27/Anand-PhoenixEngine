#include "Globals.h"
#include "EnvironmentGenerator.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

std::unique_ptr<EnvironmentMap>
EnvironmentGenerator::LoadCubemap(const std::string& file)
{
    auto* resources = app->getResources();
    auto* shaderDesc = app->getShaderDescriptors();

    if (!resources || !shaderDesc)
        return nullptr;

    auto env = std::make_unique<EnvironmentMap>();

    env->cubemap = resources->createTextureFromFile(file, true);

    if (!env->cubemap)
    {
        LOG("EnvironmentGenerator: Failed to load cubemap %s", file.c_str());
        return nullptr;
    }

    env->srvTable = shaderDesc->allocTable("SkyboxCubemap");

    if (!env->srvTable.isValid())
        return nullptr;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = env->cubemap->GetDesc().Format;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.TextureCube.MipLevels = env->cubemap->GetDesc().MipLevels;
    desc.TextureCube.MostDetailedMip = 0;
    desc.TextureCube.ResourceMinLODClamp = 0.0f;

    env->srvTable.createSRV(env->cubemap.Get(), 0, &desc);

    env->gpuHandle = env->srvTable.getGPUHandle(0);

    LOG("EnvironmentGenerator: Loaded cubemap %s", file.c_str());

    return env;
}