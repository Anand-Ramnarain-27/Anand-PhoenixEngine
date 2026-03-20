#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include "FaceCB.h"

using Microsoft::WRL::ComPtr;

class EnvironmentMap;

class IBLGenerator {
public:
	bool generate(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env);

	bool prepareResources(ID3D12Device* device, EnvironmentMap& env);
	bool bakeIrradiance(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env);
	bool bakePrefilter(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env, uint32_t mipIndex);
	bool bakeBRDFLut(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env);
	bool finaliseSRVs(EnvironmentMap& env);

	void releasePipelines() {
		m_irradianceRS.Reset();
		m_irradiancePSO.Reset();
		m_prefilterRS.Reset();
		m_prefilterPSO.Reset();
		m_brdfRS.Reset();
		m_brdfPSO.Reset();
	}

private:
	struct alignas(256) PassCB {
		float roughness = 0.0f;
		int numSamples = 512;
		int cubemapSize = 2048;
		float lodBias = 0.0f;
	};

	void renderCubeFace(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, ID3D12Resource* target, uint32_t faceIndex, uint32_t mipLevel, uint32_t totalMips, uint32_t baseFaceSize, float roughness, ID3D12RootSignature* rs, ID3D12PipelineState* pso, D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV, DXGI_FORMAT rtvFmt, int numSamples, int cubemapSize);

	bool ensureGeometry(ID3D12Device* device);
	bool ensureFaceCB(ID3D12Device* device);
	bool ensurePassCB(ID3D12Device* device);

	ComPtr<ID3D12Resource> m_cubeVB;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
	bool m_geometryReady = false;

	// FIX: one slot per face so each draw command reads its own VP matrix.
	// FaceCB is alignas(256), so slot N is at offset N * sizeof(FaceCB) — always CBV-aligned.
	static constexpr uint32_t kNumFaces = 6;
	ComPtr<ID3D12Resource> m_faceCB;           // single buffer, 6 * sizeof(FaceCB) bytes
	FaceCB* m_faceCBPtr = nullptr;             // base pointer; face N -> m_faceCBPtr[N]

	// PassCB is the same for all faces within a mip, but allocate 6 slots anyway so
	// bakePrefilter (called per-mip with submit+reset in between) stays consistent.
	ComPtr<ID3D12Resource> m_passCB;           // single buffer, 6 * sizeof(PassCB) bytes
	PassCB* m_passCBPtr = nullptr;             // base pointer; face N -> m_passCBPtr[N]

	ComPtr<ID3D12RootSignature> m_irradianceRS;
	ComPtr<ID3D12RootSignature> m_prefilterRS;
	ComPtr<ID3D12RootSignature> m_brdfRS;
	ComPtr<ID3D12PipelineState> m_irradiancePSO;
	ComPtr<ID3D12PipelineState> m_prefilterPSO;
	ComPtr<ID3D12PipelineState> m_brdfPSO;
};