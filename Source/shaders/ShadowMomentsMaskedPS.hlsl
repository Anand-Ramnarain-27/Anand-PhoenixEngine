#include "ShadowAlphaMask.hlsli"

cbuffer MomentParams : register(b1){
    float ExpK;
    uint  UseExp;
    float2 _pad;
};

float2 main(float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
    ClipShadowAlpha(uv);
    float d = position.z;
    if (UseExp) d = exp2(d * ExpK);
    return float2(d, d * d);
}
