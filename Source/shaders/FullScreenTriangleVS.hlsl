static const float2 positions[3] =
{
    float2(-1.0f, 1.0f),
                                     float2(3.0f, 1.0f),
                                     float2(-1.0f, -3.0f)
};

static const float2 uvs[3] =
{
    float2(0.0f, 0.0f),
                                     float2(2.0f, 0.0f),
                                     float2(0.0f, 2.0f)
};

void main(uint vertexID : SV_VertexID,
          out float4 position : SV_Position,
          out float2 texcoord : TEXCOORD)
{
    position = float4(positions[vertexID], 0.0f, 1.0f);
    texcoord = uvs[vertexID];
}
