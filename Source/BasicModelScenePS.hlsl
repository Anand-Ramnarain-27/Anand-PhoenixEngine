cbuffer Material : register(b1)
{
    float4 baseColour;
    bool hasColourTexture;
};

Texture2D colourTex : register(t0);
SamplerState colourSamp : register(s0);

float4 main(float2 texCoord : TEXCOORD, float3 normal : NORMAL) : SV_TARGET
{
    return hasColourTexture ? colourTex.Sample(colourSamp, texCoord) * baseColour : baseColour;
}