// ParticleRenderPS.hlsl — GPU particle pixel shader.
// Samples the particle texture (t1) using the per-vertex UV and multiplies by
// the per-particle color (already baked with the over-lifetime gradient on the CPU).
// Near-transparent texels are discarded to avoid polluting the depth buffer with
// invisible geometry that blocks back-to-front sorted particles behind it.

#include "Samplers.hlsli"

Texture2D ParticleTex : register(t1);

struct PS_INPUT
{
    float4 svPos : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PS_INPUT i) : SV_TARGET
{
    float4 tex    = ParticleTex.Sample(BilinearWrap, i.uv);
    float4 result = tex * i.color;

    // Discard near-transparent pixels — same threshold as TrailPS.
    if (result.a < 0.01f)
        discard;

    return result;
}
