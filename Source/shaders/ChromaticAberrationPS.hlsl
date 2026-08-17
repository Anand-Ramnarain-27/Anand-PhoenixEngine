#include "PostProcessEffect.hlsli"

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float2 focus = Params1.xy;
    float2 direction = focus - uv;

    float3 colour;
    colour.r = InputTexture.Sample(BilinearClamp, uv + direction * Params0.x).r;
    colour.g = InputTexture.Sample(BilinearClamp, uv + direction * Params0.y).g;
    colour.b = InputTexture.Sample(BilinearClamp, uv + direction * Params0.z).b;

    return float4(colour, 1.0);
}
