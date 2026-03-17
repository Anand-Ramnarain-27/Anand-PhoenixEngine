#include "GgxBrdf.hlsli"
#include "sampling.hlsli"
#include "ImageBasedLighting.hlsli"

cbuffer Constants : register(b2)
{
    float roughness;
    int numSamples;
    int cubemapSize;
    float lodBias;
};

TextureCube skybox : register(t0);
SamplerState skyboxSampler : register(s0);

float4 main(float3 texcoords : TEXCOORD) : SV_Target
{
    float3 R = normalize(texcoords);
    float3 N = R, V = R;

    float3 color = 0.0;
    float weight = 0.0;
    float3x3 tangentSpace = BuildTangentBasis(N);

    float alphaRoughness = roughness * roughness;

    alphaRoughness = max(alphaRoughness, 0.001); 

    for (int i = 0; i < numSamples; ++i)
    {
        float3 dir = GGXImportanceSample(HammersleySample(i, numSamples), alphaRoughness);
        
        float pdf = NormalDistributionGGX(alphaRoughness, dir.z) / 4.0;
        float lod = ComputeEnvMapLOD(pdf, numSamples, cubemapSize);

        float3 H = normalize(mul(dir, tangentSpace));
        float3 L = reflect(-V, H);
        float NdotL = dot(N, L);
        if (NdotL > 0)
        {
            color += skybox.SampleLevel(skyboxSampler, L, lod + lodBias).rgb * NdotL;
            weight += NdotL;
        }
    }

    return float4(color / weight, 1.0);
}
