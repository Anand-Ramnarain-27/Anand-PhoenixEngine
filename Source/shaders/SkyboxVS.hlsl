cbuffer SkyboxCB : register(b0)
{
    float4x4 vp;
    bool flipX;
    bool flipZ;
    uint padding[2];
};

struct VSOut
{
    float3 texCoord : TEXCOORD;
    float4 position : SV_POSITION;
};

VSOut main(float3 pos : POSITION)
{
    VSOut o;

    o.texCoord = pos;
    float4 clipPos = mul(float4(pos, 1.0f), vp);
    o.position = clipPos.xyww;

    if (flipX)
        o.texCoord.x = -o.texCoord.x;
    if (flipZ)
        o.texCoord.z = -o.texCoord.z;

    return o;
}
