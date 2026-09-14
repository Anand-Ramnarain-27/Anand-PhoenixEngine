#include "FogCommon.hlsli"
#include "Noise.hlsli"
#include "Lights.hlsli"
#include "Shadows.hlsli"
#include "Samplers.hlsli"

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 64

cbuffer CbPerFrame : register(b0){
    float4x4 InvViewProj;
    float3 CameraPosition;
    float Time;
    uint ViewportWidth;
    uint ViewportHeight;
    uint FullViewportWidth;
    uint FullViewportHeight;
    uint NumSteps;
    float ExtinctionCoeff;
    float NoiseAmount;
    float FogIntensity;
    float AnisotropyG;
    uint FrameIndex;
    uint BoundedRayLength;
    float FramePad0;
    uint DirLightCount;
    uint PointLightCount;
    uint SpotLightCount;
    float FramePad1;
    float4x4 LightViewProj[MAX_CASCADES];
    float4 ShadowParams0;
    float4 ShadowParams1;
    float4 ShadowParams2;
    float3 ShadowLightDir;
    float ShadowPad;
    float4x4 SpotViewProj;
    float4 SpotShadowParams;
    float4 SpotShadowPos;
    float4 PointShadowParams;
    float4 PointShadowPos;
};

Texture2D<float> GBufferDepth : register(t0);
RWTexture2D<float4> OutputFog : register(u0);

StructuredBuffer<DirectionalLight> DirLights : register(t1);
StructuredBuffer<PointLight> PointLights : register(t2);
StructuredBuffer<SpotLight> SpotLights : register(t3);

Texture2DArray ShadowMap : register(t4);
Texture2DArray ShadowMoments : register(t5);
Texture2D SpotShadowMap : register(t6);
TextureCube PointShadowMap : register(t7);

StructuredBuffer<int> PointLightIndices : register(t8);
StructuredBuffer<int> SpotLightIndices : register(t9);

cbuffer GpuVP : register(b1){
    row_major float4x4 GpuViewProj;
};

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

// Henyey-Greenstein phase function, Schlick's fast approximation.
float PhaseHG_Schlick(float cosTheta, float g){
    float k = 1.55f * g - 0.55f * g * g * g;
    float denom = 1.0f - k * cosTheta;
    return (1.0f - k * k) / (4.0f * PI * denom * denom);
}

// Interleaved Gradient Noise: a cheap, animated low-discrepancy sequence used to dither the
// ray-march start offset so a low step count bands instead of banding coherently every frame.
float SampleIGN(float2 pixelXY, float frameIndex){
    pixelXY += frameIndex * 5.588238f;
    return frac(52.9829189f * frac(0.06711056f * pixelXY.x + 0.00583715f * pixelXY.y));
}

uint GetTileIndex(uint2 pixelPos){
    uint numTilesX = (FullViewportWidth + TILE_SIZE - 1) / TILE_SIZE;
    return (pixelPos.y / TILE_SIZE) * numTilesX + (pixelPos.x / TILE_SIZE);
}

bool RaySphereIntersect(float3 ro, float3 rd, float3 center, float radius, out float t0, out float t1){
    float3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0f){ t0 = 0.0f; t1 = 0.0f; return false; }
    float s = sqrt(disc);
    t0 = -b - s;
    t1 = -b + s;
    return true;
}

// Bounding sphere of a spot light's cone, same construction used by LightCullingCS.hlsl's tile test.
void SpotBoundingSphere(SpotLight sl, out float3 sphereC, out float sphereR){
    float spotR = sqrt(sl.SquaredRadius);
    float cosOuter = sl.OuterAngle;
    const float kCosQuarterPi = 0.70710678f;
    if (cosOuter < kCosQuarterPi){
        float sinA = sqrt(1.0f - cosOuter * cosOuter);
        float tanA = sinA / cosOuter;
        sphereR = spotR * tanA;
        sphereC = sl.Position + sl.Direction * spotR;
    } else {
        sphereR = spotR * 0.5f / (cosOuter * cosOuter);
        sphereC = sl.Position + sl.Direction * sphereR;
    }
}

float SampleDirectionalShadow(float3 worldPos){
    if (ShadowParams1.x < 0.5f) return 1.0f;
    int mode = (int)ShadowParams1.y;
    int cascadeCount = (int)ShadowParams1.z;
    int cascade;
    if (ShadowParams2.z > 0.5f){
        float3 s = WorldToShadowUV(worldPos, GpuViewProj);
        if (!InsideCascade(s)) return 1.0f;
        return SampleShadowArrayPCF(ShadowMap, ShadowCmp, s.xy, s.z - ShadowParams0.x, 0,
                                    ShadowParams0.z, ShadowParams0.w);
    } else if (mode == 0){
        return ComputeCascadeShadow(ShadowMap, ShadowCmp, worldPos, LightViewProj,
                                    cascadeCount, ShadowParams0.x, ShadowParams0.z, ShadowParams0.w, cascade);
    }
    return ComputeCascadeShadowMoments(ShadowMoments, BilinearClamp, worldPos,
                                       LightViewProj, cascadeCount, ShadowParams0.x,
                                       ShadowParams2.x, mode == 2 ? 1 : 0, ShadowParams2.y, cascade);
}

// Sum of Li(x, L) * Vis(x, L) * phase(rayDir, L) over lights visible at world-space point worldPos.
// rayDir is the camera-to-point ray direction (the lecture's -V). Directional lights are always
// evaluated (cheap, few of them); point/spot lights use this pixel's tile light list and are
// skipped entirely when includePointSpot is false (the "bounded ray length" optimization).
float3 AccumulateInScattering(float3 worldPos, float3 rayDir, uint tileIdx, bool includePointSpot){
    float3 result = float3(0.0f, 0.0f, 0.0f);

    float dirShadow = SampleDirectionalShadow(worldPos);
    for (uint i = 0; i < DirLightCount; ++i){
        DirectionalLight L = DirLights[i];
        float3 Ldir = normalize(-L.Direction);
        float phase = PhaseHG_Schlick(dot(rayDir, Ldir), AnisotropyG);
        float vis = (i == 0) ? dirShadow : 1.0f;
        result += L.Color * L.Intensity * phase * vis;
    }

    if (!includePointSpot) return result;

    for (uint pi = 0; pi < MAX_LIGHTS_PER_TILE; ++pi){
        int idx = PointLightIndices[tileIdx * MAX_LIGHTS_PER_TILE + pi];
        if (idx < 0) break;
        PointLight L = PointLights[idx];
        float3 toLight = L.Position - worldPos;
        float sqDist = dot(toLight, toLight);
        float atten = PointLightAttenuation(sqDist, L.SquaredRadius);
        float3 Ldir = normalize(toLight);
        float phase = PhaseHG_Schlick(dot(rayDir, Ldir), AnisotropyG);
        float vis = 1.0f;
        if (PointShadowParams.x > 0.5f && distance(L.Position, PointShadowPos.xyz) < 0.05f)
            vis = SamplePointShadow(PointShadowMap, BilinearClamp, worldPos, PointShadowPos.xyz,
                                    PointShadowParams.z, PointShadowParams.y);
        result += L.Color * L.Intensity * atten * phase * vis;
    }

    for (uint si = 0; si < MAX_LIGHTS_PER_TILE; ++si){
        int idx = SpotLightIndices[tileIdx * MAX_LIGHTS_PER_TILE + si];
        if (idx < 0) break;
        SpotLight L = SpotLights[idx];
        float3 toLight = L.Position - worldPos;
        float3 Ldir = normalize(toLight);
        float projDist = dot(-toLight, L.Direction);
        float atten = PointLightAttenuation(projDist * projDist, L.SquaredRadius);
        float cosAngle = dot(-Ldir, L.Direction);
        atten *= SpotLightAttenuation(cosAngle, L.InnerAngle, L.OuterAngle);
        float phase = PhaseHG_Schlick(dot(rayDir, Ldir), AnisotropyG);
        float vis = 1.0f;
        if (SpotShadowParams.x > 0.5f && distance(L.Position, SpotShadowPos.xyz) < 0.05f)
            vis = SampleSpotShadow(SpotShadowMap, ShadowCmp, worldPos, SpotViewProj,
                                   SpotShadowParams.y, SpotShadowParams.z, SpotShadowParams.w);
        result += L.Color * L.Intensity * atten * phase * vis;
    }

    return result;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID){
    if (dtid.x >= ViewportWidth || dtid.y >= ViewportHeight) return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(ViewportWidth, ViewportHeight);
    float depth = GBufferDepth.SampleLevel(PointClamp, uv, 0);
    float3 worldPos = ReconstructWorldPosFog(uv, depth, InvViewProj);

    float3 rayVec = worldPos - CameraPosition;
    float dist = length(rayVec);
    float3 marchDir = (dist > 1e-5f) ? (rayVec / dist) : float3(0.0f, 0.0f, 1.0f);

    uint steps = max(NumSteps, 1u);
    float stepSize = dist / float(steps);
    float3 marchStep = marchDir * stepSize;

    float ign = SampleIGN(uv * float2(ViewportWidth, ViewportHeight), (float)FrameIndex);
    float3 currentPos = CameraPosition + marchStep * ign;

    uint2 fullPixel = uint2(uv * float2(FullViewportWidth, FullViewportHeight));
    uint tileIdx = GetTileIndex(min(fullPixel, uint2(FullViewportWidth - 1, FullViewportHeight - 1)));

    float lightT0 = 0.0f;
    float lightT1 = dist;
    if (BoundedRayLength != 0u){
        lightT0 = 1e9f;
        lightT1 = -1e9f;
        for (uint pi = 0; pi < MAX_LIGHTS_PER_TILE; ++pi){
            int idx = PointLightIndices[tileIdx * MAX_LIGHTS_PER_TILE + pi];
            if (idx < 0) break;
            PointLight L = PointLights[idx];
            float t0, t1;
            if (RaySphereIntersect(CameraPosition, marchDir, L.Position, sqrt(L.SquaredRadius), t0, t1)){
                lightT0 = min(lightT0, t0);
                lightT1 = max(lightT1, t1);
            }
        }
        for (uint si = 0; si < MAX_LIGHTS_PER_TILE; ++si){
            int idx = SpotLightIndices[tileIdx * MAX_LIGHTS_PER_TILE + si];
            if (idx < 0) break;
            SpotLight L = SpotLights[idx];
            float3 sphereC; float sphereR;
            SpotBoundingSphere(L, sphereC, sphereR);
            float t0, t1;
            if (RaySphereIntersect(CameraPosition, marchDir, sphereC, sphereR, t0, t1)){
                lightT0 = min(lightT0, t0);
                lightT1 = max(lightT1, t1);
            }
        }
        lightT0 = clamp(lightT0, 0.0f, dist);
        lightT1 = clamp(lightT1, 0.0f, dist);
    }

    float extCoeff = 0.0f;
    float3 accInScattering = float3(0.0f, 0.0f, 0.0f);
    float t = 0.0f;
    [loop]
    for (uint i = 0; i < steps; ++i){
        extCoeff += CalculateExtinctionCoeff(currentPos) * stepSize;
        bool withinLightBounds = (t >= lightT0 && t <= lightT1);
        float3 inScatter = AccumulateInScattering(currentPos, marchDir, tileIdx, withinLightBounds) * stepSize;
        accInScattering += inScatter * exp(-extCoeff);
        currentPos += marchStep;
        t += stepSize;
    }

    float transmittance = saturate(FogIntensity * exp(-extCoeff));
    OutputFog[dtid.xy] = float4(accInScattering * FogIntensity, transmittance);
}
