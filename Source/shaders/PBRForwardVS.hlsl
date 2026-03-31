#include "ForwardPass.hlsli"

struct VS_OUTPUT
{
    float3 worldPos : POSITION;
    float2 texCoord : TEXCOORD;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT; 
    float4 position : SV_POSITION;
};

VS_OUTPUT main(
    float3 position : POSITION,
    float2 texCoord : TEXCOORD,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    uint4 joints : JOINTS, 
    float4 weights : WEIGHTS)

{
    VS_OUTPUT output;

    float4 worldPos = mul(float4(position, 1.0), ModelMatrix);
    output.position = mul(float4(position, 1.0), MVP);
    output.worldPos = worldPos.xyz;

    output.normal = normalize(mul(float4(normal, 0.0), NormalMatrix)).xyz;
    
    float3 worldTangent = normalize(mul(float4(tangent.xyz, 0.0), NormalMatrix)).xyz;
    output.tangent = float4(worldTangent, tangent.w);

    output.texCoord = texCoord;
    return output;
}
