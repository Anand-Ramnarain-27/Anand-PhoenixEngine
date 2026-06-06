#include "Samplers.hlsli"

Texture2D ParticleTex : register(t1);  // t0 = particle SRV (VS), t1 = sprite texture (PS)

struct PSIn
{
    float4 pos    : SV_POSITION;
    float2 uv     : TEXCOORD0;
    float4 colour : COLOR0;
};

float4 main(PSIn i) : SV_TARGET
{
    float4 tex = ParticleTex.Sample(BilinearClamp, i.uv);
    return tex * i.colour;
}
