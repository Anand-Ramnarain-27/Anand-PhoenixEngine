#ifndef _FORWARD_HLSLI_
#define _FORWARD_HLSLI_

#include "Common.hlsli"
#include "Lights.hlsli"
#include "Material.hlsli"
#include "Samplers.hlsli"
#include "Shadows.hlsli"

cbuffer CbMVP : register(b0){
    float4x4 MVP;
};

cbuffer CbPerFrame : register(b1){
    uint DirLightCount;
    uint PointLightCount;
    uint SpotLightCount;
    uint EnvRoughnessLevels;
    float3 CameraPosition;
    uint FramePad;
    float4x4 DirLightViewProj[MAX_CASCADES];
    float4 DirShadowParams0;
    float4 DirShadowParams1;
};

cbuffer CbPerInstance : register(b2){
    float4x4 ModelMatrix;
    float4x4 NormalMatrix;
    Material InstanceMaterial;
};

StructuredBuffer<DirectionalLight> DirLights : register(t0);
StructuredBuffer<PointLight> PointLights : register(t1);
StructuredBuffer<SpotLight> SpotLights : register(t2);

TextureCube IrradianceMap : register(t3);
TextureCube PrefilteredEnvMap : register(t4);
Texture2D BrdfLUT : register(t5);

Texture2D BaseColorTex : register(t6);
Texture2D MetallicRoughnessTex : register(t7);
Texture2D NormalTex : register(t8);
Texture2D OcclusionTex : register(t9);
Texture2D EmissiveTex : register(t10);
Texture2DArray DirShadowMap : register(t11);

float ComputeForwardDirShadow(float3 worldPos){
    if (DirShadowParams1.x < 0.5f) return 1.0f;
    int cascade;
    return ComputeCascadeShadow(DirShadowMap, ShadowCmp, worldPos, DirLightViewProj,
                                (int)DirShadowParams1.z, DirShadowParams0.x,
                                DirShadowParams0.z, DirShadowParams0.w, cascade);
}

#define VARIANCE  0.3
#define THRESHOLD 0.2

float getGeometricSpecularAA(float3 N, float roughness){
    float3 ndx = ddx(N);
    float3 ndy = ddy(N);
    float curvature = max(dot(ndx, ndx), dot(ndy, ndy));
    float geomRoughnessOffset = pow(curvature, 0.333) * VARIANCE;
    geomRoughnessOffset = min(geomRoughnessOffset, THRESHOLD);
    return saturate(roughness + geomRoughnessOffset);
}

#endif
