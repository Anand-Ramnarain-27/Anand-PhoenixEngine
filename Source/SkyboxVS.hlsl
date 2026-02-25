cbuffer SkyboxCB : register(b0)
{
    float4x4 viewProj;
};

struct VSOutput
{
    float3 texCoord : TEXCOORD;
    float4 position : SV_POSITION;
};

VSOutput main(float3 position : POSITION)
{
    VSOutput output;
    output.texCoord = position;

    float4 pos = mul(float4(position, 1.0f), viewProj);
    output.position = pos.xyww;

    return output;
}