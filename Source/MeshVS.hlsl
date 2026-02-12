cbuffer PerObject : register(b0)
{
    float4x4 MVP;
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
};

VSOutput main(VSInput input)
{
    VSOutput o;

    o.pos = mul(float4(input.pos, 1.0f), MVP);
    o.uv = input.uv;

    return o;
}
