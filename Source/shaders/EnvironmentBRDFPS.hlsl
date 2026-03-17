#include "GgxBrdf.hlsli"
#include "Sampling.hlsli"

#define NUM_SAMPLES 1024

float4 main(in float2 uv : TEXCOORD) : SV_Target
{
    float NdotV = uv.x;
    float alphaRoughness = uv.y * uv.y;

    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV); 
    V.y = 0.0;
    V.z = NdotV; 

    float3 N = float3(0.0, 0.0, 1.0);

    precise float A = 0.0;
    precise float B = 0.0;

    for (uint i = 0; i < NUM_SAMPLES; i++)
    {
        float3 H = GGXImportanceSample(HammersleySample(i, NUM_SAMPLES), alphaRoughness);

        float3 L = reflect(-V, H);

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float V_pdf = VisibilityGGX(NdotL, NdotV, alphaRoughness) * 4 * VdotH * NdotL / NdotH;
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * V_pdf;
            B += Fc * V_pdf;
        }
    }

    return float4(A / float(NUM_SAMPLES), B / float(NUM_SAMPLES), 0.0, 1.0);
}
