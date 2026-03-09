#include "Globals.h"
#include "Material.h"

void Material::setBaseColorTexture(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) {
    m_baseColorTex = tex;
    m_baseColorSRV = srv;
    m_hasBaseColor = true;
    m_data.hasBaseColorTexture = 1;
}

void Material::setNormalMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) {
    m_normalTex = tex;
    m_normalSRV = srv;
    m_hasNormal = true;
    m_data.hasNormalMap = 1;
}

void Material::setAOMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) {
    m_aoTex = tex;
    m_aoSRV = srv;
    m_hasAO = true;
    m_data.hasAOMap = 1;
}

void Material::setEmissiveMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) {
    m_emissiveTex = tex;
    m_emissiveSRV = srv;
    m_hasEmissive = true;
    m_data.hasEmissiveMap = 1;
}