// Billboard pixel shader.
// Samples the (optionally animated) sprite sheet, blends between the current
// and next frame, applies the tint, and discards near-transparent texels so
// overlapping billboards don't write blended "halo" alpha into the scene.

#include "Samplers.hlsli"

Texture2D SpriteSheet : register(t0);

struct PS_INPUT
{
    float4 svPos : SV_POSITION;
    float2 uvA   : TEXCOORD0;
    float2 uvB   : TEXCOORD1;
    float  blend : TEXCOORD2;
    float4 tint  : COLOR0;
};

float4 main(PS_INPUT i) : SV_TARGET
{
    float4 colorA = SpriteSheet.Sample(BilinearWrap, i.uvA);
    float4 colorB = SpriteSheet.Sample(BilinearWrap, i.uvB);
    float4 color  = lerp(colorA, colorB, i.blend) * i.tint;

    if (color.a < 0.05f)
        discard;

    return color;
}
