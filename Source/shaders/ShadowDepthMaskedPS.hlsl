#include "ShadowAlphaMask.hlsli"

void main(float4 position : SV_POSITION, float2 uv : TEXCOORD){
    ClipShadowAlpha(uv);
}
