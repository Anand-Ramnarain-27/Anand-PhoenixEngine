#include "FogCommon.hlsli"
#include "Samplers.hlsli"

#define FOG_MODE_LINEAR 0u
#define FOG_MODE_EXP_HEIGHT 1u

cbuffer CbPerFrame : register(b0){
    float4x4 InvViewProj;
    float3 CameraPosition;
    float FramePad0;
    float3 FogColor;
    float FramePad1;
    float StartDistance;
    float EndDistance;
    float MaxOpacity;
    uint Mode;
    float Density;
    float HeightFalloff;
    float HeightOffset;
    float FramePad2;
};

Texture2D SceneColor : register(t0);
Texture2D GBufferDepth : register(t1);

// Analytic integral of exponential height-fog density along the ray [ro, ro + t*rd].
// a = global density, b = height falloff.
float ApplyHeightFog(float t, float3 ro, float3 rd, float a, float b){
    if (abs(rd.y) < 1e-5f)
        return a * exp(-ro.y * b) * t;
    return (a / b) * exp(-ro.y * b) * (1.0f - exp(-t * rd.y * b)) / rd.y;
}

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float3 sceneColor = SceneColor.Sample(BilinearClamp, uv).rgb;
    float depth = GBufferDepth.Sample(PointClamp, uv).r;

    float3 worldPos = ReconstructWorldPosFog(uv, depth, InvViewProj);
    float3 rayDir = worldPos - CameraPosition;
    float dist = length(rayDir);
    float3 normalizedRay = (dist > 1e-5f) ? (rayDir / dist) : float3(0.0f, 0.0f, 1.0f);

    float range = max(EndDistance - StartDistance, 1e-4f);
    float distWindow = saturate((dist - StartDistance) / range);

    float fogFactor;
    if (Mode == FOG_MODE_EXP_HEIGHT){
        float3 ro = CameraPosition;
        ro.y -= HeightOffset;
        fogFactor = saturate(ApplyHeightFog(dist, ro, normalizedRay, Density, HeightFalloff)) * distWindow;
    } else {
        fogFactor = distWindow;
    }
    fogFactor = saturate(fogFactor) * saturate(MaxOpacity);

    float3 result = lerp(sceneColor, FogColor, fogFactor);
    return float4(result, 1.0f);
}
