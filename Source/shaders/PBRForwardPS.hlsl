#include "ForwardPass.hlsli"
#include "Lighting.hlsli"
#include "Tonemap.hlsli"
#include "ImageBasedLighting.hlsli"

#define VARIANCE  0.3
#define THRESHOLD 0.2

float getGeometricSpecularAA(float3 N, float roughness)
{
    float3 ndx = ddx(N);
    float3 ndy = ddy(N);
    float curvature = max(dot(ndx, ndx), dot(ndy, ndy));
    float geomRoughnessOffset = pow(curvature, 0.333) * VARIANCE;
    geomRoughnessOffset = min(geomRoughnessOffset, THRESHOLD);
    return saturate(roughness + geomRoughnessOffset);
}

float4 main(
    float3 worldPos : POSITION,
    float2 texCoord : TEXCOORD,
    float3 normal : NORMAL0,
    float4 tangent : TANGENT) : SV_TARGET 
{
    float3 V = normalize(CameraPosition - worldPos);
    float3 N = normalize(normal);

    float3 baseColour;
    float roughness;
    float alphaRoughness;
    float metallic;
    SampleMetallicRoughness(InstanceMaterial, BaseColorTex, MetallicRoughnessTex,
                            texCoord, baseColour, roughness, alphaRoughness, metallic);

    roughness = getGeometricSpecularAA(N, roughness);
    alphaRoughness = roughness * roughness;

    float3 T = normalize(tangent.xyz);
    float3 B = normalize(cross(N, T) * tangent.w);
    N = SampleNormal(InstanceMaterial, NormalTex, texCoord, N, T, B);

    float NdotV = saturate(dot(N, V));
    float3 R = reflect(-V, N);
    float NdotR = saturate(dot(N, R));

    float diffuseAO, specularAO;
    SampleAmbientOcclusion(InstanceMaterial, OcclusionTex, texCoord,
                           NdotV, NdotR, roughness, diffuseAO, specularAO);

    float3 colour = ComputeIBLLighting(V, N,
                                       IrradianceMap, PrefilteredEnvMap, BrdfLUT,
                                       EnvRoughnessLevels,
                                       baseColour, roughness, metallic,
                                       diffuseAO, specularAO);

    if (DirLightCount == 0 && PointLightCount == 0 && SpotLightCount == 0 && EnvRoughnessLevels == 0)
        colour = baseColour * 0.03;

    for (uint i = 0; i < DirLightCount; i++)
        colour += EvaluateDirectionalLight(V, N, DirLights[i],
                                           baseColour, alphaRoughness, metallic);

    for (uint i = 0; i < PointLightCount; i++)
        colour += EvaluatePointLight(V, N, PointLights[i], worldPos,
                                     baseColour, alphaRoughness, metallic);

    for (uint i = 0; i < SpotLightCount; i++)
        colour += EvaluateSpotLight(V, N, SpotLights[i], worldPos,
                                    baseColour, alphaRoughness, metallic);

    colour += SampleEmissive(InstanceMaterial, EmissiveTex, texCoord);

    float3 ldr = PBRNeutralTonemap(colour);
    return float4(LinearToSRGB(ldr), 1.0);
}
