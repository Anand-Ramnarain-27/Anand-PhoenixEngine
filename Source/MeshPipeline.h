#pragma once

#include "Module.h"
#include "ModuleSamplerHeap.h"
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class EnvironmentSystem;

class MeshPipeline {
public:
	static constexpr UINT MAX_DIR_LIGHTS = 4;
	static constexpr UINT MAX_POINT_LIGHTS = 32;
	static constexpr UINT MAX_SPOT_LIGHTS = 16;

	struct CbMVP {
		Matrix mvp;
	};

	struct CbPerFrame {
		uint32_t dirLightCount;
		uint32_t pointLightCount;
		uint32_t spotLightCount;
		uint32_t envRoughnessLevels;
		Vector3 cameraPosition;
		uint32_t framePad;
	};

	static constexpr uint32_t MAT_FLAG_BASECOLOR_TEX = 0x01;
	static constexpr uint32_t MAT_FLAG_METALROUGH_TEX = 0x02;
	static constexpr uint32_t MAT_FLAG_NORMAL_TEX = 0x04;
	static constexpr uint32_t MAT_FLAG_COMPRESSED_NORMS = 0x08;
	static constexpr uint32_t MAT_FLAG_OCCLUSION_TEX = 0x10;
	static constexpr uint32_t MAT_FLAG_EMISSIVE_TEX = 0x20;

	struct GpuMaterial {
		Vector4 baseColor = { 1.f, 1.f, 1.f, 1.f };
		float metallicFactor = 0.f;
		float roughnessFactor = 0.5f;
		float normalScale = 1.f;
		float occlusionStrength = 1.f;
		Vector3 emissiveFactor = { 0.f, 0.f, 0.f };
		float alphaCutoff = 0.f;
		uint32_t flags = 0;
		uint32_t padding = 0;
	};

	struct CbPerInstance {
		Matrix modelMatrix;
		Matrix normalMatrix;
		GpuMaterial material;
	};

	struct GPUDirectionalLight {
		Vector3 direction;
		float intensity;
		Vector3 color;
		float _pad;
	};

	struct GPUPointLight {
		Vector3 position;
		float squaredRadius;
		Vector3 color;
		float intensity;
	};

	struct GPUSpotLight {
		Vector3 direction;
		float squaredRadius;
		Vector3 position;
		float innerAngle;
		Vector3 color;
		float outerAngle;
		float intensity;
		float _pad[3];
	};

	static constexpr UINT SLOT_MVP_CB = 0;
	static constexpr UINT SLOT_PERFRAME_CB = 1;
	static constexpr UINT SLOT_PERINSTANCE_CB = 2;
	static constexpr UINT SLOT_DIR_LIGHTS = 3;
	static constexpr UINT SLOT_POINT_LIGHTS = 4;
	static constexpr UINT SLOT_SPOT_LIGHTS = 5;
	static constexpr UINT SLOT_IRRADIANCE = 6;
	static constexpr UINT SLOT_PREFILTER = 7;
	static constexpr UINT SLOT_BRDF_LUT = 8;
	static constexpr UINT SLOT_MAT_TEXTURES = 9;
	static constexpr UINT SLOT_SAMPLER = 10;

	bool init(ID3D12Device* device, bool useMSAA = false);
	void bindIBL(ID3D12GraphicsCommandList* cmd, const EnvironmentSystem* env) const;

	ID3D12PipelineState* getPSO() const {
		return m_pso.Get();
	}

	ID3D12RootSignature* getRootSig() const {
		return m_rootSig.Get();
	}

private:
	bool createRootSignature(ID3D12Device* device);
	bool createPSO(ID3D12Device* device, bool useMSAA);

	ComPtr<ID3D12RootSignature> m_rootSig;
	ComPtr<ID3D12PipelineState> m_pso;
};