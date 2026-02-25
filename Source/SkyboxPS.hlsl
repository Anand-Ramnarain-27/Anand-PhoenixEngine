TextureCube skyboxTexture : register(t0);
SamplerState skySampler : register(s0);

float4 main(float3 texCoord : TEXCOORD) : SV_TARGET
{
    return skyboxTexture.Sample(skySampler, texCoord);
}