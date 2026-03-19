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
        float2 rand_value = HammersleySample(i, numSamples);
        float3 dir = GGXImportanceSample(rand_value, alphaRoughness);
        
        float3 H = normalize(mul(dir, tangentSpace));
        float3 L = reflect(-V, H);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VdotH = saturate(dot(V, H));
        
        if (NdotL > 0.0)
        {
            // CORRECT PDF: D * NdotH / (4 * VdotH)
            float D = NormalDistributionGGX(alphaRoughness, NdotH);
            float pdf = D * NdotH / (4.0 * VdotH + 1e-6);
            
            float lod = ComputeEnvMapLOD(pdf, numSamples, cubemapSize);
            
            color += skybox.SampleLevel(skyboxSampler, L, lod + lodBias).rgb * NdotL;
            weight += NdotL;
        }
    }

    return float4(color / max(weight, 1e-6), 1.0);
}
