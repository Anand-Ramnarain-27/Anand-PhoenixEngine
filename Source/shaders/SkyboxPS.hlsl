
static const float GAMMA = 2.2f;
static const float INV_GAMMA = 1.0f / GAMMA;

float4 linearToSRGB(float4 color)
{
    return float4(pow(color.rgb, INV_GAMMA), 1.0f);
}

TextureCube sky : register(t0);
SamplerState samp : register(s0);

struct PSIn
{
    float3 texCoord : TEXCOORD; 
    float4 position : SV_POSITION; 
};

float4 main(PSIn input) : SV_TARGET
{
    return linearToSRGB(sky.Sample(samp, input.texCoord));
}
