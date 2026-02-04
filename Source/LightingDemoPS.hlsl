#include "LightingDemo.hlsli"

Texture2D diffuseTex : register(t0);
SamplerState diffuseSamp : register(s0);

float3 schlick(float3 rf0, float dotNL)
{
    return rf0 + (1 - rf0) * pow(1.0 - dotNL, 5);
}

// Simple Blinn-Phong lighting for compatibility with your Material structure
float4 main(float3 worldPos : POSITION, float3 normal : NORMAL, float2 coord : TEXCOORD) : SV_TARGET
{
    // Use baseColour from Material::Data (extract RGB)
    float3 Cd = baseColour.rgb;
    
    // Apply texture if available
    if (hasColourTexture)
    {
        float4 texSample = diffuseTex.Sample(diffuseSamp, coord);
        Cd *= texSample.rgb;
    }
    
    float3 N = normalize(normal);
    float3 L_dir = normalize(-L); // Light direction (opposite of light vector)
    float3 V = normalize(viewPos - worldPos);
    float3 H = normalize(L_dir + V);
    
    // Simple ambient + diffuse + specular
    float ambient = 0.1f;
    float diffuse = max(dot(N, L_dir), 0.0f);
    float specular = pow(max(dot(N, H), 0.0f), 32.0f); // Fixed shininess
    
    // Simple lighting equation
    float3 colour = (Ac * Cd) +
                    (Lc * Cd * diffuse) +
                    (Lc * specular * 0.5f);
    
    return float4(colour, 1.0f);
}