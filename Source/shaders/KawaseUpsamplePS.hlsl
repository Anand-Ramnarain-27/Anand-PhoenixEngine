Texture2D InputTexture : register(t0);
#include "Samplers.hlsli"

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float2 texSize;
    InputTexture.GetDimensions(texSize.x, texSize.y);
    float2 half2 = 0.5 / texSize;

    float4 sum = InputTexture.Sample(BilinearClamp, uv + float2(-half2.x * 2.0, 0.0));
    sum += InputTexture.Sample(BilinearClamp, uv + float2(-half2.x, half2.y)) * 2.0;
    sum += InputTexture.Sample(BilinearClamp, uv + float2(0.0, half2.y * 2.0));
    sum += InputTexture.Sample(BilinearClamp, uv + float2(half2.x, half2.y)) * 2.0;
    sum += InputTexture.Sample(BilinearClamp, uv + float2(half2.x * 2.0, 0.0));
    sum += InputTexture.Sample(BilinearClamp, uv + float2(half2.x, -half2.y)) * 2.0;
    sum += InputTexture.Sample(BilinearClamp, uv + float2(0.0, -half2.y * 2.0));
    sum += InputTexture.Sample(BilinearClamp, uv + float2(-half2.x, -half2.y)) * 2.0;

    return sum / 12.0;
}
