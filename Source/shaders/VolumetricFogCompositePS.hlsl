#include "Samplers.hlsli"

cbuffer CbComposite : register(b0){
    float MaxOpacity;
    float3 CompositePad0;
};

Texture2D SceneColor : register(t0);
Texture2D VolumetricFog : register(t1);

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float3 sceneColor = SceneColor.Sample(BilinearClamp, uv).rgb;
    // Bilinear upsample also doubles as a cheap blur that filters the dithering noise (matches
    // the lecture's "blur to filter the noise" step, and is a free side-effect of the half-res
    // optimization when that mode is enabled).
    float4 vol = VolumetricFog.Sample(BilinearClamp, uv);

    float strength = saturate(MaxOpacity);
    float transmittance = lerp(1.0f, vol.a, strength);
    float3 inScattering = vol.rgb * strength;

    float3 result = sceneColor * transmittance + inScattering;
    return float4(result, 1.0f);
}
