#include "Common.hlsli"
#include "Sampling.hlsli"
#include "ImageBasedLighting.hlsli"

cbuffer Constants : register(b2)
{
    float roughness; 
    int numSamples;
    int cubemapSize;
    float lodBias;
};

TextureCube environment : register(t0);
SamplerState samplerState : register(s0);

float4 main(float3 texcoords : TEXCOORD) : SV_Target
{
    float3 normal = normalize(texcoords);
    float3x3 tangentSpace = BuildTangentBasis(normal);

    float3 irradiance = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        float2 rand_value = HammersleySample(i, numSamples);
        float3 sampleDir = CosineSampleHemisphere(rand_value.x, rand_value.y);

        float3 L = mul(sampleDir, tangentSpace);

        float pdf = sampleDir.z / PI; 
        float lod = ComputeEnvMapLOD(pdf, numSamples, cubemapSize);

        float3 Li = environment.SampleLevel(samplerState, L, lod + lodBias).rgb;
        irradiance += Li;
    }

    return float4(irradiance * (1.0 / float(numSamples)), 1.0);
}
