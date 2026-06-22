#include "GBufferPass.hlsli"
#include "Lights.hlsli"
#include "Lighting.hlsli"
#include "ImageBasedLighting.hlsli"
#include "Samplers.hlsli"
#include "Shadows.hlsli"
#include "Tonemap.hlsli"

#define TILE_SIZE        16
#define MAX_LIGHTS_PER_TILE 64

cbuffer CbPerFrame : register(b0){
    uint DirLightCount;
    uint PointLightCount;
    uint SpotLightCount;
    uint EnvRoughnessLevels;
    float3 CameraPosition;
    uint FramePad;
    float4x4 InvViewProj;
    uint ViewportWidth;
    uint ViewportHeight;
    uint2 LightCullingPad;
    float4x4 LightViewProj[MAX_CASCADES];
    float4 ShadowParams0;
    float4 ShadowParams1;
    float4 ShadowParams2;
    float3 ShadowLightDir;
    float ShadowPad;
    float4x4 SpotViewProj;
    float4 SpotShadowParams;
    float4 SpotShadowPos;
    float4 PointShadowParams;
    float4 PointShadowPos;
};

StructuredBuffer<DirectionalLight> DirLights : register(t0);
StructuredBuffer<PointLight> PointLights : register(t1);
StructuredBuffer<SpotLight> SpotLights : register(t2);

StructuredBuffer<int> PointLightIndices : register(t10);
StructuredBuffer<int> SpotLightIndices : register(t11);

Texture2DArray ShadowMap : register(t12);
Texture2DArray ShadowMoments : register(t13);
Texture2D SpotShadowMap : register(t14);
TextureCube PointShadowMap : register(t15);

cbuffer GpuVP : register(b1){
    row_major float4x4 GpuViewProj;
};

TextureCube IrradianceMap : register(t3);
TextureCube PrefilteredEnvMap : register(t4);
Texture2D BrdfLUT : register(t5);

Texture2D GBufferAlbedo : register(t6);
Texture2D GBufferNormalMR : register(t7);
Texture2D GBufferEmissiveAO : register(t8);
Texture2D GBufferDepth : register(t9);

float3 ReconstructWorldPos(float2 uv, float depth){
    float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 clipPos = float4(ndc, depth, 1.0f);
    float4 worldH = mul(clipPos, InvViewProj);
    return worldH.xyz / worldH.w;
}

uint getTileIndex(uint2 pixelPos){
    uint numTilesX = (ViewportWidth + TILE_SIZE - 1) / TILE_SIZE;
    return (pixelPos.y / TILE_SIZE) * numTilesX + (pixelPos.x / TILE_SIZE);
}

float4 main(float4 svPos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float4 albedoSample = GBufferAlbedo.Sample(PointClamp, uv);
    float4 normalMRSample = GBufferNormalMR.Sample(PointClamp, uv);
    float4 emissAOSample = GBufferEmissiveAO.Sample(PointClamp, uv);
    float depth = GBufferDepth.Sample(PointClamp, uv).r;

    if (albedoSample.a < 0.5f)
        discard;

    float3 albedo = albedoSample.rgb;
    float metallic = normalMRSample.b;
    float roughness = normalMRSample.a;
    float3 emissive = emissAOSample.rgb;
    float ao = emissAOSample.a;

    float3 N = OctDecode(normalMRSample.rg);
    float3 worldPos = ReconstructWorldPos(uv, depth);
    float3 V = normalize(CameraPosition - worldPos);

    float3 color = float3(0.0f, 0.0f, 0.0f);

    float shadowFactor = 1.0f;
    int shadowCascade = -1;
    if (ShadowParams1.x > 0.5f){
        int mode = (int)ShadowParams1.y;
        int cascadeCount = (int)ShadowParams1.z;
        if (ShadowParams2.z > 0.5f){
            float3 s = WorldToShadowUV(worldPos, GpuViewProj);
            if (InsideCascade(s)){
                shadowCascade = 0;
                shadowFactor = SampleShadowArrayPCF(ShadowMap, ShadowCmp, s.xy,
                                                    s.z - ShadowParams0.x, 0,
                                                    ShadowParams0.z, ShadowParams0.w);
            }
        } else if (mode == 0){
            shadowFactor = ComputeCascadeShadow(ShadowMap, ShadowCmp, worldPos, LightViewProj,
                                                cascadeCount, ShadowParams0.x,
                                                ShadowParams0.z, ShadowParams0.w, shadowCascade);
        } else {
            shadowFactor = ComputeCascadeShadowMoments(ShadowMoments, BilinearClamp, worldPos,
                                                       LightViewProj, cascadeCount, ShadowParams0.x,
                                                       ShadowParams2.x, mode == 2 ? 1 : 0,
                                                       ShadowParams2.y, shadowCascade);
        }
    }

    for (uint i = 0; i < DirLightCount; ++i){
        float3 dirContribution = EvaluateDirectionalLight(V, N, DirLights[i], albedo, roughness, metallic);
        if (i == 0) dirContribution *= shadowFactor;
        color += dirContribution;
    }

    uint tileIdx = getTileIndex(uint2(svPos.xy));

    for (uint j = 0; j < MAX_LIGHTS_PER_TILE; ++j){
        int pIdx = PointLightIndices[tileIdx * MAX_LIGHTS_PER_TILE + j];
        if (pIdx < 0) break;
        float3 pc = EvaluatePointLight(V, N, PointLights[pIdx], worldPos, albedo, roughness, metallic);
        if (PointShadowParams.x > 0.5f &&
            distance(PointLights[pIdx].Position, PointShadowPos.xyz) < 0.05f)
            pc *= SamplePointShadow(PointShadowMap, BilinearClamp, worldPos,
                                    PointShadowPos.xyz, PointShadowParams.z, PointShadowParams.y);
        color += pc;
    }

    for (uint k = 0; k < MAX_LIGHTS_PER_TILE; ++k){
        int sIdx = SpotLightIndices[tileIdx * MAX_LIGHTS_PER_TILE + k];
        if (sIdx < 0) break;
        float3 spc = EvaluateSpotLight(V, N, SpotLights[sIdx], worldPos, albedo, roughness, metallic);
        if (SpotShadowParams.x > 0.5f &&
            distance(SpotLights[sIdx].Position, SpotShadowPos.xyz) < 0.05f)
            spc *= SampleSpotShadow(SpotShadowMap, ShadowCmp, worldPos, SpotViewProj,
                                    SpotShadowParams.y, SpotShadowParams.z, SpotShadowParams.w);
        color += spc;
    }

    float NdotV = saturate(dot(N, V));
    float specularAO = saturate(NdotV + ao) - (1.0f - ao);
    float3 ibl = ComputeIBLLighting(V, N, IrradianceMap, PrefilteredEnvMap, BrdfLUT,
                                    EnvRoughnessLevels, albedo, roughness, metallic,
                                    ao, max(specularAO, 0.0f));
    ibl *= lerp(1.0f - ShadowParams2.w, 1.0f, shadowFactor);
    color += ibl;

    color += emissive;

    if (ShadowParams1.w > 0.5f && shadowCascade >= 0)
        color *= CascadeDebugTint(shadowCascade);

    color = PBRNeutralTonemap(color);
    color = LinearToSRGB(color);

    return float4(color, 1.0f);
}
