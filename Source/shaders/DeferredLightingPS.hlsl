#include "Lighting.hlsli"
#include "Lights.hlsli"
#include "ImageBasedLighting.hlsli"  // This already includes Samplers.hlsli
#include "Tonemap.hlsli"
 
Texture2D gAlbedo : register(t0);
Texture2D gNormalMR : register(t1);
Texture2D gEmissive : register(t2);
Texture2D gDepth : register(t3);
 
TextureCube IrradianceMap : register(t4);
TextureCube PrefilteredEnvMap : register(t5);
Texture2D BrdfLUT : register(t6);

// REMOVE these - they conflict with Samplers.hlsli
// SamplerState pointSampler : register(s0);
// SamplerState linearSampler : register(s1);

cbuffer DeferredLightCB : register(b0)
{
    float4x4 InvViewProj;
    float3 CameraPosition;
    uint DirLightCount;
    uint PointLightCount;
    uint SpotLightCount;
    uint EnvRoughnessLevels;
    float pad;
};

StructuredBuffer<DirectionalLight> DirLights : register(t7);
StructuredBuffer<PointLight> PointLights : register(t8);
StructuredBuffer<SpotLight> SpotLights : register(t9);
 
float3 ReconstructWorldPos(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 worldPos = mul(clipPos, InvViewProj);
    return worldPos.xyz / worldPos.w;
}

float4 main(float2 texCoord : TEXCOORD) : SV_Target
{
    // Use samplers from Samplers.hlsli
    // PointClamp for G-Buffer reads (no filtering)
    float4 albedoSample = gAlbedo.Sample(PointClamp, texCoord);
    float4 normalMRSample = gNormalMR.Sample(PointClamp, texCoord);
    float4 emissiveAO = gEmissive.Sample(PointClamp, texCoord);
    float depth = gDepth.Sample(PointClamp, texCoord).r;

    // Skip skybox pixels
    if (depth >= 0.9999)
        return float4(0, 0, 0, 0);
     
    float3 baseColour = albedoSample.rgb;
    float metallic = albedoSample.a;
    float3 N = normalize(normalMRSample.rgb * 2.0 - 1.0);
    float roughness = normalMRSample.a;
    float ao = emissiveAO.a;
    float3 emissive = emissiveAO.rgb;
    float3 worldPos = ReconstructWorldPos(texCoord, depth);
     
    float3 V = normalize(CameraPosition - worldPos);
    float NdotV = saturate(dot(N, V));
    float3 R = reflect(-V, N);
    float NdotR = saturate(dot(N, R));
    float alphaRough = roughness * roughness;

    float specularAO = saturate(
        pow(NdotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao);

    // Use BilinearWrap from Samplers.hlsli for IBL
    float3 colour = ComputeIBLLighting(
        V, N, IrradianceMap, PrefilteredEnvMap, BrdfLUT,
        EnvRoughnessLevels, baseColour, roughness, metallic, ao, specularAO);

    for (uint i = 0; i < DirLightCount; i++)
        colour += EvaluateDirectionalLight(
            V, N, DirLights[i], baseColour, alphaRough, metallic);

    for (uint i = 0; i < PointLightCount; i++)
        colour += EvaluatePointLight(
            V, N, PointLights[i], worldPos, baseColour, alphaRough, metallic);

    for (uint i = 0; i < SpotLightCount; i++)
        colour += EvaluateSpotLight(
            V, N, SpotLights[i], worldPos, baseColour, alphaRough, metallic);

    colour += emissive;

    float3 ldr = PBRNeutralTonemap(colour);
    return float4(LinearToSRGB(ldr), 1.0);
}