cbuffer SkyboxCB : register(b0)
{
    float4x4 gViewProj; 
};

struct VertexIn
{
    float3 position : POSITION;
    float2 uv : TEXCOORD; 
    float3 normal : NORMAL;
};

struct VertexOut
{
    float3 texCoord : TEXCOORD0;
    float4 position : SV_POSITION;
};

VertexOut main(VertexIn vin)
{
    VertexOut vout;

    vout.texCoord = vin.position;

    float4 clipPos = mul(float4(vin.position, 1.0f), gViewProj);

    vout.position = clipPos.xyww;

    return vout;
}
