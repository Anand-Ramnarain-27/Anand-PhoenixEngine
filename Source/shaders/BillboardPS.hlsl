
#include "Samplers.hlsli"

Texture2D SpriteSheet : register(t0);

struct PS_INPUT {
    float4 svPos : SV_POSITION;
    float2 uvA : TEXCOORD0;
    float2 uvB : TEXCOORD1;
    float blend : TEXCOORD2;
    float4 tint : COLOR0;
    float premultiplied : TEXCOORD3;
};

float4 main(PS_INPUT i) : SV_TARGET {
    float4 colorA = SpriteSheet.Sample(BilinearWrap, i.uvA);
    float4 colorB = SpriteSheet.Sample(BilinearWrap, i.uvB);
    float4 tex = lerp(colorA, colorB, i.blend);

    if (i.premultiplied > 0.5f){
        // Premultiplied blend (ONE, INV_SRC_ALPHA): the texture's own alpha masks the colour, the tint alpha only
        // decides how much it occludes, so tint alpha 0 is a pure additive glow.
        float3 rgb = tex.rgb * tex.a * i.tint.rgb;
        float a = tex.a * i.tint.a;
        if (a < 0.004f && max(rgb.r, max(rgb.g, rgb.b)) < 0.004f)
            discard;
        return float4(rgb, a);
    }

    float4 color = tex * i.tint;

    if (color.a < 0.05f)
        discard;

    return color;
}
