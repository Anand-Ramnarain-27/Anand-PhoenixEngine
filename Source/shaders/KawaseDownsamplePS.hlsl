Texture2D InputTexture : register(t0);
#include "Samplers.hlsli"

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float2 texSize;
    InputTexture.GetDimensions(texSize.x, texSize.y);
    float2 half2 = 0.5 / texSize;

    float4 sum = InputTexture.Sample(BilinearClamp, uv) * 4.0;
    sum += InputTexture.Sample(BilinearClamp, uv + float2(-half2.x, half2.y));
    sum += InputTexture.Sample(BilinearClamp, uv + float2(half2.x, half2.y));
    sum += InputTexture.Sample(BilinearClamp, uv + float2(-half2.x, -half2.y));
    sum += InputTexture.Sample(BilinearClamp, uv + float2(half2.x, -half2.y));

    return sum / 8.0;
}
