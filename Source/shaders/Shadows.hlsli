#ifndef _SHADOWS_HLSLI_
#define _SHADOWS_HLSLI_


#define MAX_CASCADES 4

float3 WorldToShadowUV(float3 worldPos, float4x4 lightViewProj){
    float4 clip = mul(float4(worldPos, 1.0f), lightViewProj);
    clip.xyz /= clip.w;
    float3 r;
    r.xy = clip.xy * float2(0.5f, -0.5f) + 0.5f;
    r.z = clip.z;
    return r;
}

bool InsideCascade(float3 s){
    return s.x >= 0.0f && s.x <= 1.0f && s.y >= 0.0f && s.y <= 1.0f && s.z <= 1.0f;
}

float SampleShadowArrayPCF(Texture2DArray shadowMap, SamplerComparisonState cmp,
                           float2 uv, float cmpDepth, int slice,
                           float pcfRadius, float texelSize){
    int r = (int)pcfRadius;
    if (r <= 0)
        return shadowMap.SampleCmpLevelZero(cmp, float3(uv, slice), cmpDepth);

    float sum = 0.0f;
    float count = 0.0f;
    for (int x = -r; x <= r; ++x){
        for (int y = -r; y <= r; ++y){
            float2 off = float2(x, y) * texelSize;
            sum += shadowMap.SampleCmpLevelZero(cmp, float3(uv + off, slice), cmpDepth);
            count += 1.0f;
        }
    }
    return sum / count;
}

float ComputeCascadeShadow(Texture2DArray shadowMap, SamplerComparisonState cmp,
                           float3 worldPos, float4x4 lightViewProj[MAX_CASCADES],
                           int cascadeCount, float bias, float pcfRadius,
                           float texelSize, out int outCascade){
    outCascade = -1;
    for (int c = 0; c < cascadeCount; ++c){
        float3 s = WorldToShadowUV(worldPos, lightViewProj[c]);
        if (InsideCascade(s)){
            outCascade = c;
            return SampleShadowArrayPCF(shadowMap, cmp, s.xy, s.z - bias, c,
                                        pcfRadius, texelSize);
        }
    }
    return 1.0f;
}


float ChebyshevUpperBound(float2 moments, float t, float lightBleed){
    float p = step(t, moments.x);
    float variance = moments.y - moments.x * moments.x;
    variance = max(variance, 0.00002);
    float d = t - moments.x;
    float pMax = variance / (variance + d * d);
    pMax = saturate((pMax - lightBleed) / (1.0 - lightBleed));
    return max(p, pMax);
}

float ComputeCascadeShadowMoments(Texture2DArray momentMap, SamplerState linClamp,
                                  float3 worldPos, float4x4 lightViewProj[MAX_CASCADES],
                                  int cascadeCount, float bias, float expK, int useExp,
                                  float lightBleed, out int outCascade){
    outCascade = -1;
    for (int c = 0; c < cascadeCount; ++c){
        float3 s = WorldToShadowUV(worldPos, lightViewProj[c]);
        if (InsideCascade(s)){
            outCascade = c;
            float2 m = momentMap.SampleLevel(linClamp, float3(s.xy, c), 0).rg;
            float t = s.z - bias;
            if (useExp) t = exp2(t * expK);
            return ChebyshevUpperBound(m, t, lightBleed);
        }
    }
    return 1.0f;
}

float SampleSpotShadow(Texture2D shadowMap, SamplerComparisonState cmp,
                       float3 worldPos, float4x4 lightViewProj,
                       float bias, float pcfRadius, float texelSize){
    float3 s = WorldToShadowUV(worldPos, lightViewProj);
    if (s.x < 0.0f || s.x > 1.0f || s.y < 0.0f || s.y > 1.0f || s.z > 1.0f)
        return 1.0f;
    float cmpDepth = s.z - bias;
    int r = (int)pcfRadius;
    if (r <= 0)
        return shadowMap.SampleCmpLevelZero(cmp, s.xy, cmpDepth);
    float sum = 0.0f, count = 0.0f;
    for (int x = -r; x <= r; ++x)
        for (int y = -r; y <= r; ++y){
            sum += shadowMap.SampleCmpLevelZero(cmp, s.xy + float2(x, y) * texelSize, cmpDepth);
            count += 1.0f;
        }
    return sum / count;
}

float SamplePointShadow(TextureCube distCube, SamplerState samp,
                        float3 worldPos, float3 lightPos, float invRange, float bias){
    float3 dir = worldPos - lightPos;
    float current = length(dir) * invRange;
    if (current > 1.0f) return 1.0f;
    float stored = distCube.SampleLevel(samp, normalize(dir), 0).r;
    return (current - bias <= stored) ? 1.0f : 0.0f;
}

float3 CascadeDebugTint(int cascade){
    if (cascade == 0) return float3(1.0f, 0.4f, 0.4f);
    if (cascade == 1) return float3(0.4f, 1.0f, 0.4f);
    if (cascade == 2) return float3(0.4f, 0.5f, 1.0f);
    if (cascade == 3) return float3(1.0f, 1.0f, 0.4f);
    return float3(1.0f, 1.0f, 1.0f);
}

#endif
