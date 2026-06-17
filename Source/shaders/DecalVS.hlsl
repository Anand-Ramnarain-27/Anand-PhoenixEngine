
#include "Decal.hlsli"

struct VS_OUTPUT {
    float3 ndcPos : POSITION;
    float4 svPos : SV_POSITION;
};

VS_OUTPUT main(float3 position : POSITION){
    VS_OUTPUT o;
    float4 clip = mul(float4(position, 1.0f), MVP);
    o.svPos = clip;
    o.ndcPos = clip.xyz / clip.w;
    return o;
}
