#ifndef _DECAL_HLSLI_
#define _DECAL_HLSLI_

// Matches the first kDecalCBBytes of DecalInstance (DecalPass.h).
cbuffer CbDecal : register(b0){
    float4x4 MVP;
    float4x4 InvModel;
    float4x4 InvViewProj;
    float4 ColourOpacity;
    float4 EmissiveAlbedoMix;   // rgb = emissive strength, w = albedo mix
};

#endif
