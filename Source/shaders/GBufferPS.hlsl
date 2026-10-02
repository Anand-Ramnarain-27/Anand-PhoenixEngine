#include "GBufferPass.hlsli"

struct PSInput {
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT;
    float4 svPos : SV_Position;
    bool isFrontFace : SV_IsFrontFace;
};

struct PSOutput {
    float4 albedo : SV_TARGET0;
    float4 normalMetalRough : SV_TARGET1;
    float4 emissiveAO : SV_TARGET2;
};

PSOutput main(PSInput input){
    const float2 uv = input.uv + InstanceMaterial.UVOffset;

    ClipAlphaMask(InstanceMaterial, BaseColorTex, uv);
    ApplyOcclusionCut(input.worldPos, input.svPos.xy, InstanceMaterial.Flags);

    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent.xyz);
    float3 B = normalize(cross(N, T)) * input.tangent.w;
    N = SampleNormal(InstanceMaterial, NormalTex, uv, N, T, B);
    if (!input.isFrontFace) N = -N;

    float3 baseColor;
    float roughness, alphaRoughness, metallic;
    SampleMetallicRoughness(InstanceMaterial, BaseColorTex, MetalRoughTex,
                             uv, baseColor, roughness, alphaRoughness, metallic);

    float diffuseAO, specularAO;
    SampleAmbientOcclusion(InstanceMaterial, OcclusionTex, uv,
                            1.0f, 1.0f, roughness, diffuseAO, specularAO);

    float3 emissive = SampleEmissive(InstanceMaterial, EmissiveTex, uv);
    emissive += VfxGlow(InstanceMaterial, N, normalize(OccCameraPos - input.worldPos));

    PSOutput o;
    o.albedo = float4(baseColor, 1.0f);
    o.normalMetalRough = float4(OctEncode(N), metallic, roughness);
    o.emissiveAO = float4(emissive, diffuseAO);
    return o;
}
