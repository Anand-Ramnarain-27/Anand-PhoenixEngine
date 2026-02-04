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

    m_descriptorTable.reset();
    m_hasTexture = false;
    m_texture.Reset();

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

                // Load texture
                m_texture = resources->createTextureFromFile(fullPath, true);

                // Create descriptor table for texture
                ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
                if (descriptors && m_texture)
                {
                    m_descriptorTable = descriptors->allocTable(m_name.c_str());
                    if (m_descriptorTable.isValid())
                    {
                        // Create SRV for the texture
                        m_descriptorTable.createTexture2DSRV(m_texture.Get(), 0);
                        m_hasTexture = true;
                        m_data.hasColourTexture = TRUE;

                        LOG("Material '%s' loaded with texture: %s",
                            m_name.c_str(), fullPath.c_str());
                    }
                }
            }
        }
    }

    if (!m_hasTexture)
    {
        ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
        if (descriptors)
        {
            std::string tableName = m_name + "_Null";
            m_descriptorTable = descriptors->allocTable(tableName.c_str());
            if (m_descriptorTable.isValid())
            {
                m_descriptorTable.createNullSRV(0);
                m_data.hasColourTexture = FALSE;

                LOG("Material '%s' created with null texture", m_name.c_str());
            }
        }
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE Material::getTextureGPUHandle() const
{
    return m_descriptorTable.getGPUHandle(0);
}