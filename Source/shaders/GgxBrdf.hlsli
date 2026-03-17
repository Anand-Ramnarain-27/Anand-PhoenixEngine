#ifndef _GGX_BRDF_HLSLI_
#define _GGX_BRDF_HLSLI_

#include "Common.hlsli"

float3 DiffuseLambert(float3 albedo)
{
    return albedo / PI;
}

float3 FresnelSchlick(float3 f0, float3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(1.0 - VdotH, 5.0);
}

float FresnelSchlick(float f0, float f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(1.0 - VdotH, 5.0);
}

float NormalDistributionGGX(float alphaRoughness, float NdotH)
{
    float a2    = alphaRoughness * alphaRoughness;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;

    if (denom > 0.0)
        return a2 / (PI * denom * denom);

    return 0.0;
}

float VisibilityGGX(float NdotV, float NdotL, float alphaRoughness)
{
    float a2  = alphaRoughness * alphaRoughness;
    float Vis = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float Lit = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    float sum = Vis + Lit;

    if (sum > 0.0)
        return 0.5 / sum;

    return 0.0;
}

float SpecularGGX(float NdotL, float NdotV, float NdotH, float alphaRoughness)
{
    float D   = NormalDistributionGGX(alphaRoughness, NdotH);
    float Vis = VisibilityGGX(NdotV, NdotL, alphaRoughness);
    return 0.25 * D * Vis;
}

#endif // _GGX_BRDF_HLSLI_
