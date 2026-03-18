#include "Common.hlsli"
#include "Sampling.hlsli"

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
    
    [loop]
    for (int i = 0; i < numSamples; ++i)
    {
        float2 rand_value = HammersleySample(i, numSamples);
        float3 sampleDir = CosineSampleHemisphere(rand_value.x, rand_value.y);
        
        float3 L = mul(sampleDir, tangentSpace);

        float NdotL = saturate(dot(normal, L));
        if (NdotL > 0.0)
        {
            float3 Li = environment.SampleLevel(samplerState, L, 0).rgb;
            irradiance += Li * NdotL;
        }
    }
    
    irradiance *= (1.0 / float(numSamples));

    return float4(irradiance, 1.0);
}