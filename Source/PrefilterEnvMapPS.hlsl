#define PI          3.14159265359f
#define NUM_SAMPLES 1024u

TextureCube environmentMap : register(t0);
SamplerState linearWrap : register(s0);

cbuffer FaceCB : register(b0)
{
    float4x4 vp;
    int flipX;
    int flipZ;
    float roughness;
    float _pad;
};

struct PSIn
{
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0; 
};

float luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float radicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

float2 hammersley(uint i, uint n)
{
    return float2(float(i) / float(n), radicalInverse_VdC(i));
}

float3 sampleGGX(float u1, float u2, float alpha)
{
    float a2 = alpha * alpha;
    float phi = 2.0f * PI * u1;
    float cosTheta = sqrt((1.0f - u2) / max(u2 * (a2 - 1.0f) + 1.0f, 1e-6f));
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float D_GGX(float NdotH, float alpha)
{
    float a2 = alpha * alpha;
    float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-7f);
}

float3x3 buildTBN(float3 N)
{
    float3 up = abs(N.y) > 0.999f ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 right = normalize(cross(up, N));
    float3 bt = cross(N, right);
    return float3x3(right, bt, N);
}

float sampleLod(float pdf, uint numSamples, uint texWidth)
{
    float solidAngleSample = 1.0f / (float(numSamples) * pdf + 1e-6f);
    float solidAngleTexel = 4.0f * PI / (6.0f * float(texWidth) * float(texWidth));
    return max(0.5f * log2(solidAngleSample / solidAngleTexel), 0.0f);
}

float4 main(PSIn input) : SV_TARGET
{
    float3 R = normalize(input.direction);
    float3 N = R, V = R;
    float alpha = max(roughness * roughness, 0.001f);
    float3x3 TBN = buildTBN(N);

    uint texW, texH, mipCount;
    environmentMap.GetDimensions(0, texW, texH, mipCount);

    float3 colorSum = 0.0f;
    float weightSum = 0.0f;
    float totalLum = 0.0f;
    uint lumCount = 0u;

    for (uint i = 0u; i < NUM_SAMPLES; ++i)
    {
        float2 u = hammersley(i, NUM_SAMPLES);
        float3 Hts = sampleGGX(u.x, u.y, alpha);
        float3 H = normalize(mul(Hts, TBN));
        float3 L = reflect(-V, H);
        float NdotL = dot(N, L);

        if (NdotL > 0.0f)
        {
            float NdotH = max(dot(N, H), 0.0f);
            float VdotH = max(dot(V, H), 0.0f);
            float D = D_GGX(NdotH, alpha);
            float pdf = (D * NdotH) / max(4.0f * VdotH, 1e-6f);
            float lod = sampleLod(pdf, NUM_SAMPLES, texW);

            float3 s = environmentMap.SampleLevel(linearWrap, L, lod).rgb;
            colorSum += s * NdotL;
            weightSum += NdotL;
            totalLum += luminance(s);
            lumCount++;
        }
    }

    float3 result = (weightSum > 1e-6f) ? colorSum / weightSum : float3(0, 0, 0);
    
    if (lumCount > 0u)
    {
        float avgLum = totalLum / float(lumCount);
        if (avgLum < 0.1f && avgLum > 1e-6f)
        {
            float exposure = 0.5f / avgLum;
            float resultLum = luminance(result);
            float3 grey = float3(resultLum, resultLum, resultLum);
            float desatAmount = saturate((0.1f - avgLum) / 0.1f);
            result = lerp(result, grey, desatAmount) * exposure;
        }
    }

    return float4(result, 1.0f);
}
