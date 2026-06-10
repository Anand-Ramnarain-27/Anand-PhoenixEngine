// noise3dVS.hlsl — Vertex shader for the 3D noise material pass.
// Matches the reference "noise3d" cbuffer layout (matVP, matGeo, uTime).
// Transforms vertex position to clip space and passes world-space position
// to the noise pixel shader as WorldPos : TEXCOORD0.
// (The reference uses POSITION as the WorldPos semantic in its PS input; we
//  use TEXCOORD0 here because using an unstaged POSITION semantic on a PS input
//  can cause driver warnings on some D3D12 validation layers.)

cbuffer cbNoisePerFrame : register(b0)
{
    float4x4 matVP;   // view-projection
    float4x4 matGeo;  // model (world) matrix
    float    uTime;   // elapsed time (used by PS only)
    float3   _pad;
};

struct VSInput
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
};

struct VSOutput
{
    float4 svPos    : SV_POSITION;
    float3 WorldPos : TEXCOORD0;   // world-space position for noise sampling
};

VSOutput main(VSInput i)
{
    VSOutput o;
    float4 world = mul(float4(i.position, 1.0f), matGeo);
    o.WorldPos   = world.xyz;
    o.svPos      = mul(world, matVP);
    return o;
}
