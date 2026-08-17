cbuffer BloomParams : register(b0){
    float Threshold;
};

Texture2D InputTexture : register(t0);
#include "Samplers.hlsli"

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float3 colour = InputTexture.Sample(BilinearClamp, uv).rgb;
    float brightness = max(colour.r, max(colour.g, colour.b));
    float contribution = max(0.0, brightness - Threshold) / max(brightness, 1e-4);
    return float4(colour * contribution, 1.0);
}
