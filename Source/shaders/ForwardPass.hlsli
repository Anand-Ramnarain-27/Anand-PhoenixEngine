#ifndef _FORWARD_HLSLI_
#define _FORWARD_HLSLI_

#include "Common.hlsli"
#include "Lights.hlsli"
#include "Material.hlsli"

cbuffer CbMVP : register(b0)
{
    float4x4 MVP;
};

cbuffer CbPerFrame : register(b1)
{
    uint DirLightCount;
    uint PointLightCount;
    uint SpotLightCount;
    uint EnvRoughnessLevels;
    float3 CameraPosition;
    uint FramePad;
};

cbuffer CbPerInstance : register(b2)
{
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

#endif // _FORWARD_HLSLI_
