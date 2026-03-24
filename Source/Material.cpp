#include "Globals.h"
#include "Material.h"

void Material::setBaseColorTexture(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) {
	m_baseColorTex = tex;
	m_baseColorSRV = srv;
	m_hasBaseColor = true;
	m_data.flags |= MAT_FLAG_BASECOLOR_TEX;
}

void Material::setNormalMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) {
	m_normalTex = tex;
	m_normalSRV = srv;
	m_hasNormal = true;
	m_data.flags |= MAT_FLAG_NORMAL_TEX;
}

void Material::setAOMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) {
	m_aoTex = tex;
	m_aoSRV = srv;
	m_hasAO = true;
	m_data.flags |= MAT_FLAG_OCCLUSION_TEX;
}

void Material::setEmissiveMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) {
	m_emissiveTex = tex;
	m_emissiveSRV = srv;
	m_hasEmissive = true;
	m_data.flags |= MAT_FLAG_EMISSIVE_TEX;
}

void Material::setMetalRoughMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv) {
	m_metalRoughTex = tex;
	m_metalRoughSRV = srv;
	m_hasMetalRough = true;
	m_data.flags |= MAT_FLAG_METALROUGH_TEX;
}