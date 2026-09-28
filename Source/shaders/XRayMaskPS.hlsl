#include "GBufferPass.hlsli"

// Scene depth after the GBuffer pass (includes the tagged meshes themselves).
Texture2D<float> SceneDepth : register(t5);

cbuffer XRayMaskCB : register(b3){
    float GroupValue;   // group / 255
    float DepthBias;
};

struct PSInput {
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT;
    float4 svPos : SV_Position;
};

// R = group where this fragment is hidden by scene geometry, G = 1 where it is the visible surface.
// Written with MAX blending, so a pixel with any visible tagged surface ends up with G = 1.
float2 main(PSInput input) : SV_Target {
    // Same cut-out as the GBuffer draw, or alpha-masked cards (hair) would read as hidden.
    ClipAlphaMask(InstanceMaterial, BaseColorTex, input.uv);

    float sceneDepth = SceneDepth.Load(int3(input.svPos.xy, 0));
    bool hidden = input.svPos.z > sceneDepth + DepthBias;
    return hidden ? float2(GroupValue, 0.0f) : float2(0.0f, 1.0f);
}
