
#include "Tonemap.hlsli"

TextureCube sky : register(t0);
SamplerState samp : register(s0);

struct PSIn {
    float3 texCoord : TEXCOORD;
    float4 position : SV_POSITION;
};

float4 main(PSIn input) : SV_TARGET {
    return LinearToSRGB(sky.Sample(samp, input.texCoord));
}
