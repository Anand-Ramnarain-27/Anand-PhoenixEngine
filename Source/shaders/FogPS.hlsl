#include "FogCommon.hlsli"
#include "Samplers.hlsli"

cbuffer CbPerFrame : register(b0){
    float4x4 InvViewProj;
    float3 CameraPosition;
    float FramePad0;
    float3 FogColor;
    float FramePad1;
    float StartDistance;
    float EndDistance;
    float MaxOpacity;
    float FramePad2;
};

Texture2D SceneColor : register(t0);
Texture2D GBufferDepth : register(t1);

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float3 sceneColor = SceneColor.Sample(BilinearClamp, uv).rgb;
    float depth = GBufferDepth.Sample(PointClamp, uv).r;

    float3 worldPos = ReconstructWorldPosFog(uv, depth, InvViewProj);
    float dist = length(worldPos - CameraPosition);

    float range = max(EndDistance - StartDistance, 1e-4f);
    float fogFactor = saturate((dist - StartDistance) / range) * saturate(MaxOpacity);

    float3 result = lerp(sceneColor, FogColor, fogFactor);
    return float4(result, 1.0f);
}
