#include "Tonemap.hlsli"

Texture2D SceneColor : register(t0);
SamplerState bilinearSampler : register(s0);

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float3 hdr = SceneColor.Sample(bilinearSampler, uv).rgb;
    float3 color = PBRNeutralTonemap(hdr);
    color = LinearToSRGB(color);
    return float4(color, 1.0f);
}
