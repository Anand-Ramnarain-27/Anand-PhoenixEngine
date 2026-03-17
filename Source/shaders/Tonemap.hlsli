#ifndef _TONEMAP_HLSLI_
#define _TONEMAP_HLSLI_

static const float Gamma    = 2.2;
static const float InvGamma = 1.0 / Gamma;

float3 LinearToSRGB(float3 color)
{
    return pow(color, InvGamma);
}

float4 LinearToSRGB(float4 color)
{
    return float4(pow(color.rgb, InvGamma), 1.0);
}

float3 SRGBToLinear(float3 color)
{
    return pow(color, Gamma);
}

float4 SRGBToLinear(float4 color)
{
    return float4(SRGBToLinear(color.rgb), color.a);
}

float3 PBRNeutralTonemap(float3 color)
{
    const float startCompression = 0.8 - 0.04;
    const float desaturation     = 0.15;

    float x      = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color       -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression)
        return color;

    const float d      = 1.0 - startCompression;
    float       newPeak = 1.0 - d * d / (peak + d - startCompression);
    color              *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return lerp(color, newPeak, g);
}

#endif // _TONEMAP_HLSLI_
