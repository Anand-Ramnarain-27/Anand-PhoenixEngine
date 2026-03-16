#pragma once
#include <memory>
#include <string>
#include "IBLGenerator.h"
#include "HDRToCubemapPass.h"
#include "EnvironmentMap.h"

class ModuleD3D12;
class ModuleShaderDescriptors;

class EnvironmentGenerator
{
public:
    std::unique_ptr<EnvironmentMap> loadCubemap(const std::string& file);
    std::unique_ptr<EnvironmentMap> loadHDR(const std::string& hdrFile, uint32_t cubeFaceSize = 2048);

private:
    std::unique_ptr<EnvironmentMap> bakeIBL(ModuleD3D12* d3d12, ModuleShaderDescriptors* shaderDesc, std::unique_ptr<EnvironmentMap> env);

    IBLGenerator m_iblGenerator;
    HDRToCubemapPass m_hdrConverter;
};