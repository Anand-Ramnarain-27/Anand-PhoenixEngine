TextureCube gCubeMap : register(t0);
SamplerState gSampler : register(s0);

struct PixelIn
{
    float3 texCoord : TEXCOORD0;
    float4 position : SV_POSITION;
};

float4 main(PixelIn pin) : SV_TARGET
{
    float3 dir = normalize(pin.texCoord);
    return gCubeMap.Sample(gSampler, dir);
}
