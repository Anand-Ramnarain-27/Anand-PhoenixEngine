cbuffer SkyboxCB : register(b0)
{
    float4x4 vp;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float3 dir : TEXCOORD;
};

VSOut main(float3 pos : POSITION)
{
    VSOut o;
    o.dir = pos;
    float4 clip = mul(float4(pos, 1), vp);
    o.position = clip.xyww;
    return o;
}