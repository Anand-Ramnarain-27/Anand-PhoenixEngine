#include "Globals.h"
#include "Material.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

Material::Material()
{

}

void Material::load(const tinygltf::Material& gltfMat,
    const tinygltf::Model& model,
    const char* basePath)
{
    name = gltfMat.name.empty() ? "Material" : gltfMat.name;

    basicData.hasColorTexture = FALSE;
    phongData.hasDiffuseTexture = FALSE;

    const auto& pbr = gltfMat.pbrMetallicRoughness;
    if (pbr.baseColorFactor.size() >= 4)
    {
        XMFLOAT4 color{
            float(pbr.baseColorFactor[0]),
            float(pbr.baseColorFactor[1]),
            float(pbr.baseColorFactor[2]),
            float(pbr.baseColorFactor[3])
        };
        basicData.color = color;
        phongData.diffuseColor = color;
        phongData.specularColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        pbrPhongData.diffuseColor = color;
        pbrPhongData.F0 = XMFLOAT3(0.04f, 0.04f, 0.04f);
        pbrPhongData.shininess = phongData.shininess;
        pbrPhongData.hasDiffuseTexture = phongData.hasDiffuseTexture;
    }

    if (pbr.baseColorTexture.index >= 0 &&
        pbr.baseColorTexture.index < (int)model.textures.size())
    {
        const auto& tex = model.textures[pbr.baseColorTexture.index];
        if (tex.source >= 0 && tex.source < (int)model.images.size())
        {
            const auto& img = model.images[tex.source];
            if (!img.uri.empty())
            {
                ModuleResources* res = app->getResources();
                std::string fullPath = std::string(basePath) + img.uri;
                texture = res->createTextureFromFile(fullPath, true);

                ModuleShaderDescriptors* desc = app->getShaderDescriptors();
                if (desc && texture)
                {
                    auto table = desc->allocTable("MaterialTexture");
                    if (table)
                    {
                        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                        srv.Format = texture->GetDesc().Format;
                        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                        srv.Texture2D.MipLevels = 1;

                        table->createSRV(0, texture.Get(), &srv);
                        gpuHandle = table->getGPUHandle();
                        textureLoaded = true;

                        basicData.hasColorTexture = TRUE;
                        phongData.hasDiffuseTexture = TRUE;
                        return;
                    }
                }
            }
        }
    }

    ModuleShaderDescriptors* desc = app->getShaderDescriptors();
    if (desc)
    {
        auto table = desc->allocTable("NullTexture");
        if (table)
        {
            table->createNullSRV(0, D3D12_SRV_DIMENSION_TEXTURE2D);
            gpuHandle = table->getGPUHandle();
        }
    }
}
