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

#endif
