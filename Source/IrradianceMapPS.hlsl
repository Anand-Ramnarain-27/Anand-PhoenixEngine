// IrradianceMapPS.hlsl
#define PI          3.14159265359f
#define NUM_SAMPLES 1024u

TextureCube environmentMap : register(t0);
SamplerState linearWrap : register(s0);

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

float3 hemisphereSampleCosine(float u1, float u2)
{
    float phi = 2.0f * PI * u1;
    float r = sqrt(u2); 
    return float3(r * cos(phi), r * sin(phi),
                  sqrt(max(0.0f, 1.0f - u2))); 
}

float3x3 buildTBN(float3 N)
{
    float3 up = abs(N.y) > 0.999f ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 right = normalize(cross(up, N));
    float3 bitangent = cross(N, right);
    return float3x3(right, bitangent, N);
}

float sampleLod(float pdf, uint numSamples, uint texWidth)
{
    float solidAngleSample = 1.0f / (float(numSamples) * pdf + 1e-6f);
    float solidAngleTexel = 4.0f * PI / (6.0f * float(texWidth) * float(texWidth));
    return max(0.5f * log2(solidAngleSample / solidAngleTexel), 0.0f);
}

float4 main(float3 direction : TEXCOORD) : SV_TARGET
{
    float3 N = normalize(direction);
    float3x3 TBN = buildTBN(N);

    uint texW, texH, mipCount;
    environmentMap.GetDimensions(0, texW, texH, mipCount);

    float3 irradiance = 0.0f;

    for (uint i = 0u; i < NUM_SAMPLES; ++i)
    {
        float2 u = hammersley(i, NUM_SAMPLES);
        float3 Lts = hemisphereSampleCosine(u.x, u.y); 
        
        float cosTheta = max(Lts.z, 0.0f);
        float pdf = cosTheta / PI;
        
        float3 Lws = mul(Lts, TBN);
        float lod = sampleLod(pdf, NUM_SAMPLES, texW);
        
        irradiance += environmentMap.SampleLevel(linearWrap, Lws, lod).rgb;
    }
    
    irradiance *= PI / float(NUM_SAMPLES);

    return float4(irradiance, 1.0f);
}
