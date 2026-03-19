#include "Globals.h"
#include "EnvironmentGenerator.h"
#include "CommandContext.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"

std::unique_ptr<EnvironmentMap> EnvironmentGenerator::loadCubemap(const std::string& file) {
	auto* d3d12 = app->getD3D12();
	auto* resources = app->getGPUResources();
	auto* shaderDesc = app->getShaderDescriptors();

	if (!d3d12 || !resources || !shaderDesc) return nullptr;

	auto env = std::make_unique<EnvironmentMap>();
	env->cubemap = resources->createTextureFromFile(file, true);

	if (!env->cubemap) {
		LOG("EnvironmentGenerator: failed to load cubemap '%s'", file.c_str());
		return nullptr;
	}
	app->getD3D12()->flush();
	env->srvTable = shaderDesc->allocTable("SkyboxCubemap");
	if (!env->srvTable.isValid()) {
		LOG("EnvironmentGenerator: failed to alloc SRV table");
		return nullptr;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = env->cubemap->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.TextureCube.MipLevels = env->cubemap->GetDesc().MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	env->srvTable.createSRV(env->cubemap.Get(), 0, &srvDesc);

	LOG("EnvironmentGenerator: cubemap loaded '%s'", file.c_str());
	return bakeIBL(d3d12, shaderDesc, std::move(env));
}

std::unique_ptr<EnvironmentMap> EnvironmentGenerator::loadHDR(const std::string& hdrFile, uint32_t cubeFaceSize) {
	auto* d3d12 = app->getD3D12();
	auto* resources = app->getGPUResources();
	auto* shaderDesc = app->getShaderDescriptors();
	auto* samplerHeap = app->getSamplerHeap();

	if (!d3d12 || !resources || !shaderDesc || !samplerHeap) return nullptr;

	LOG("EnvironmentGenerator: loading HDR '%s' (faceSize=%u)...", hdrFile.c_str(), cubeFaceSize);

	CommandContext ctx(d3d12, shaderDesc, samplerHeap);
	if (!ctx.isValid()) return nullptr;

	auto env = std::make_unique<EnvironmentMap>();
	ID3D12Device* device = d3d12->getDevice();

	if (!m_hdrConverter.loadHDRTexture(device, hdrFile, *env)) return nullptr;
	if (!m_hdrConverter.createCubemapResource(device, *env, cubeFaceSize)) return nullptr;

	const uint32_t numMips = m_hdrConverter.getNumMips();

	if (!m_hdrConverter.recordConversion(ctx.cmd(), *env)) return nullptr;
	if (!ctx.submitAndReset("HDR conversion mip0")) return nullptr;
	LOG("EnvironmentGenerator: mip 0 done.");

	for (uint32_t mip = 1; mip < numMips; ++mip) {
		if (!m_hdrConverter.recordMipLevel(device, ctx.cmd(), *env, mip)) return nullptr;
		if (!ctx.submitAndReset("HDR mip blit")) return nullptr;
	}
	LOG("EnvironmentGenerator: mip chain done (%u levels).", numMips);

	if (!m_hdrConverter.finaliseSRV(*env)) return nullptr;

	if (!m_iblGenerator.prepareResources(device, *env)) {
		LOG("EnvironmentGenerator: IBL prepareResources FAILED");
		return nullptr;
	}

	if (!m_iblGenerator.bakeIrradiance(device, ctx.cmd(), *env)) return nullptr;
	if (!ctx.submitAndReset("irradiance")) return nullptr;
	LOG("EnvironmentGenerator: irradiance done.");

	for (uint32_t mip = 0; mip < EnvironmentMap::NUM_ROUGHNESS_LEVELS; ++mip) {
		if (!m_iblGenerator.bakePrefilter(device, ctx.cmd(), *env, mip)) return nullptr;
		if (!ctx.submitAndReset("prefilter")) return nullptr;
	}
	LOG("EnvironmentGenerator: prefilter done.");

	if (!m_iblGenerator.bakeBRDFLut(device, ctx.cmd(), *env)) return nullptr;
	if (!ctx.submit("BRDF LUT")) return nullptr;
	LOG("EnvironmentGenerator: BRDF LUT done.");

	if (!m_iblGenerator.finaliseSRVs(*env)) return nullptr;

	m_iblGenerator.releasePipelines();
	LOG("EnvironmentGenerator: HDR load + IBL bake complete.");
	return env;
}

std::unique_ptr<EnvironmentMap> EnvironmentGenerator::bakeIBL(ModuleD3D12* d3d12, ModuleShaderDescriptors* shaderDesc, std::unique_ptr<EnvironmentMap> env) {
	auto* samplerHeap = app->getSamplerHeap();

	CommandContext ctx(d3d12, shaderDesc, samplerHeap);
	if (!ctx.isValid()) return nullptr;

	LOG("EnvironmentGenerator: starting IBL pre-computation...");

	if (!m_iblGenerator.generate(d3d12->getDevice(), ctx.cmd(), *env)) {
		LOG("EnvironmentGenerator: IBL pre-computation FAILED.");
		return nullptr;
	}

	if (!ctx.submitAndReset("IBL bake")) return nullptr;

	m_iblGenerator.releasePipelines();
	LOG("EnvironmentGenerator: IBL pre-computation done.");
	return env;
}