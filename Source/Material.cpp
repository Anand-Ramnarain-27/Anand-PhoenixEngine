#include "Globals.h"
#include "Material.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"
#include "ShaderTableDesc.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

Material::Material()
{
    m_data.baseColour = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    m_data.hasColourTexture = FALSE;
}

Material::Material(Material&& other) noexcept
    : m_data(other.m_data)
    , m_texture(std::move(other.m_texture))
    , m_shaderTable(std::move(other.m_shaderTable))
    , m_name(std::move(other.m_name))
    , m_hasTexture(other.m_hasTexture)
    , m_textureGPUHandle(other.m_textureGPUHandle)
{
    other.m_textureGPUHandle = { 0 };
    other.m_hasTexture = false;
}

Material& Material::operator=(Material&& other) noexcept
{
    if (this != &other)
    {
        m_data = other.m_data;
        m_texture = std::move(other.m_texture);
        m_shaderTable = std::move(other.m_shaderTable);
        m_name = std::move(other.m_name);
        m_hasTexture = other.m_hasTexture;
        m_textureGPUHandle = other.m_textureGPUHandle;

        other.m_textureGPUHandle = { 0 };
        other.m_hasTexture = false;
    }
    return *this;
}

Material::~Material() = default;

void Material::load(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model, const char* basePath)
{
    m_name = gltfMaterial.name.empty() ? "Material" : gltfMaterial.name;
    m_hasTexture = false;
    m_textureGPUHandle = { 0 };

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
                    m_shaderTable = descriptors->createTable();

                    if (m_shaderTable)
                    {
                        m_shaderTable.createTexture2DSRV(m_texture.Get(),
                            DXGI_FORMAT_UNKNOWN,  
                            1,                 
                            0,               
                            0);         

                        m_textureGPUHandle = m_shaderTable.getGPUHandle();
                        m_hasTexture = true;
                        m_data.hasColourTexture = TRUE;
                        return;
                    }
                }
            }
        }
    }

    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    if (descriptors)
    {
        m_shaderTable = descriptors->createTable();

        if (m_shaderTable)
        {
            m_shaderTable.createNullSRV(D3D12_SRV_DIMENSION_TEXTURE2D,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                0); 

            m_textureGPUHandle = m_shaderTable.getGPUHandle();
        }
    }

    m_data.hasColourTexture = FALSE;
}