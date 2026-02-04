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
    m_data.baseColour = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    m_data.hasColourTexture = FALSE;
}

void Material::load(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model, const char* basePath)
{
    m_name = gltfMaterial.name.empty() ? "Material" : gltfMaterial.name;

    const auto& pbr = gltfMaterial.pbrMetallicRoughness;
    if (pbr.baseColorFactor.size() >= 4)
    {
        m_data.baseColour = XMFLOAT4(
            float(pbr.baseColorFactor[0]),
            float(pbr.baseColorFactor[1]),
            float(pbr.baseColorFactor[2]),
            float(pbr.baseColorFactor[3])
        );
    }

    if (pbr.baseColorTexture.index >= 0 &&
        pbr.baseColorTexture.index < (int)model.textures.size())
    {
        const auto& texture = model.textures[pbr.baseColorTexture.index];
        if (texture.source >= 0 && texture.source < (int)model.images.size())
        {
            const auto& image = model.images[texture.source];
            if (!image.uri.empty())
            {
                ModuleResources* resources = app->getResources();
                std::string fullPath = std::string(basePath) + image.uri;
                m_texture = resources->createTextureFromFile(fullPath, true);

                ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
                if (descriptors && m_texture)
                {
                    auto table = descriptors->allocTable();
                    if (table.isValid()) 
                    {
                        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                        srvDesc.Format = m_texture->GetDesc().Format;
                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                        srvDesc.Texture2D.MipLevels = 1;
                        srvDesc.Texture2D.MostDetailedMip = 0;

                        table.createSRV(m_texture.Get(), &srvDesc, 0);
                        m_textureGPUHandle = table.getGPUHandle();  
                        m_hasTexture = true;
                    }
                }

                m_data.hasColourTexture = TRUE;
            }
        }
    }
    else
    {
        ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
        if (descriptors)
        {
            auto table = descriptors->allocTable();
            if (table.isValid())
            {
                // FIX: Correct parameter order
                table.createNullSRV(D3D12_SRV_DIMENSION_TEXTURE2D,
                    DXGI_FORMAT_R8G8B8A8_UNORM, 0);
                m_textureGPUHandle = table.getGPUHandle(); 
            }
        }
        m_data.hasColourTexture = FALSE;
    }
}