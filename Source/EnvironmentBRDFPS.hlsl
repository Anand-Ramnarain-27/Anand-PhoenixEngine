// EnvironmentBRDFPS.hlsl
// Pre-computes the Environment BRDF integration LUT (Karis split-sum, second term).
// Output: R16G16_FLOAT   R = fa (F0 scale),  G = fb (constant bias)
// UV axes: uv.x = N·V,  uv.y = roughness

#define PI          3.14159265359f
#define NUM_SAMPLES 1024u

struct PSIn
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0; // must match FullScreenTriangleVS output exactly
};

// ---- Hammersley ---------------------------------------------------------
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

// ---- GGX importance sampling --------------------------------------------
float3 sampleGGX(float u1, float u2, float alpha)
{
    float a2 = alpha * alpha;
    float phi = 2.0f * PI * u1;
    float cosTheta = sqrt((1.0f - u2) / max(u2 * (a2 - 1.0f) + 1.0f, 1e-6f));
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// ---- Smith-GGX Visibility -----------------------------------------------
float V_SmithGGX(float NdotL, float NdotV, float alpha)
{
    float k = alpha * 0.5f;
    float gL = NdotL / max(NdotL * (1.0f - k) + k, 1e-6f);
    float gV = NdotV / max(NdotV * (1.0f - k) + k, 1e-6f);
    return gL * gV;
}

// ---- Main ---------------------------------------------------------------
float4 main(PSIn input) : SV_TARGET
{
    float NdotV = max(input.uv.x, 1e-4f);
    float roughness = input.uv.y;
    float alpha = max(roughness * roughness, 0.001f);

    float3 N = float3(0.0f, 0.0f, 1.0f);
    float3 V = float3(sqrt(max(0.0f, 1.0f - NdotV * NdotV)), 0.0f, NdotV);

    precise float fa = 0.0f;
    precise float fb = 0.0f;

    for (uint i = 0u; i < NUM_SAMPLES; ++i)
    {
        float2 u = hammersley(i, NUM_SAMPLES);
        float3 H = sampleGGX(u.x, u.y, alpha);
        float3 L = reflect(-V, H);

        float NdotL = max(dot(N, L), 0.0f);
        float NdotH = max(dot(N, H), 0.0f);
        float VdotH = max(dot(V, H), 0.0f);

        if (NdotL > 0.0f)
        {
            float V_pdf = V_SmithGGX(NdotL, NdotV, alpha)
                        * 4.0f * VdotH * NdotL / max(NdotH, 1e-6f);
            float Fc = pow(1.0f - VdotH, 5.0f);
            fa += (1.0f - Fc) * V_pdf;
            fb += Fc * V_pdf;
        }
    }

    return float4(fa / float(NUM_SAMPLES), fb / float(NUM_SAMPLES), 0.0f, 1.0f);
}
