#include "PostProcessEffect.hlsli"

float luma(float3 c){ return dot(c, float3(0.299, 0.587, 0.114)); }

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float2 texSize;
    InputTexture.GetDimensions(texSize.x, texSize.y);
    float2 texel = 1.0 / texSize;

    float3 colourM = InputTexture.Sample(BilinearClamp, uv).rgb;
    float lumaM = luma(colourM);
    float lumaN = luma(InputTexture.Sample(BilinearClamp, uv + float2(0, -texel.y)).rgb);
    float lumaS = luma(InputTexture.Sample(BilinearClamp, uv + float2(0,  texel.y)).rgb);
    float lumaE = luma(InputTexture.Sample(BilinearClamp, uv + float2( texel.x, 0)).rgb);
    float lumaW = luma(InputTexture.Sample(BilinearClamp, uv + float2(-texel.x, 0)).rgb);

    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));
    float contrast = lumaMax - lumaMin;

    float threshold = max(Params0.y * lumaMax, Params0.x);
    if (contrast < threshold)
        return float4(colourM, 1.0);

    float lumaNW = luma(InputTexture.Sample(BilinearClamp, uv + float2(-texel.x, -texel.y)).rgb);
    float lumaNE = luma(InputTexture.Sample(BilinearClamp, uv + float2( texel.x, -texel.y)).rgb);
    float lumaSW = luma(InputTexture.Sample(BilinearClamp, uv + float2(-texel.x,  texel.y)).rgb);
    float lumaSE = luma(InputTexture.Sample(BilinearClamp, uv + float2( texel.x,  texel.y)).rgb);

    float average = 2.0 * (lumaN + lumaS + lumaE + lumaW);
    average += lumaNE + lumaNW + lumaSE + lumaSW;
    average /= 12.0;

    float factor = abs(average - lumaM) / max(contrast, 1e-5);
    factor = smoothstep(0.0, 1.0, saturate(factor));
    factor *= factor;
    factor *= Params0.z;

    float edgeVert = abs((0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE))
                    + abs((0.50 * lumaW) + (-1.0 * lumaM) + (0.50 * lumaE))
                    + abs((0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE));
    float edgeHorz = abs((0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW))
                    + abs((0.50 * lumaN) + (-1.0 * lumaM) + (0.50 * lumaS))
                    + abs((0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE));
    bool isHorizontal = edgeHorz >= edgeVert;

    float2 pixelStep = isHorizontal ? float2(0.0, texel.y) : float2(texel.x, 0.0);

    float positiveLuma = isHorizontal ? lumaS : lumaE;
    float negativeLuma = isHorizontal ? lumaN : lumaW;
    float positiveGradient = abs(positiveLuma - lumaM);
    float negativeGradient = abs(negativeLuma - lumaM);
    if (negativeGradient > positiveGradient)
        pixelStep = -pixelStep;

    float2 finalUV = uv + pixelStep * factor;
    float3 result = InputTexture.Sample(BilinearClamp, finalUV).rgb;
    return float4(result, 1.0);
}
