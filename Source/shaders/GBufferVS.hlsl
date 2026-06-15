#include "GBufferPass.hlsli"

struct VSOutput {
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT;
    float4 position : SV_POSITION;
};

VSOutput main(
    float3 position : POSITION,
    float2 uv : TEXCOORD,
    float3 normal : NORMAL,
    float4 tangent : TANGENT){
    VSOutput o;

    float4 worldPos4 = mul(float4(position, 1.0f), ModelMatrix);
    o.worldPos = worldPos4.xyz;
    o.position = mul(float4(position, 1.0f), MVP);
    o.uv = uv;
    o.normal = normalize(mul(float4(normal, 0.0f), NormalMatrix)).xyz;
    float3 worldTan = normalize(mul(float4(tangent.xyz, 0.0f), NormalMatrix)).xyz;
    o.tangent = float4(worldTan, tangent.w);

    return o;
}
