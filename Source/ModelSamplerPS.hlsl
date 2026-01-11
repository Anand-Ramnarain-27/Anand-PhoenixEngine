#include "ModelSampler.hlsli"

Texture2D diffuseTex : register(t0);
SamplerState diffuseSamp : register(s0);

float4 main(
    float3 worldPos : POSITION,
    float3 normal : NORMAL,
    float2 coord : TEXCOORD
) : SV_TARGET
{
    float3 Cd = hasDiffuseTex
        ? diffuseTex.Sample(diffuseSamp, coord).rgb * diffuseColour.rgb 
        : diffuseColour.rgb;

    float3 N = normalize(normal);
    float3 Ln = normalize(L);
    float3 V = normalize(viewPos - worldPos);

    float3 ambient = Ac * Cd;

    float NdotL = saturate(-dot(Ln, N));
    
    if (NdotL <= 0.0f)
    {
        return float4(ambient, 1.0f);
    }

    float3 diffuse = Kd * Cd * Lc * NdotL;

    float3 R = reflect(Ln, N);
    float RdotV = saturate(dot(R, V));
    float3 specular = Ks * specularColour.rgb * pow(RdotV, shininess);

    float3 finalColor = ambient + diffuse + specular;
    
    return float4(finalColor, 1.0f);
}