cbuffer Transforms : register(b0)
{
    float4x4 mvp;
};

struct VertexOutput
{
    float2 texCoord : TEXCOORD;
    float3 normal : NORMAL;
    float4 position : SV_POSITION;
};

VertexOutput main(float3 position : POSITION, float2 texCoord : TEXCOORD, float3 normal : NORMAL)
{
    VertexOutput output;
    output.position = mul(float4(position, 1.0f), mvp);
    output.texCoord = texCoord;
    output.normal = normal; // Pass through normal for potential future use

    return output;
}