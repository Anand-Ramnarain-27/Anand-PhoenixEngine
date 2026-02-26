TextureCube sky : register(t0);
SamplerState samp : register(s0);

struct PSIn
{
    float4 position : SV_POSITION;
    float3 dir : TEXCOORD;
};

float4 main(PSIn input) : SV_TARGET
{
    return sky.Sample(samp, input.dir);
}