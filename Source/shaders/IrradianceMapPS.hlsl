#include "Common.hlsli"
#include "Sampling.hlsli"
#include "ImageBasedLighting.hlsli"

cbuffer Constants : register(b2)
{
    int numSamples;
    int cubemapSize;
    float lodBias;
    int padding; 
};

TextureCube environment : register(t0);
SamplerState samplerState : register(s0);

float4 main(float3 texcoords : TEXCOORD) : SV_Target
{
    float3 irradiance = 0.0;
    float3 normal = normalize(texcoords);
    float3x3 tangentSpace = BuildTangentBasis(normal);

    for (int i = 0; i < numSamples; ++i)
    {
        float2 rand_value = HammersleySample(i, numSamples);
        float3 sample = CosineSampleHemisphere(rand_value[0], rand_value[1]);
        float3 L = mul(sample, tangentSpace);

        float pdf = sample.z / PI;
        float lod = ComputeEnvMapLOD(pdf, numSamples, cubemapSize);

        float3 Li = environment.SampleLevel(samplerState, L, lod + lodBias).rgb;

        irradiance += Li;
    }

    return float4(irradiance * (1.0 / float(numSamples)), 1.0);
}
