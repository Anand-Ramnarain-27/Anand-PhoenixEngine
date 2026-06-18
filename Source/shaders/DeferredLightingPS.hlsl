#include "Lights.hlsli"
#include "Lighting.hlsli"
#include "ImageBasedLighting.hlsli"
#include "Samplers.hlsli"
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
};

StructuredBuffer<DirectionalLight> DirLights : register(t0);
StructuredBuffer<PointLight> PointLights : register(t1);
StructuredBuffer<SpotLight> SpotLights : register(t2);

StructuredBuffer<int> PointLightIndices : register(t10);
StructuredBuffer<int> SpotLightIndices : register(t11);

TextureCube IrradianceMap : register(t3);
TextureCube PrefilteredEnvMap : register(t4);
Texture2D BrdfLUT : register(t5);

Texture2D GBufferAlbedo : register(t6);
Texture2D GBufferNormalMR : register(t7);
Texture2D GBufferEmissiveAO : register(t8);
Texture2D GBufferDepth : register(t9);

float3 OctDecode(float2 e){
    e = e * 2.0f - 1.0f;
    float3 v = float3(e.x, e.y, 1.0f - abs(e.x) - abs(e.y));
    if (v.z < 0.0f)
        v.xy = (1.0f - abs(v.yx)) * sign(v.xy);
    return normalize(v);
}

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

    for (uint i = 0; i < DirLightCount; ++i)
        color += EvaluateDirectionalLight(V, N, DirLights[i], albedo, roughness, metallic);

    uint tileIdx = getTileIndex(uint2(svPos.xy));

    for (uint j = 0; j < MAX_LIGHTS_PER_TILE; ++j){
        int pIdx = PointLightIndices[tileIdx * MAX_LIGHTS_PER_TILE + j];
        if (pIdx < 0) break;
        color += EvaluatePointLight(V, N, PointLights[pIdx], worldPos, albedo, roughness, metallic);
    }

    for (uint k = 0; k < MAX_LIGHTS_PER_TILE; ++k){
        int sIdx = SpotLightIndices[tileIdx * MAX_LIGHTS_PER_TILE + k];
        if (sIdx < 0) break;
        color += EvaluateSpotLight(V, N, SpotLights[sIdx], worldPos, albedo, roughness, metallic);
    }

    float NdotV = saturate(dot(N, V));
    float specularAO = saturate(NdotV + ao) - (1.0f - ao);
    color += ComputeIBLLighting(V, N, IrradianceMap, PrefilteredEnvMap, BrdfLUT,
                                 EnvRoughnessLevels, albedo, roughness, metallic,
                                 ao, max(specularAO, 0.0f));

    color += emissive;

    color = PBRNeutralTonemap(color);
    color = LinearToSRGB(color);

    return float4(color, 1.0f);
}
