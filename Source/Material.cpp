#include "Globals.h"
#include "Material.h"

Material::Material() = default;
Material::~Material() = default;

void Material::setBaseColorTexture(ComPtr<ID3D12Resource> texture, D3D12_GPU_DESCRIPTOR_HANDLE srvHandle)
{
    m_baseColorTexture = texture;
    m_textureSRV = srvHandle;
    m_hasTexture = true;
    m_data.hasBaseColorTexture = 1;
}