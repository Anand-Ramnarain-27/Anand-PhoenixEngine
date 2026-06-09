// Trail pixel shader — samples the (optional) ribbon texture, multiplies by
// the per-vertex over-lifetime colour (lecture: "Render properties — over
// lifetime: Color gradient"), and discards near-transparent texels.

#include "Samplers.hlsli"

Texture2D RibbonTex : register(t0);

struct PS_INPUT
{
    float4 svPos : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PS_INPUT i) : SV_TARGET
{
    float4 tex   = RibbonTex.Sample(BilinearWrap, i.uv);
    float4 color = tex * i.color;

    if (color.a < 0.02f)
        discard;

    return color;
}
