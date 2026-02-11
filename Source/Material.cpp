#include "Material.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleShaderDescriptors.h"

// Binary layout (must match importer)
struct MaterialBinary
{
    UID materialUID;

    float baseColor[4];
    uint32_t hasTexture;

    UID textureUID; // Phase 4: referenced but not loaded
};

Material::Material()
    : uid(GenerateUID())
{
}

bool Material::loadFromBinary(const char* path)
{
    std::vector<uint8_t> data;
    if (!app->getFileSystem()->Load(path, data))
        return false;

    if (data.size() < sizeof(MaterialBinary))
        return false;

    MaterialBinary bin{};
    memcpy(&bin, data.data(), sizeof(MaterialBinary));

    // Restore UID
    uid = bin.materialUID;

    // Base color
    m_data.baseColour = {
        bin.baseColor[0],
        bin.baseColor[1],
        bin.baseColor[2],
        bin.baseColor[3]
    };

    m_pbrData.diffuseColour = {
        bin.baseColor[0],
        bin.baseColor[1],
        bin.baseColor[2]
    };

    // Texture handling (Phase 4 stub)
    m_hasTexture = bin.hasTexture != 0;
    m_data.hasColourTexture = m_hasTexture ? TRUE : FALSE;
    m_pbrData.hasDiffuseTex = m_hasTexture ? TRUE : FALSE;

    // Phase 4 rule:
    // We do NOT load textures yet ? bind null SRV
    createNullDescriptor();

    return true;
}

void Material::createNullDescriptor()
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    if (!descriptors)
        return;

    m_descriptorTable.reset();
    m_descriptorTable = descriptors->allocTable("Material_Null");

    if (m_descriptorTable.isValid())
    {
        m_descriptorTable.createNullSRV(0);
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE Material::getTextureGPUHandle() const
{
    return m_descriptorTable.getGPUHandle(0);
}
