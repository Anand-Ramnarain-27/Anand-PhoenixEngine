#pragma once

#include "MeshPipeline.h"
#include "MeshEntry.h"
#include "ShaderTableDesc.h"
#include <vector>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class EnvironmentSystem;

struct FrameLightData {
	std::vector<MeshPipeline::GPUDirectionalLight> dirLights;
	std::vector<MeshPipeline::GPUPointLight> pointLights;
	std::vector<MeshPipeline::GPUSpotLight> spotLights;
};

class MeshRenderPass {
public:
	MeshRenderPass() = default;
	~MeshRenderPass() = default;

	bool init(ID3D12Device* device, bool useMSAA = false);

	void render(ID3D12GraphicsCommandList* cmd, const std::vector<MeshEntry*>& meshes,
	            const FrameLightData& lights, const Vector3& cameraPos,
	            const Matrix& viewProj, const EnvironmentSystem* env, int samplerType = 0);

	// Renders with the transparent PSO (alpha blend, depth test, no depth write).
	// Uses the upper half of the instance ring so it can be called after render()
	// in the same frame without slot overlap.
	void renderTransparent(ID3D12GraphicsCommandList* cmd, const std::vector<MeshEntry*>& meshes,
	                       const FrameLightData& lights, const Vector3& cameraPos,
	                       const Matrix& viewProj, const EnvironmentSystem* env, int samplerType = 0);

	MeshPipeline& getPipeline() {
		return m_pipeline;
	}

private:
	bool createUploadBuffers(ID3D12Device* device);
	bool createLightSRVs();
	bool createFallbackTextures(ID3D12Device* device);
	bool createMatTableRing();

	void uploadLights(const FrameLightData& lights);
	void uploadPerFrameCB(const FrameLightData& lights, const Vector3& cameraPos, uint32_t envRoughLevels);
	void writePerDrawCBs(const MeshEntry& entry, const Matrix& viewProj, UINT slot, D3D12_GPU_VIRTUAL_ADDRESS& outMvpVA, D3D12_GPU_VIRTUAL_ADDRESS& outInstVA);

	// Core rendering implementation — selects PSO and uses [slotBase, slotBase+maxSlots) from the ring
	void renderWithPSO(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso,
	                   const std::vector<MeshEntry*>& meshes,
	                   const FrameLightData& lights, const Vector3& cameraPos,
	                   const Matrix& viewProj, const EnvironmentSystem* env,
	                   int samplerType, UINT slotBase, UINT maxSlots);

	MeshPipeline m_pipeline;

	// Ring is split: opaque uses [0, MAX_OPAQUE), transparent uses [MAX_OPAQUE, MAX_INSTANCES)
	static constexpr UINT MAX_OPAQUE = 256;
	static constexpr UINT MAX_TRANSPARENT = 256;
	static constexpr UINT MAX_INSTANCES = MAX_OPAQUE + MAX_TRANSPARENT;

	ComPtr<ID3D12Resource> m_mvpRing;
	void* m_mvpMapped = nullptr;

	ComPtr<ID3D12Resource> m_perFrameCB;
	void* m_perFrameMapped = nullptr;

	ComPtr<ID3D12Resource> m_perInstanceRing;
	void* m_perInstanceMapped = nullptr;

	ComPtr<ID3D12Resource> m_dirLightBuf;
	ComPtr<ID3D12Resource> m_pointLightBuf;
	ComPtr<ID3D12Resource> m_spotLightBuf;
	void* m_dirLightMapped = nullptr;
	void* m_pointLightMapped = nullptr;
	void* m_spotLightMapped = nullptr;

	ShaderTableDesc m_dirLightSRV;
	ShaderTableDesc m_pointLightSRV;
	ShaderTableDesc m_spotLightSRV;

	ComPtr<ID3D12Resource> m_fallbackTex2D;

	ComPtr<ID3D12Resource> m_fallbackCube;
	ShaderTableDesc m_fallbackIrradianceSRV;
	ShaderTableDesc m_fallbackPrefilterSRV;
	ShaderTableDesc m_fallbackBRDFSRV;

	std::vector<ShaderTableDesc> m_matRing;
};
