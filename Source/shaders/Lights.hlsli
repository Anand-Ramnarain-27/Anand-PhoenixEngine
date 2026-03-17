#ifndef _LIGHTS_HLSLI_
#define _LIGHTS_HLSLI_

#include "Common.hlsli"

struct DirectionalLight
{
    float3 Direction;
    float Intensity;
    float3 Color;
};

struct PointLight
{
    float3 Position;
    float SquaredRadius;
    float3 Color;
    float Intensity;
};

struct SpotLight
{
    float3 Direction;
    float SquaredRadius;
    float3 Position;
    float InnerAngle;
    float3 Color;
    float OuterAngle;
    float Intensity;
};

float PointLightAttenuation(float sqDist, float sqRadius)
{
    float num = max(1.0 - (sqDist * sqDist) / (sqRadius * sqRadius), 0.0);
    return (num * num) / (sqDist + 1.0);
}

float SpotLightAttenuation(float cosAngle, float innerAngle, float outerAngle)
{
    float t = saturate((cosAngle - outerAngle) / (innerAngle - outerAngle));
    return t * t;
}

#endif // _LIGHTS_HLSLI_
