#define PI 3.14159265359f

Texture2D hdrTexture : register(t0);
SamplerState linearWrap : register(s0);

float2 dirToEquirect(float3 dir)
{
    float phi = atan2(dir.z, dir.x);
    float theta = asin(clamp(dir.y, -1.0f, 1.0f));

    float u = 1.0f - (phi / (2.0f * PI) + 0.5f);
    float v = 1.0f - (theta / PI + 0.5f);

    return float2(u, v);
}

float4 main(float4 pos : SV_POSITION, float3 direction : TEXCOORD0) : SV_Target
{
    float3 dir = normalize(direction);
    float2 uv = dirToEquirect(dir);
    return float4(hdrTexture.Sample(linearWrap, uv).rgb, 1.0f);
}
