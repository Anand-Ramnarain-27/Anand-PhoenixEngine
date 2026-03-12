#pragma once

#include "Module.h"
#include "ModuleSamplerHeap.h"
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class EnvironmentSystem;

class MeshPipeline
{
public:
    static constexpr UINT MAX_DIR_LIGHTS = 4;
    static constexpr UINT MAX_POINT_LIGHTS = 32;
    static constexpr UINT MAX_SPOT_LIGHTS = 16;

    struct WorldConstants
    {
        Matrix world;
        Matrix normalMat; 
    };

    static WorldConstants makeWorldConstants(const Matrix& world)
    {
        WorldConstants wc;
        wc.world = world;
        Matrix inv;
        world.Invert(inv);
        wc.normalMat = inv;
        return wc;
    }

    static constexpr UINT SLOT_VP = 0;
    static constexpr UINT SLOT_WORLD = 1;     
    static constexpr UINT SLOT_LIGHT_CB = 2;
    static constexpr UINT SLOT_MATERIAL_CB = 3;
    static constexpr UINT SLOT_ALBEDO_TEX = 4;
    static constexpr UINT SLOT_SAMPLER = 5;
    static constexpr UINT SLOT_IRRADIANCE = 6;
    static constexpr UINT SLOT_PREFILTER = 7;
    static constexpr UINT SLOT_BRDF_LUT = 8;
    static constexpr UINT SLOT_NORMAL_TEX = 9;
    static constexpr UINT SLOT_AO_TEX = 10;
    static constexpr UINT SLOT_EMISSIVE_TEX = 11;
    static constexpr UINT SLOT_METALROUGH_TEX = 12;

    struct GPUDirectionalLight {
        Vector3 direction; float intensity;
        Vector3 color;     float pad;
    };
    struct GPUPointLight {
        Vector3 position;  float sqRadius;
        Vector3 color;     float intensity;
    };
    struct GPUSpotLight {
        Vector3 position;  float sqRadius;
        Vector3 direction; float innerCos;
        Vector3 color;     float outerCos;
        float   intensity; float pad0;
        Vector2 pad;
    };

    struct LightCB {
        Vector3  ambientColor;
        float    ambientIntensity;
        Vector3  viewPos;

        float    pad0;
        uint32_t numDirLights;
        uint32_t numPointLights;
        uint32_t numSpotLights;
        uint32_t iblEnabled;
        float    numRoughnessLevels;
        float    pad1[3];

        GPUDirectionalLight dirLights[MAX_DIR_LIGHTS];
        GPUPointLight       pointLights[MAX_POINT_LIGHTS];
        GPUSpotLight        spotLights[MAX_SPOT_LIGHTS];
    };

    bool init(ID3D12Device* device);

    void bindIBL(ID3D12GraphicsCommandList* cmd, const EnvironmentSystem* env) const;

    ID3D12PipelineState* getPSO()     const { return pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return rootSig.Get(); }

    void setSamplerType(ModuleSamplerHeap::Type type) { m_samplerType = type; }
    ModuleSamplerHeap::Type getSamplerType() const { return m_samplerType; }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3D12PipelineState> pso;
    ModuleSamplerHeap::Type     m_samplerType = ModuleSamplerHeap::LINEAR_WRAP;
};