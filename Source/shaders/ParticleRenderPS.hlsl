
#include "Samplers.hlsli"

Texture2D ParticleTex : register(t1);

struct PS_INPUT {
    float4 svPos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PS_INPUT i) : SV_TARGET {
    float4 tex = ParticleTex.Sample(BilinearWrap, i.uv);
    float4 result = tex * i.color;

    if (result.a < 0.01f)
        discard;

    return result;
}
