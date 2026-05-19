#include "Lights.hlsli"
#include "Lighting.hlsli"
#include "ImageBasedLighting.hlsli"
#include "Samplers.hlsli"
#include "Tonemap.hlsli"

cbuffer CbPerFrame : register(b0)
{
    uint     DirLightCount;
    uint     PointLightCount;
    uint     SpotLightCount;
    uint     EnvRoughnessLevels;
    float3   CameraPosition;
    uint     FramePad;
    float4x4 InvViewProj;
};

StructuredBuffer<DirectionalLight> DirLights   : register(t0);
StructuredBuffer<PointLight>       PointLights : register(t1);
StructuredBuffer<SpotLight>        SpotLights  : register(t2);

TextureCube IrradianceMap     : register(t3);
TextureCube PrefilteredEnvMap : register(t4);
Texture2D   BrdfLUT           : register(t5);

Texture2D GBufferAlbedo     : register(t6);
Texture2D GBufferNormalMR   : register(t7);
Texture2D GBufferEmissiveAO : register(t8);
Texture2D GBufferDepth      : register(t9);

// Cigolle et al. octahedral decode: [0,1]^2 -> unit sphere
float3 OctDecode(float2 e)
{
    e = e * 2.0f - 1.0f;
    float3 v = float3(e.x, e.y, 1.0f - abs(e.x) - abs(e.y));
    if (v.z < 0.0f)
        v.xy = (1.0f - abs(v.yx)) * sign(v.xy);
    return normalize(v);
}

// Reconstruct world-space position from UV + depth using the inverse VP matrix
float3 ReconstructWorldPos(float2 uv, float depth)
{
    float2 ndc      = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 clipPos  = float4(ndc, depth, 1.0f);
    float4 worldH   = mul(clipPos, InvViewProj);
    return worldH.xyz / worldH.w;
}

float4 main(float4 svPos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 albedoSample    = GBufferAlbedo.Sample(PointClamp, uv);
    float4 normalMRSample  = GBufferNormalMR.Sample(PointClamp, uv);
    float4 emissAOSample   = GBufferEmissiveAO.Sample(PointClamp, uv);
    float  depth           = GBufferDepth.Sample(PointClamp, uv).r;

    // alpha == 0 means no geometry was written (background pixel)
    if (albedoSample.a < 0.5f)
        discard;

    float3 albedo    = albedoSample.rgb;
    float  metallic  = normalMRSample.b;
    float  roughness = normalMRSample.a;
    float3 emissive  = emissAOSample.rgb;
    float  ao        = emissAOSample.a;

    float3 N        = OctDecode(normalMRSample.rg);
    float3 worldPos = ReconstructWorldPos(uv, depth);
    float3 V        = normalize(CameraPosition - worldPos);

    // Direct lighting
    float3 color = float3(0.0f, 0.0f, 0.0f);

    for (uint i = 0; i < DirLightCount; ++i)
        color += EvaluateDirectionalLight(V, N, DirLights[i], albedo, roughness, metallic);

    for (uint j = 0; j < PointLightCount; ++j)
        color += EvaluatePointLight(V, N, PointLights[j], worldPos, albedo, roughness, metallic);

    for (uint k = 0; k < SpotLightCount; ++k)
        color += EvaluateSpotLight(V, N, SpotLights[k], worldPos, albedo, roughness, metallic);

    // Image-based lighting
    float NdotV    = saturate(dot(N, V));
    float specularAO = saturate(NdotV + ao) - (1.0f - ao);
    color += ComputeIBLLighting(V, N, IrradianceMap, PrefilteredEnvMap, BrdfLUT,
                                 EnvRoughnessLevels, albedo, roughness, metallic,
                                 ao, max(specularAO, 0.0f));

    // Emissive
    color += emissive;

    // Tonemap and gamma-correct
    color = PBRNeutralTonemap(color);
    color = LinearToSRGB(color);

    return float4(color, 1.0f);
}
