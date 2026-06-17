
#include "Noise3d.hlsli"

struct VSInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
};

struct VSOutput {
    float4 svPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
};

VSOutput main(VSInput i){
    VSOutput o;
    float4 world = mul(float4(i.position, 1.0f), matGeo);
    o.WorldPos = world.xyz;
    o.svPos = mul(world, matVP);
    return o;
}
