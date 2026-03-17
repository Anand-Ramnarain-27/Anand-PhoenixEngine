cbuffer FaceIndex : register(b0)
{
    uint faceSlice;
    uint pad[3];
};

Texture2DArray SrcMipLevel : register(t0);
SamplerState bilinearSampler : register(s2);

struct PSIn
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

float4 main(PSIn input) : SV_Target
{
    return SrcMipLevel.Sample(bilinearSampler, float3(input.uv, float(faceSlice)));
}
