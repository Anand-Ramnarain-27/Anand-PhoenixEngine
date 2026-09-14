#include "FogCommon.hlsli"
#include "Noise.hlsli"
#include "Lights.hlsli"
#include "Shadows.hlsli"
#include "Samplers.hlsli"

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
    float AnisotropyG;
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

// Sum of Li(x, L) * Vis(x, L) * phase(rayDir, L) over all lights, at world-space point worldPos.
// rayDir is the camera-to-point ray direction (the lecture's -V).
float3 AccumulateInScattering(float3 worldPos, float3 rayDir){
    float3 result = float3(0.0f, 0.0f, 0.0f);

    float dirShadow = SampleDirectionalShadow(worldPos);
    for (uint i = 0; i < DirLightCount; ++i){
        DirectionalLight L = DirLights[i];
        float3 Ldir = normalize(-L.Direction);
        float phase = PhaseHG_Schlick(dot(rayDir, Ldir), AnisotropyG);
        float vis = (i == 0) ? dirShadow : 1.0f;
        result += L.Color * L.Intensity * phase * vis;
    }

    for (uint p = 0; p < PointLightCount; ++p){
        PointLight L = PointLights[p];
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

    for (uint s = 0; s < SpotLightCount; ++s){
        SpotLight L = SpotLights[s];
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
    float3 accInScattering = float3(0.0f, 0.0f, 0.0f);
    [loop]
    for (uint i = 0; i < steps; ++i){
        extCoeff += CalculateExtinctionCoeff(currentPos) * stepSize;
        float3 inScatter = AccumulateInScattering(currentPos, marchDir) * stepSize;
        accInScattering += inScatter * exp(-extCoeff);
        currentPos += marchStep;
    }

    float transmittance = saturate(FogIntensity * exp(-extCoeff));
    OutputFog[dtid.xy] = float4(accInScattering * FogIntensity, transmittance);
}
