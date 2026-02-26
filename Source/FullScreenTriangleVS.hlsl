// FullScreenTriangleVS.hlsl
// Generates a single giant triangle from SV_VertexID. No vertex buffer needed.

struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0; // explicit index prevents linkage register mismatch
};

static const float2 kPositions[3] =
{
    float2(-1.0f, 1.0f),
    float2(3.0f, 1.0f),
    float2(-1.0f, -3.0f)
};

static const float2 kUVs[3] =
{
    float2(0.0f, 0.0f),
    float2(2.0f, 0.0f),
    float2(0.0f, 2.0f)
};

VSOut main(uint vertexID : SV_VertexID)
{
    VSOut o;
    o.uv = kUVs[vertexID];
    o.position = float4(kPositions[vertexID], 0.0f, 1.0f);
    return o;
}
