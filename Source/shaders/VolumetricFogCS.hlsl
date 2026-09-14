#include "FogCommon.hlsli"
#include "Noise.hlsli"

cbuffer CbPerFrame : register(b0){
    float4x4 InvViewProj;
    float3 CameraPosition;
    float Time;
    uint ViewportWidth;
    uint ViewportHeight;
    uint NumSteps;
    float ExtinctionCoeff;
    float NoiseAmount;
    float FogIntensity;
    float FramePad0;
    float FramePad1;
};

Texture2D<float> GBufferDepth : register(t0);
RWTexture2D<float4> OutputFog : register(u0);

float3 GradientNoiseGrad(uint3 cell){
    uint h0 = hash3U(cell);
    uint h1 = hashU(h0);
    float ct = 2.0f * hash2Float(h0) - 1.0f;
    float st = sqrt(max(0.0f, 1.0f - ct * ct));
    float ph = 6.28318530718f * hash2Float(h1);
    return float3(cos(ph) * st, sin(ph) * st, ct);
}

// 3D Perlin-style gradient noise, ported locally so this pass doesn't pull in
// noise3dPS.hlsl's own cbuffer (which is bound to a different b0 layout).
float GradientNoise(float3 p){
    int3 cell = (int3)floor(p);
    float3 f = frac(p);
    float3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    float v0 = dot(GradientNoiseGrad(uint3(cell + int3(0, 0, 0))), f - float3(0, 0, 0));
    float v1 = dot(GradientNoiseGrad(uint3(cell + int3(1, 0, 0))), f - float3(1, 0, 0));
    float v2 = dot(GradientNoiseGrad(uint3(cell + int3(0, 1, 0))), f - float3(0, 1, 0));
    float v3 = dot(GradientNoiseGrad(uint3(cell + int3(1, 1, 0))), f - float3(1, 1, 0));
    float v4 = dot(GradientNoiseGrad(uint3(cell + int3(0, 0, 1))), f - float3(0, 0, 1));
    float v5 = dot(GradientNoiseGrad(uint3(cell + int3(1, 0, 1))), f - float3(1, 0, 1));
    float v6 = dot(GradientNoiseGrad(uint3(cell + int3(0, 1, 1))), f - float3(0, 1, 1));
    float v7 = dot(GradientNoiseGrad(uint3(cell + int3(1, 1, 1))), f - float3(1, 1, 1));

    float front = lerp(lerp(v0, v1, u.x), lerp(v2, v3, u.x), u.y);
    float back  = lerp(lerp(v4, v5, u.x), lerp(v6, v7, u.x), u.y);
    return lerp(front, back, u.z);
}

float CalculateExtinctionCoeff(float3 worldPos){
    float n = saturate(GradientNoise(worldPos * 0.15f + Time * 0.3f) + 0.5f);
    float noiseFactor = lerp(1.0f, n, saturate(NoiseAmount));
    return ExtinctionCoeff * noiseFactor;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID){
    if (dtid.x >= ViewportWidth || dtid.y >= ViewportHeight) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(ViewportWidth, ViewportHeight);
    float depth = GBufferDepth.Load(int3(dtid.xy, 0));
    float3 worldPos = ReconstructWorldPosFog(uv, depth, InvViewProj);

    float3 rayVec = worldPos - CameraPosition;
    float dist = length(rayVec);
    float3 marchDir = (dist > 1e-5f) ? (rayVec / dist) : float3(0.0f, 0.0f, 1.0f);

    uint steps = max(NumSteps, 1u);
    float stepSize = dist / float(steps);
    float3 marchStep = marchDir * stepSize;
    float3 currentPos = CameraPosition;

    float extCoeff = 0.0f;
    [loop]
    for (uint i = 0; i < steps; ++i){
        extCoeff += CalculateExtinctionCoeff(currentPos) * stepSize;
        currentPos += marchStep;
    }

    float transmittance = saturate(FogIntensity * exp(-extCoeff));
    OutputFog[dtid.xy] = float4(0.0f, 0.0f, 0.0f, transmittance);
}
