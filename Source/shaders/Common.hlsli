#ifndef _COMMON_HLSLI_
#define _COMMON_HLSLI_

#define PI 3.14159265359

float Square(float x)
{
    return x * x;
}

float3 Square(float3 x)
{
    return x * x;
}

float3x3 BuildTangentBasis(in float3 normal)
{
    float3 up    = abs(normal.y) > 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up           = cross(normal, right);

    return float3x3(right, up, normal);
}

#endif // _COMMON_HLSLI_
