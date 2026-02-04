#include "RenderToTextureDemo.hlsli"

Texture2D diffuseTex : register(t0);
SamplerState diffuseSamp : register(s0);

// Schlick's approximation for Fresnel reflectance
float3 schlick(float3 rf0, float dotNV)
{
    return rf0 + (1.0 - rf0) * pow(1.0 - dotNV, 5.0);
}

float4 main(float3 worldPos : POSITION,
                             float3 normal : NORMAL,
                             float2 coord : TEXCOORD) : SV_TARGET
{
    // Calculate diffuse color
    float3 Cd = baseColour.rgb;
    if (hasColourTexture)
    {
        Cd *= diffuseTex.Sample(diffuseSamp, coord).rgb;
    }
        
    float3 N = normalize(normal);
    float3 L_dir = normalize(-L); // Light direction
    float3 V = normalize(viewPos - worldPos);
    float3 R = reflect(L_dir, N);
    
    float dotVR = saturate(dot(V, R));
    float dotNL = saturate(dot(N, L_dir));
    
    // Fresnel term using Schlick's approximation
    float3 fresnel = schlick(specularColour, dotNL);
    
    // Simplified PBR-inspired shading
    float rf0Max = max(max(specularColour.r, specularColour.g), specularColour.b);
    
    // Diffuse + Specular + Ambient
    float3 diffusePart = Cd * (1.0 - rf0Max) * Kd * dotNL;
    float3 specularPart = ((shininess + 2.0) / (2.0 * PI)) * fresnel * pow(dotVR, shininess) * Ks;
    float3 ambientPart = Ac * Cd;
    
    float3 colour = (diffusePart + specularPart) * Lc + ambientPart;
    
    return float4(colour, 1.0);
}