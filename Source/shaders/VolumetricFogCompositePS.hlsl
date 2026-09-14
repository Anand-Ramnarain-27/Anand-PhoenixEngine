#include "Samplers.hlsli"

cbuffer CbComposite : register(b0){
    float MaxOpacity;
    float3 CompositePad0;
};

Texture2D SceneColor : register(t0);
Texture2D VolumetricFog : register(t1);

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float3 sceneColor = SceneColor.Sample(BilinearClamp, uv).rgb;
    float4 vol = VolumetricFog.Sample(PointClamp, uv);

    float strength = saturate(MaxOpacity);
    float transmittance = lerp(1.0f, vol.a, strength);
    float3 inScattering = vol.rgb * strength;

    float3 result = sceneColor * transmittance + inScattering;
    return float4(result, 1.0f);
}
