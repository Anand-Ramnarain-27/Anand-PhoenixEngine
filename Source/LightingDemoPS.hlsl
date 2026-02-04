#include "LightingDemo.hlsli"

Texture2D diffuseTex : register(t0);
SamplerState diffuseSamp : register(s0);

float4 main(float3 worldPos : POSITION,
                      float3 normal : NORMAL,
                      float2 coord : TEXCOORD) : SV_TARGET
{
    float3 N = normalize(normal);
    float3 L_dir = normalize(-L); 
    float3 V = normalize(viewPos - worldPos);
    
    float3 Cd = baseColour.rgb;
    if (hasColourTexture)
    {
        Cd *= diffuseTex.Sample(diffuseSamp, coord).rgb;
    }
    
    float dotNL = saturate(dot(N, L_dir));
    float3 diffuse = Cd * Kd * dotNL * Lc;
    
    float3 ambient = Ac * Cd;
    
    float3 R = reflect(-L_dir, N);
    float dotVR = saturate(dot(V, R));
    float3 specular = Ks * Lc * pow(dotVR, shininess);
    
    float3 colour = diffuse + ambient + specular;
    
    return float4(colour, 1.0);
}