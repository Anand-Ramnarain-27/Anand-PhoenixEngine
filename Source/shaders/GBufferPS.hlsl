#include "GBufferPass.hlsli"

struct PSInput
{
    float3 worldPos : POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL0;
    float4 tangent  : TANGENT;
};

struct PSOutput
{
    float4 albedo           : SV_TARGET0;  // rgb = albedo (linear), a = unused
    float4 normalMetalRough : SV_TARGET1;  // rg = oct-encoded normal, b = metallic, a = roughness
    float4 emissiveAO       : SV_TARGET2;  // rgb = emissive, a = AO
};

// Cigolle et al. octahedral encoding: maps unit sphere to [0,1]^2
float2 OctEncode(float3 n)
{
    float invL1 = 1.0f / (abs(n.x) + abs(n.y) + abs(n.z));
    n *= invL1;
    if (n.z < 0.0f)
    {
        float2 wrapped;
        wrapped.x = (1.0f - abs(n.y)) * (n.x >= 0.0f ? 1.0f : -1.0f);
        wrapped.y = (1.0f - abs(n.x)) * (n.y >= 0.0f ? 1.0f : -1.0f);
        n.xy = wrapped;
    }
    return n.xy * 0.5f + 0.5f;
}

PSOutput main(PSInput input)
{
    // Reconstruct TBN
    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent.xyz);
    float3 B = normalize(cross(N, T)) * input.tangent.w;
    N = SampleNormal(InstanceMaterial, NormalTex, input.uv, N, T, B);

    // Base colour + metal/roughness
    float3 baseColor;
    float  roughness, alphaRoughness, metallic;
    SampleMetallicRoughness(InstanceMaterial, BaseColorTex, MetalRoughTex,
                             input.uv, baseColor, roughness, alphaRoughness, metallic);

    // Ambient occlusion — NdotV/NdotR not available here; pass 1 to get raw AO
    float diffuseAO, specularAO;
    SampleAmbientOcclusion(InstanceMaterial, OcclusionTex, input.uv,
                            1.0f, 1.0f, roughness, diffuseAO, specularAO);

    // Emissive
    float3 emissive = SampleEmissive(InstanceMaterial, EmissiveTex, input.uv);

    PSOutput o;
    o.albedo           = float4(baseColor, 1.0f);
    o.normalMetalRough = float4(OctEncode(N), metallic, roughness);
    o.emissiveAO       = float4(emissive, diffuseAO);
    return o;
}
