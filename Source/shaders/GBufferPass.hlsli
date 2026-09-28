#ifndef _GBUFFER_PASS_HLSLI_
#define _GBUFFER_PASS_HLSLI_

#include "Material.hlsli"

cbuffer CbMVP : register(b0){
    float4x4 MVP;
};

cbuffer CbPerInstance : register(b1){
    float4x4 ModelMatrix;
    float4x4 NormalMatrix;
    Material InstanceMaterial;
};

// Occlusion fade (wall cut-out): root constants, set once per pass. Matches OcclusionParams in GBufferPass.h.
cbuffer OcclusionCB : register(b2){
    float3 OccCameraPos;
    float OccRadius;
    float3 OccFocusPos;
    float OccFeather;
    float OccFocusFeetY;
    float OccFloorClearance;
    float OccConeNearScale;
    float OccEnabled;
};

Texture2D BaseColorTex : register(t0);
Texture2D MetalRoughTex : register(t1);
Texture2D NormalTex : register(t2);
Texture2D OcclusionTex : register(t3);
Texture2D EmissiveTex : register(t4);

// Interleaved gradient noise (Jimenez 2014): a stable per-pixel threshold in [0,1).
float DitherIGN(float2 pixel){
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

// Dithers away opaque pixels inside a soft cone between the camera and the focus point, so the focus
// (the player) stays visible through walls. Pixels at or beyond the focus, below the focus' feet plus
// clearance (floors), and draws flagged NO_OCCLUSION_CUT are never cut.
void ApplyOcclusionCut(float3 worldPos, float2 pixel, uint flags){
    if (OccEnabled < 0.5f || (flags & NO_OCCLUSION_CUT))
        return;
    if (worldPos.y <= OccFocusFeetY + OccFloorClearance)
        return;

    float3 seg = OccFocusPos - OccCameraPos;
    float segLenSq = max(dot(seg, seg), 1e-4f);
    float t = saturate(dot(worldPos - OccCameraPos, seg) / segLenSq);
    if (t >= 0.98f)
        return;

    // Narrower toward the camera: world radius grows with distance so the hole keeps a steady size on screen.
    float radius = OccRadius * lerp(OccConeNearScale, 1.0f, t);
    float feather = min(OccFeather, radius);
    float dist = length(worldPos - (OccCameraPos + seg * t));
    float fade = smoothstep(radius, radius - feather, dist);
    // Soften the far end so the cut doesn't end in a hard ring right in front of the focus.
    fade *= saturate((0.98f - t) / 0.08f);

    clip(DitherIGN(pixel) - fade);
}

float2 OctEncode(float3 n){
    float invL1 = 1.0f / (abs(n.x) + abs(n.y) + abs(n.z));
    n *= invL1;
    if (n.z < 0.0f){
        float2 wrapped;
        wrapped.x = (1.0f - abs(n.y)) * (n.x >= 0.0f ? 1.0f : -1.0f);
        wrapped.y = (1.0f - abs(n.x)) * (n.y >= 0.0f ? 1.0f : -1.0f);
        n.xy = wrapped;
    }
    return n.xy * 0.5f + 0.5f;
}

float3 OctDecode(float2 e){
    e = e * 2.0f - 1.0f;
    float3 v = float3(e.x, e.y, 1.0f - abs(e.x) - abs(e.y));
    if (v.z < 0.0f)
        v.xy = (1.0f - abs(v.yx)) * sign(v.xy);
    return normalize(v);
}

#endif
