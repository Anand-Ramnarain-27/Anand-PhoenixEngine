#ifndef _DECAL_HLSLI_
#define _DECAL_HLSLI_

cbuffer CbDecal : register(b0){
    float4x4 MVP;
    float4x4 InvModel;
    float4x4 InvViewProj;
    float4 ColourOpacity;
};

#endif
