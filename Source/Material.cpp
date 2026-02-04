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
    }

    // Try to load texture
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
                    // NEW: Use value semantics instead of shared_ptr
                    shaderTable = desc->createTable();

                    if (shaderTable)
                    {
                        // NEW: Simpler API call
                        shaderTable.createTexture2DSRV(texture.Get(),
                            DXGI_FORMAT_UNKNOWN,  // Use texture's format
                            1,                    // Mip levels
                            0,                    // Most detailed mip
                            0);                   // Slot 0

                        gpuHandle = shaderTable.getGPUHandle();
                        textureLoaded = true;

                        basicData.hasColorTexture = TRUE;
                        phongData.hasDiffuseTexture = TRUE;
                        return; // Success - we have a texture
                    }
                }
            }
        }
    }

    // Fallback: Create null texture descriptor
    ModuleShaderDescriptors* desc = app->getShaderDescriptors();
    if (desc)
    {
        // NEW: Use value semantics
        shaderTable = desc->createTable();

        if (shaderTable)
        {
            // NEW: Simpler null SRV creation
            shaderTable.createNullSRV(D3D12_SRV_DIMENSION_TEXTURE2D,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                0);  // Slot 0

            gpuHandle = shaderTable.getGPUHandle();
        }
    }
}