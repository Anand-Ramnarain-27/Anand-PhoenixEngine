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

    // Builds the world + normal matrix constants for the vertex shader (b1).
    // - world    is transposed because HLSL expects row-major matrices when
    //            uploaded via 32-bit root constants / memcpy.
    // - normalMat is the inverse-transpose of world, also transposed for HLSL.
    //   (inverse-transpose is the correct transform for normals so that they
    //    stay perpendicular to surfaces under non-uniform scale.)
    static WorldConstants makeWorldConstants(const Matrix& world)
    {
        WorldConstants wc;
        wc.world = world.Transpose();
        Matrix inv;
        world.Invert(inv);
        wc.normalMat = inv.Transpose(); // was: inv — missing the transpose
        return wc;
    }

    // Root signature slot indices - must match MeshPS.hlsl register bindings exactly
    static constexpr UINT SLOT_VP = 0;   // b0: 16 x 32-bit constants (ViewProj)
    static constexpr UINT SLOT_WORLD = 1;   // b1: 32 x 32-bit constants (World + NormalMat)
    static constexpr UINT SLOT_LIGHT_CB = 2;   // b2: CBV (LightCB)
    static constexpr UINT SLOT_MATERIAL_CB = 3;   // b3: CBV (MaterialCB)
    static constexpr UINT SLOT_ALBEDO_TEX = 4;   // t0: SRV
    static constexpr UINT SLOT_SAMPLER = 5;   // s0-s3: Sampler table
    static constexpr UINT SLOT_IRRADIANCE = 6;   // t1: SRV (irradiance cubemap)
    static constexpr UINT SLOT_PREFILTER = 7;   // t2: SRV (prefiltered env cubemap)
    static constexpr UINT SLOT_BRDF_LUT = 8;   // t3: SRV (BRDF integration LUT)
    static constexpr UINT SLOT_NORMAL_TEX = 9;   // t4: SRV
    static constexpr UINT SLOT_AO_TEX = 10;  // t5: SRV
    static constexpr UINT SLOT_EMISSIVE_TEX = 11;  // t6: SRV
    static constexpr UINT SLOT_METALROUGH_TEX = 12; // t7: SRV

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

    // useMSAA must match the render target this pipeline draws into
    bool init(ID3D12Device* device, bool useMSAA = false);

    void bindIBL(ID3D12GraphicsCommandList* cmd, const EnvironmentSystem* env) const;

    ID3D12PipelineState* getPSO()     const { return pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return rootSig.Get(); }

    void setSamplerType(ModuleSamplerHeap::Type type) { m_samplerType = type; }
    ModuleSamplerHeap::Type getSamplerType() const { return m_samplerType; }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device, bool useMSAA);

    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3D12PipelineState> pso;
    ModuleSamplerHeap::Type     m_samplerType = ModuleSamplerHeap::LINEAR_WRAP;
};