cbuffer CameraCB : register(b0)
{
    float4x4 viewProj;
};

cbuffer ObjectCB : register(b1)
{
    float4x4 world;
};

struct VSInput
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 nrm : NORMAL;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 worldPos : POSITION;
    float3 nrm : NORMAL;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.pos, 1.0f), world);
    output.worldPos = worldPos.xyz;
    output.pos = mul(worldPos, viewProj);
    output.nrm = mul(input.nrm, (float3x3) world);
    output.uv = input.uv;
    return output;
}
