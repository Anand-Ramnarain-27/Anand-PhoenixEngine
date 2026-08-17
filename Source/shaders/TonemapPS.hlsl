#include "Tonemap.hlsli"
#include "Samplers.hlsli"

cbuffer TonemapParams : register(b0){
    float Exposure;
    float BloomIntensity;
    float LutEnabled;
    float _Pad;
};

Texture2D SceneColor : register(t0);
Texture2D BloomTex : register(t1);
Texture3D LutTex : register(t2);

float4 main(float2 uv : TEXCOORD) : SV_Target {
    float3 hdr = SceneColor.Sample(BilinearClamp, uv).rgb;
    float3 bloom = BloomTex.Sample(BilinearClamp, uv).rgb;
    hdr += bloom * BloomIntensity;

    hdr *= pow(2.0, Exposure);

    float3 mapped = PBRNeutralTonemap(hdr);

    if (LutEnabled > 0.5)
        mapped = LutTex.Sample(BilinearClamp, saturate(mapped)).rgb;

    return float4(LinearToSRGB(mapped), 1.0f);
}
