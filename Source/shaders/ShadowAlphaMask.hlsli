#ifndef _SHADOW_ALPHA_MASK_HLSLI_
#define _SHADOW_ALPHA_MASK_HLSLI_

// Bound only by the "masked" shadow PSOs (alpha-tested and/or double-sided casters).
cbuffer AlphaMaskCB : register(b2){
    float MaskCutoff;
    float MaskBaseAlpha;
    uint MaskHasTexture;
    uint MaskPad;
};

Texture2D MaskBaseColorTex : register(t0);
SamplerState MaskSampler : register(s0);

void ClipShadowAlpha(float2 uv){
    float alpha = MaskBaseAlpha;
    if (MaskHasTexture)
        alpha *= MaskBaseColorTex.Sample(MaskSampler, uv).a;
    clip(alpha - MaskCutoff);
}

#endif
