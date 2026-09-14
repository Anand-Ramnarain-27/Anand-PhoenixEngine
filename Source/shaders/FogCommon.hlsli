#ifndef _FOG_COMMON_HLSLI_
#define _FOG_COMMON_HLSLI_

float3 ReconstructWorldPosFog(float2 uv, float depth, float4x4 invViewProj){
    float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 clipPos = float4(ndc, depth, 1.0f);
    float4 worldH = mul(clipPos, invViewProj);
    return worldH.xyz / worldH.w;
}

#endif
