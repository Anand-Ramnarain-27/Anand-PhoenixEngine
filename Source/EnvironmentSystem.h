#pragma once
#include <memory>
#include <string>
#include "IBLGenerator.h"
#include "HDRToCubemapPass.h"
#include "EnvironmentMap.h"
#include "SkyboxRenderer.h"

class ModuleD3D12;
class ModuleShaderDescriptors;

class EnvironmentSystem {
public:
	bool init(ID3D12Device* device, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, bool useMSAA);
	void load(const std::string& file);
	void loadHDR(const std::string& hdrFile, uint32_t cubeFaceSize = 2048);
	void render(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& projection);

	const EnvironmentMap* getEnvironmentMap() const{
		return m_environment.get();
	}

	bool hasIBL() const{
		return m_environment && m_environment->hasIBL();
	}

	bool isLoaded() const{
		return m_environment && m_environment->isValid();
	}

private:
	std::unique_ptr<EnvironmentMap> loadCubemap(const std::string& file);
	std::unique_ptr<EnvironmentMap> loadHDRMap(const std::string& hdrFile, uint32_t cubeFaceSize = 2048);
	std::unique_ptr<EnvironmentMap> bakeIBL(ModuleD3D12* d3d12, ModuleShaderDescriptors* shaderDesc, std::unique_ptr<EnvironmentMap> env);

	IBLGenerator m_iblGenerator;
	HDRToCubemapPass m_hdrConverter;
	SkyboxRenderer m_renderer;
	std::unique_ptr<EnvironmentMap> m_environment;
};
