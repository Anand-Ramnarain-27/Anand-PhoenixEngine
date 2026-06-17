#ifndef _NOISE3D_HLSLI_
#define _NOISE3D_HLSLI_

cbuffer cbNoisePerFrame : register(b0){
    float4x4 matVP;
    float4x4 matGeo;
    float uTime;
    float3 _pad;
};

#endif
