#ifndef _LIGHTING_HLSLI_
#define _LIGHTING_HLSLI_

#include "Lights.hlsli"
#include "GgxBrdf.hlsli"

float3 EvaluateBRDF(float3 V, float3 N, float3 L, float3 lightColor,
                    float3 baseColor, float roughness, float metallic)
{
    float3 H = normalize(V + L);

    float NdotL = saturate(dot(L, N));
    float VdotH = saturate(dot(V, H));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));

    float3 metalFresnel = FresnelSchlick(baseColor, 1.0, VdotH);
    float dielectricFresnel = FresnelSchlick(0.04, 1.0, VdotH);

    float3 diffuse = DiffuseLambert(baseColor) * NdotL * lightColor;
    float3 specular = SpecularGGX(NdotL, NdotV, NdotH, roughness) * NdotL * lightColor;

    float3 dielectric = diffuse + specular * dielectricFresnel;
    float3 metal = specular * metalFresnel;

    return lerp(dielectric, metal, metallic);
}

float3 EvaluateDirectionalLight(float3 V, float3 N, DirectionalLight light,
                                float3 baseColor, float roughness, float metallic)
{
    float3 L = normalize(-light.Direction);
    return EvaluateBRDF(V, N, L, light.Color * light.Intensity, baseColor, roughness, metallic);
}

float3 EvaluatePointLight(float3 V, float3 N, PointLight light, float3 worldPos,
                          float3 baseColor, float roughness, float metallic)
{
    float3 toLight = light.Position - worldPos;
    float3 L = normalize(toLight);
    float sqDist = dot(toLight, toLight);
    float atten = PointLightAttenuation(sqDist, light.SquaredRadius);

    return EvaluateBRDF(V, N, L, light.Color * light.Intensity, baseColor, roughness, metallic) * atten;
}

float3 EvaluateSpotLight(float3 V, float3 N, SpotLight light, float3 worldPos,
                         float3 baseColor, float roughness, float metallic)
{
    float3 toLight = light.Position - worldPos;
    float3 L = normalize(toLight);
    float projDist = dot(-toLight, light.Direction);
    float atten = PointLightAttenuation(projDist * projDist, light.SquaredRadius);
    float cosAngle = dot(-L, light.Direction);
    atten *= SpotLightAttenuation(cosAngle, light.InnerAngle, light.OuterAngle);

    return EvaluateBRDF(V, N, L, light.Color * light.Intensity, baseColor, roughness, metallic) * atten;
}

#endif // _LIGHTING_HLSLI_
