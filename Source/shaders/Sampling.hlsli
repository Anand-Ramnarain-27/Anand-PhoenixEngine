#ifndef _SAMPLING_HLSLI_
#define _SAMPLING_HLSLI_

#include "Common.hlsli"

float RadicalInverseVanDerCorput(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u)  | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u)  | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u)  | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u)  | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 HammersleySample(uint i, uint count)
{
    return float2(float(i) / float(count), RadicalInverseVanDerCorput(i));
}

float3 CosineSampleHemisphere(float u1, float u2)
{
    float phi = 2.0 * PI * u1;
    float r   = sqrt(u2);

    return float3(r * cos(phi), r * sin(phi), sqrt(1.0 - u2));
}

float3 GGXImportanceSample(in float2 rand, float alphaRoughness)
{
    float a2        = alphaRoughness * alphaRoughness;
    float phi       = 2.0 * PI * rand.x;
    float cosTheta  = sqrt((1.0 - rand.y) / (rand.y * (a2 - 1.0) + 1.0));
    float sinTheta  = sqrt(1.0 - cosTheta * cosTheta);

    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

#endif // _SAMPLING_HLSLI_
