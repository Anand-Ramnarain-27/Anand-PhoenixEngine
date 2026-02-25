#include "Globals.h"
#include "EnvironmentGenerator.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

std::unique_ptr<EnvironmentMap>
EnvironmentGenerator::loadCubemap(const std::string& file)
{
    auto* resources = app->getResources();
    auto* descriptors = app->getShaderDescriptors();

    if (!resources || !descriptors)
        return nullptr;

    auto env = std::make_unique<EnvironmentMap>();

    // Load DDS as cubemap
    env->texture = resources->createTextureFromFile(file, true);

    if (!env->texture)
    {
        LOG("EnvironmentGenerator: Failed to load cubemap %s", file.c_str());
        return nullptr;
    }

    // Allocate descriptor table
    env->srvTable = descriptors->allocTable("EnvironmentCubemap");

    if (!env->srvTable.isValid())
        return nullptr;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = env->texture->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.TextureCube.MipLevels = env->texture->GetDesc().MipLevels;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

    env->srvTable.createSRV(env->texture.Get(), 0, &srvDesc);

    env->gpuHandle = env->srvTable.getGPUHandle(0);

    LOG("EnvironmentGenerator: Loaded cubemap %s", file.c_str());

    return env;
}