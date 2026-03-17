#ifndef _IBL_HLSLI_
#define _IBL_HLSLI_

#include "Common.hlsli"
#include "Samplers.hlsli"

float ComputeEnvMapLOD(float pdf, int sampleCount, int cubemapWidth)
{
    precise float sampleSolidAngle = 1.0 / (float(sampleCount) * pdf + 1e-6);
    precise float texelSolidAngle  = 1.0 / (6.0 * cubemapWidth * cubemapWidth);
    return max(0.5 * log2(sampleSolidAngle / texelSolidAngle), 0.0);
}

float3 SampleDiffuseIBL(float3 N, float3 baseColor, TextureCube irradianceMap)
{
    return irradianceMap.Sample(BilinearClamp, N).rgb * baseColor;
}

void SampleSpecularIBL(in float3 R, in float NdotV, in float roughness, in float roughnessLevels,
                       in TextureCube prefilteredEnvMap, in Texture2D brdfLUT,
                       out float3 scaledRadiance, out float3 biasedRadiance)
{
    float3 radiance = prefilteredEnvMap.SampleLevel(BilinearClamp, R, roughness * (roughnessLevels - 1.0)).rgb;
    float2 fab      = brdfLUT.Sample(BilinearClamp, float2(NdotV, roughness)).rg;

    scaledRadiance  = radiance * fab.x;
    biasedRadiance  = radiance * fab.y;
}

float3 ComputeIBLLighting(in float3 V, in float3 N, in TextureCube irradianceMap, in TextureCube prefilteredEnvMap,
                           in Texture2D brdfLUT, in float roughnessLevels, in float3 baseColor,
                           in float roughness, in float metallic, in float diffuseAO, in float specularAO)
{
    float3 R    = reflect(-V, N);
    float NdotV = saturate(dot(N, V));

    float3 diffuse = SampleDiffuseIBL(N, baseColor, irradianceMap) * diffuseAO;

    float3 scaledRadiance, biasedRadiance;
    SampleSpecularIBL(R, NdotV, roughness, roughnessLevels, prefilteredEnvMap, brdfLUT, scaledRadiance, biasedRadiance);

    float3 metalSpecular      = (baseColor * scaledRadiance + biasedRadiance) * specularAO;
    float3 dielectricSpecular = (0.04 * scaledRadiance    + biasedRadiance) * specularAO;

    return lerp(diffuse + dielectricSpecular, metalSpecular, metallic);
}

#endif // _IBL_HLSLI_
