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
    float4 tangent : TANGENT; 
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 worldPos : POSITION;
    float3 nrm : NORMAL;
    float4 tangent : TANGENT; 
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(float4(input.pos, 1.0f), world);
    output.worldPos = worldPos.xyz;
    output.pos = mul(worldPos, viewProj);
    output.uv = input.uv;
    
    float3x3 worldRot = (float3x3) world;
    output.nrm = normalize(mul(input.nrm, worldRot));
    output.tangent = float4(normalize(mul(input.tangent.xyz, worldRot)),
                               input.tangent.w); 

    return output;
}
