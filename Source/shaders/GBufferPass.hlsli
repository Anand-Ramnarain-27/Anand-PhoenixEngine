#ifndef _GBUFFER_PASS_HLSLI_
#define _GBUFFER_PASS_HLSLI_

#include "Material.hlsli"

cbuffer CbMVP : register(b0){
    float4x4 MVP;
};

cbuffer CbPerInstance : register(b1){
    float4x4 ModelMatrix;
    float4x4 NormalMatrix;
    Material InstanceMaterial;
};

Texture2D BaseColorTex : register(t0);
Texture2D MetalRoughTex : register(t1);
Texture2D NormalTex : register(t2);
Texture2D OcclusionTex : register(t3);
Texture2D EmissiveTex : register(t4);

float2 OctEncode(float3 n){
    float invL1 = 1.0f / (abs(n.x) + abs(n.y) + abs(n.z));
    n *= invL1;
    if (n.z < 0.0f){
        float2 wrapped;
        wrapped.x = (1.0f - abs(n.y)) * (n.x >= 0.0f ? 1.0f : -1.0f);
        wrapped.y = (1.0f - abs(n.x)) * (n.y >= 0.0f ? 1.0f : -1.0f);
        n.xy = wrapped;
    }
    return n.xy * 0.5f + 0.5f;
}

float3 OctDecode(float2 e){
    e = e * 2.0f - 1.0f;
    float3 v = float3(e.x, e.y, 1.0f - abs(e.x) - abs(e.y));
    if (v.z < 0.0f)
        v.xy = (1.0f - abs(v.yx)) * sign(v.xy);
    return normalize(v);
}

#endif
