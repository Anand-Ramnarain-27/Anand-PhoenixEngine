#include "Globals.h"
#include "MeshRenderPass.h"
#include "EnvironmentSystem.h"
#include "ModuleSamplerHeap.h"
#include "ModuleShaderDescriptors.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "Material.h"
#include "Mesh.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include <d3dx12.h>
#include <algorithm>

static constexpr UINT MAT_SLOT_BASECOLOR = 0;
static constexpr UINT MAT_SLOT_METALROUGH = 1;
static constexpr UINT MAT_SLOT_NORMAL = 2;
static constexpr UINT MAT_SLOT_AO = 3;
static constexpr UINT MAT_SLOT_EMISSIVE = 4;

namespace {
	constexpr UINT cbAlign(UINT b) {
		return (b + 255u) & ~255u;
	}

	ComPtr<ID3D12Resource> makeUploadBuf(ID3D12Device* device, UINT64 bytes, void** mapped, const wchar_t* name) {
		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
		ComPtr<ID3D12Resource> buf;
		HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf));
		if (FAILED(hr)) {
			LOG("MeshRenderPass: upload buf failed 0x%08X", hr);
			return nullptr;
		}
		buf->SetName(name);
		if (mapped) buf->Map(0, nullptr, mapped);
		return buf;
	}

	MeshPipeline::GpuMaterial toGpuMaterial(const Material* mat) {
		MeshPipeline::GpuMaterial gm;
		if (!mat) return gm;
		const Material::Data& d = mat->getData();
		gm.baseColor = d.baseColor;
		gm.metallicFactor = d.metallic;
		gm.roughnessFactor = d.roughness;
		gm.normalScale = d.normalStrength;
		gm.occlusionStrength = d.aoStrength;
		gm.emissiveFactor = d.emissiveFactor;
		gm.alphaCutoff = 0.f;
		gm.flags = 0;
		gm.padding = 0;
		if (mat->hasTexture()) gm.flags |= MeshPipeline::MAT_FLAG_BASECOLOR_TEX;
		if (mat->hasNormalMap()) gm.flags |= MeshPipeline::MAT_FLAG_NORMAL_TEX;
		if (mat->hasAOMap()) gm.flags |= MeshPipeline::MAT_FLAG_OCCLUSION_TEX;
		if (mat->hasEmissive()) gm.flags |= MeshPipeline::MAT_FLAG_EMISSIVE_TEX;
		if (mat->hasMetalRoughMap()) gm.flags |= MeshPipeline::MAT_FLAG_METALROUGH_TEX;
		return gm;
	}

	void makeStructuredSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* buf, UINT numElems, UINT stride) {
		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srv.Format = DXGI_FORMAT_UNKNOWN;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.Buffer.NumElements = numElems;
		srv.Buffer.StructureByteStride = stride;
		table.createSRV(buf, slot, &srv);
	}

	void writeTex2DSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* tex) {
		D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
		sv.Format = tex->GetDesc().Format;
		sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		sv.Texture2D.MipLevels = tex->GetDesc().MipLevels;
		table.createSRV(tex, slot, &sv);
	}

	void writeFallbackTex2DSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* fallback) {
		D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
		sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		sv.Texture2D.MipLevels = 1;
		table.createSRV(fallback, slot, &sv);
	}

	void writeFallbackCubeSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* cube) {
		D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
		sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		sv.TextureCube.MipLevels = 1;
		sv.TextureCube.MostDetailedMip = 0;
		sv.TextureCube.ResourceMinLODClamp = 0.f;
		table.createSRV(cube, slot, &sv);
	}
}

bool MeshRenderPass::init(ID3D12Device* device, bool useMSAA) {
	if (!m_pipeline.init(device, useMSAA)) {
		LOG("MeshRenderPass: pipeline init failed");
		return false;
	}
	if (!createUploadBuffers(device)) return false;
	if (!createLightSRVs()) return false;
	if (!createFallbackTextures(device)) return false;
	if (!createMatTableRing()) return false;
	LOG("MeshRenderPass: init OK");
	return true;
}

bool MeshRenderPass::createUploadBuffers(ID3D12Device* device) {
	const UINT mvpSz = cbAlign(sizeof(MeshPipeline::CbMVP));
	const UINT instSz = cbAlign(sizeof(MeshPipeline::CbPerInstance));

	m_mvpRing = makeUploadBuf(device, (UINT64)mvpSz * MAX_INSTANCES, &m_mvpMapped, L"MeshPass_MVPRing");
	if (!m_mvpRing) return false;

	m_perFrameCB = makeUploadBuf(device, cbAlign(sizeof(MeshPipeline::CbPerFrame)), &m_perFrameMapped, L"MeshPass_PerFrameCB");
	if (!m_perFrameCB) return false;

	m_perInstanceRing = makeUploadBuf(device, (UINT64)instSz * MAX_INSTANCES, &m_perInstanceMapped, L"MeshPass_InstanceRing");
	if (!m_perInstanceRing) return false;

	m_dirLightBuf = makeUploadBuf(device, sizeof(MeshPipeline::GPUDirectionalLight) * MeshPipeline::MAX_DIR_LIGHTS, &m_dirLightMapped, L"MeshPass_DirLights");
	m_pointLightBuf = makeUploadBuf(device, sizeof(MeshPipeline::GPUPointLight) * MeshPipeline::MAX_POINT_LIGHTS, &m_pointLightMapped, L"MeshPass_PointLights");
	m_spotLightBuf = makeUploadBuf(device, sizeof(MeshPipeline::GPUSpotLight) * MeshPipeline::MAX_SPOT_LIGHTS, &m_spotLightMapped, L"MeshPass_SpotLights");

	if (!m_dirLightBuf || !m_pointLightBuf || !m_spotLightBuf) return false;
	return true;
}

bool MeshRenderPass::createLightSRVs() {
	auto* sd = app->getShaderDescriptors();
	m_dirLightSRV = sd->allocTable("MeshPass_DirSRV");
	m_pointLightSRV = sd->allocTable("MeshPass_PointSRV");
	m_spotLightSRV = sd->allocTable("MeshPass_SpotSRV");

	if (!m_dirLightSRV.isValid() || !m_pointLightSRV.isValid() || !m_spotLightSRV.isValid()) {
		LOG("MeshRenderPass: light SRV alloc failed");
		return false;
	}

	makeStructuredSRV(m_dirLightSRV, 0, m_dirLightBuf.Get(), MeshPipeline::MAX_DIR_LIGHTS, sizeof(MeshPipeline::GPUDirectionalLight));
	makeStructuredSRV(m_pointLightSRV, 0, m_pointLightBuf.Get(), MeshPipeline::MAX_POINT_LIGHTS, sizeof(MeshPipeline::GPUPointLight));
	makeStructuredSRV(m_spotLightSRV, 0, m_spotLightBuf.Get(), MeshPipeline::MAX_SPOT_LIGHTS, sizeof(MeshPipeline::GPUSpotLight));
	return true;
}

bool MeshRenderPass::createFallbackTextures(ID3D12Device* device) {
	{
		D3D12_RESOURCE_DESC td = {};
		td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		td.Width = td.Height = 1;
		td.DepthOrArraySize = 1;
		td.MipLevels = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc = { 1, 0 };
		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&m_fallbackTex2D));
		if (FAILED(hr)) {
			LOG("MeshRenderPass: fallback Texture2D failed 0x%08X", hr);
			return false;
		}
		m_fallbackTex2D->SetName(L"MeshPass_Fallback2D");
	}

	{
		D3D12_RESOURCE_DESC td = {};
		td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		td.Width = td.Height = 1;
		td.DepthOrArraySize = 6;
		td.MipLevels = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc = { 1, 0 };
		td.Flags = D3D12_RESOURCE_FLAG_NONE;
		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&m_fallbackCube));
		if (FAILED(hr)) {
			LOG("MeshRenderPass: fallback TextureCube failed 0x%08X", hr);
			return false;
		}
		m_fallbackCube->SetName(L"MeshPass_FallbackCube");

		m_fallbackIrradianceSRV = app->getShaderDescriptors()->allocTable("MeshPass_FallbackIrr");
		m_fallbackPrefilterSRV = app->getShaderDescriptors()->allocTable("MeshPass_FallbackPref");
		m_fallbackBRDFSRV = app->getShaderDescriptors()->allocTable("MeshPass_FallbackBRDF");

		if (!m_fallbackIrradianceSRV.isValid() || !m_fallbackPrefilterSRV.isValid() || !m_fallbackBRDFSRV.isValid()) {
			LOG("MeshRenderPass: fallback IBL SRV alloc failed");
			return false;
		}

		writeFallbackCubeSRV(m_fallbackIrradianceSRV, 0, m_fallbackCube.Get());
		writeFallbackCubeSRV(m_fallbackPrefilterSRV, 0, m_fallbackCube.Get());
		writeFallbackTex2DSRV(m_fallbackBRDFSRV, 0, m_fallbackTex2D.Get());
	}

	return true;
}

bool MeshRenderPass::createMatTableRing() {
	auto* sd = app->getShaderDescriptors();
	m_matRing.reserve(MAX_INSTANCES);

	for (UINT i = 0; i < MAX_INSTANCES; ++i) {
		ShaderTableDesc t = sd->allocTable("MeshPass_MatTex");
		if (!t.isValid()) {
			LOG("MeshRenderPass: mat ring alloc failed at index %u", i);
			return false;
		}
		writeFallbackTex2DSRV(t, MAT_SLOT_BASECOLOR, m_fallbackTex2D.Get());
		writeFallbackTex2DSRV(t, MAT_SLOT_METALROUGH, m_fallbackTex2D.Get());
		writeFallbackTex2DSRV(t, MAT_SLOT_NORMAL, m_fallbackTex2D.Get());
		writeFallbackTex2DSRV(t, MAT_SLOT_AO, m_fallbackTex2D.Get());
		writeFallbackTex2DSRV(t, MAT_SLOT_EMISSIVE, m_fallbackTex2D.Get());
		m_matRing.push_back(std::move(t));
	}
	return true;
}

void MeshRenderPass::uploadLights(const FrameLightData& lights) {
	auto copy = [](void* dst, const void* src, size_t count, size_t stride, size_t maxCount) {
		UINT n = static_cast<UINT>(std::min(count, maxCount));
		if (n > 0) memcpy(dst, src, n * stride);
		};
	copy(m_dirLightMapped, lights.dirLights.data(), lights.dirLights.size(), sizeof(MeshPipeline::GPUDirectionalLight), MeshPipeline::MAX_DIR_LIGHTS);
	copy(m_pointLightMapped, lights.pointLights.data(), lights.pointLights.size(), sizeof(MeshPipeline::GPUPointLight), MeshPipeline::MAX_POINT_LIGHTS);
	copy(m_spotLightMapped, lights.spotLights.data(), lights.spotLights.size(), sizeof(MeshPipeline::GPUSpotLight), MeshPipeline::MAX_SPOT_LIGHTS);
}

void MeshRenderPass::uploadPerFrameCB(const FrameLightData& lights, const Vector3& cameraPos, uint32_t envRoughLevels) {
	MeshPipeline::CbPerFrame cb;
	cb.dirLightCount = static_cast<uint32_t>(std::min(lights.dirLights.size(), (size_t)MeshPipeline::MAX_DIR_LIGHTS));
	cb.pointLightCount = static_cast<uint32_t>(std::min(lights.pointLights.size(), (size_t)MeshPipeline::MAX_POINT_LIGHTS));
	cb.spotLightCount = static_cast<uint32_t>(std::min(lights.spotLights.size(), (size_t)MeshPipeline::MAX_SPOT_LIGHTS));
	cb.envRoughnessLevels = envRoughLevels;
	cb.cameraPosition = cameraPos;
	cb.framePad = 0;
	memcpy(m_perFrameMapped, &cb, sizeof(cb));
}

void MeshRenderPass::writePerDrawCBs(const MeshEntry& entry, const Matrix& viewProj, UINT slot, D3D12_GPU_VIRTUAL_ADDRESS& outMvpVA, D3D12_GPU_VIRTUAL_ADDRESS& outInstVA) {
	const UINT mvpSz = cbAlign(sizeof(MeshPipeline::CbMVP));
	const UINT instSz = cbAlign(sizeof(MeshPipeline::CbPerInstance));

	Matrix world;
	memcpy(&world, entry.worldMatrix, sizeof(float) * 16);

	{
		MeshPipeline::CbMVP mvp;
		mvp.mvp = (world * viewProj).Transpose();
		memcpy(static_cast<char*>(m_mvpMapped) + (UINT64)slot * mvpSz, &mvp, sizeof(mvp));
	}

	{
		MeshPipeline::CbPerInstance inst = {};
		inst.modelMatrix = world.Transpose();
		Matrix inv;
		world.Invert(inv);
		inst.normalMatrix = inv.Transpose();

		const Material* mat = nullptr;
		if (entry.materialRes) mat = entry.materialRes->getMaterial();
		else if (entry.material) mat = entry.material;
		inst.material = toGpuMaterial(mat);

		memcpy(static_cast<char*>(m_perInstanceMapped) + (UINT64)slot * instSz, &inst, sizeof(inst));
	}

	outMvpVA = m_mvpRing->GetGPUVirtualAddress() + (UINT64)slot * mvpSz;
	outInstVA = m_perInstanceRing->GetGPUVirtualAddress() + (UINT64)slot * instSz;
}


void MeshRenderPass::render(ID3D12GraphicsCommandList* cmd, const std::vector<MeshEntry*>& meshes, const FrameLightData& lights, const Vector3& cameraPos, const Matrix& viewProj, const EnvironmentSystem* env, int samplerType) {
	if (meshes.empty()) return;

	uploadLights(lights);

	uint32_t roughLevels = 0;
	if (env && env->hasIBL())
		roughLevels = EnvironmentMap::NUM_ROUGHNESS_LEVELS;

	uploadPerFrameCB(lights, cameraPos, roughLevels);

	cmd->SetPipelineState(m_pipeline.getPSO());
	cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

	auto* samplerHeap = app->getSamplerHeap();
	ID3D12DescriptorHeap* heaps[] = {app->getShaderDescriptors()->getHeap(), samplerHeap->getHeap()};
	cmd->SetDescriptorHeaps(2, heaps);

	cmd->SetGraphicsRootConstantBufferView(MeshPipeline::SLOT_PERFRAME_CB, m_perFrameCB->GetGPUVirtualAddress());

	cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_DIR_LIGHTS, m_dirLightSRV.getGPUHandle(0));
	cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_POINT_LIGHTS, m_pointLightSRV.getGPUHandle(0));
	cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_SPOT_LIGHTS, m_spotLightSRV.getGPUHandle(0));

	if (env && env->hasIBL()) {
		m_pipeline.bindIBL(cmd, env);
	}
	else {
		cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_IRRADIANCE, m_fallbackIrradianceSRV.getGPUHandle(0));
		cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_PREFILTER, m_fallbackPrefilterSRV.getGPUHandle(0));
		cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_BRDF_LUT, m_fallbackBRDFSRV.getGPUHandle(0));
	}

	cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_SAMPLER,
		samplerHeap->getGPUHandle(static_cast<ModuleSamplerHeap::Type>(samplerType)));

	UINT slot = 0;

	for (MeshEntry* entry : meshes) {
		if (!entry) continue;

		Mesh* mesh = entry->meshRes ? entry->meshRes->getMesh() : entry->mesh;
		if (!mesh) continue;

		if (slot >= MAX_INSTANCES) {
			LOG("MeshRenderPass: MAX_INSTANCES exceeded");
			break;
		}

		D3D12_GPU_VIRTUAL_ADDRESS mvpVA;
		D3D12_GPU_VIRTUAL_ADDRESS instVA;
		writePerDrawCBs(*entry, viewProj, slot, mvpVA, instVA);

		cmd->SetGraphicsRootConstantBufferView(MeshPipeline::SLOT_MVP_CB, mvpVA);
		cmd->SetGraphicsRootConstantBufferView(MeshPipeline::SLOT_PERINSTANCE_CB, instVA);

		ShaderTableDesc& matTable = m_matRing[slot];

		writeFallbackTex2DSRV(matTable, MAT_SLOT_BASECOLOR, m_fallbackTex2D.Get());
		writeFallbackTex2DSRV(matTable, MAT_SLOT_METALROUGH, m_fallbackTex2D.Get());
		writeFallbackTex2DSRV(matTable, MAT_SLOT_NORMAL, m_fallbackTex2D.Get());
		writeFallbackTex2DSRV(matTable, MAT_SLOT_AO, m_fallbackTex2D.Get());
		writeFallbackTex2DSRV(matTable, MAT_SLOT_EMISSIVE, m_fallbackTex2D.Get());

		const Material* mat = nullptr;
		if (entry->materialRes) mat = entry->materialRes->getMaterial();
		else if (entry->material) mat = entry->material;

		if (mat) {
			if (mat->hasTexture() && mat->getBaseColorResource()) writeTex2DSRV(matTable, MAT_SLOT_BASECOLOR, mat->getBaseColorResource());
			if (mat->hasMetalRoughMap() && mat->getMetalRoughResource()) writeTex2DSRV(matTable, MAT_SLOT_METALROUGH, mat->getMetalRoughResource());
			if (mat->hasNormalMap() && mat->getNormalMapResource()) writeTex2DSRV(matTable, MAT_SLOT_NORMAL, mat->getNormalMapResource());
			if (mat->hasAOMap() && mat->getAOMapResource()) writeTex2DSRV(matTable, MAT_SLOT_AO, mat->getAOMapResource());
			if (mat->hasEmissive() && mat->getEmissiveResource()) writeTex2DSRV(matTable, MAT_SLOT_EMISSIVE, mat->getEmissiveResource());
		}

		cmd->SetGraphicsRootDescriptorTable(MeshPipeline::SLOT_MAT_TEXTURES, matTable.getGPUHandle(0));

		mesh->draw(cmd);

		++slot;
	}
}