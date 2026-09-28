#include "ShadowAlphaMask.hlsli"

cbuffer CubeMVP : register(b0){
    float4x4 WorldLightViewProj;
    float4x4 World;
    float3 LightPos;
    float InvRange;
};

float main(float4 position : SV_POSITION, float3 worldPos : POSITION,
           float2 uv : TEXCOORD) : SV_TARGET {
    ClipShadowAlpha(uv);
    return length(worldPos - LightPos) * InvRange;
}
