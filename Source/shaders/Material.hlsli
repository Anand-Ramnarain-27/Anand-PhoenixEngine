#ifndef _MATERIAL_HLSLI_
#define _MATERIAL_HLSLI_

#include "Samplers.hlsli"
#include "Tonemap.hlsli"

#define HAS_BASECOLOUR_TEX        0x1
#define HAS_METALLICROUGHNESS_TEX 0x2
#define HAS_NORMAL_TEX            0x4
#define HAS_COMPRESSED_NORMALS    0x8
#define HAS_OCCLUSION_TEX         0x10
#define HAS_EMISSIVE_TEX          0x20

struct Material
{
    float4 BaseColor;
    float  MetallicFactor;
    float  RoughnessFactor;
    float  NormalScale;
    float  OcclusionStrength;
    float3 EmissiveFactor;
    float  AlphaCutoff;
    uint   Flags;
    uint   Padding;
};

float ComputeSpecularAO(float NdotV, float ao, float roughness)
{
    return clamp(pow(NdotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

void SampleAmbientOcclusion(in Material material, in Texture2D occlusionTex, in float2 uv,
                             in float NdotV, in float NdotR, in float roughness,
                             out float diffuseAO, out float specularAO)
{
    if (material.Flags & HAS_OCCLUSION_TEX)
    {
        float sampledAO = occlusionTex.Sample(BilinearWrap, uv).r;
        diffuseAO = lerp(1.0, sampledAO, material.OcclusionStrength);
        specularAO = ComputeSpecularAO(NdotV, diffuseAO, roughness);
    }
    else
    {
        diffuseAO  = 1.0;
        specularAO = 1.0;
    }

    specularAO *= max(1.0 + NdotR, 1.0);
}

float3 SampleNormal(in Material material, in Texture2D normalTex, in float2 uv,
                    in float3 normal, in float3 tangent, in float3 bitangent)
{
    if (material.Flags & HAS_NORMAL_TEX)
    {
        float3 n = normalTex.Sample(BilinearWrap, uv).xyz * 2.0 - 1.0;

        if (material.Flags & HAS_COMPRESSED_NORMALS)
            n.z = sqrt(1.0 - saturate(dot(n.xy, n.xy)));

        n.xy *= material.NormalScale;
        n     = normalize(n);

        float3x3 TBN = float3x3(tangent, bitangent, normal);
        return mul(n, TBN);
    }

    return normal;
}

float3 SampleEmissive(in Material material, in Texture2D emissiveTex, in float2 uv)
{
    if (material.Flags & HAS_EMISSIVE_TEX)
        return emissiveTex.Sample(BilinearClamp, uv).rgb * material.EmissiveFactor;

    return material.EmissiveFactor;
}

void SampleMetallicRoughness(in Material material, in Texture2D baseColorTex, in Texture2D metallicRoughnessTex,
                              in float2 uv, out float3 baseColor, out float roughness,
                              out float alphaRoughness, out float metallic)
{
    baseColor = material.BaseColor.rgb;

    if (material.Flags & HAS_BASECOLOUR_TEX)
        baseColor *= baseColorTex.Sample(BilinearWrap, uv).rgb;

    float2 mr = float2(material.MetallicFactor, material.RoughnessFactor);

    if (material.Flags & HAS_METALLICROUGHNESS_TEX)
        mr *= metallicRoughnessTex.Sample(BilinearWrap, uv).bg;

    metallic       = mr.x;
    roughness      = mr.y;
    alphaRoughness = roughness * roughness;
}

#endif // _MATERIAL_HLSLI_
