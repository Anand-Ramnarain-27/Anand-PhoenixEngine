#include "ForwardPass.hlsli" 

struct PSOutput
{
    float4 albedo : SV_Target0; // RGB = albedo colour, A = unused
    float4 normalMR : SV_Target1; // RGB = world-space normal (encoded),
                                     // A   = roughness
    float4 emissiveAO : SV_Target2; // RGB = emissive colour, A = AO
};

PSOutput main(
    float3 worldPos : POSITION,
    float2 texCoord : TEXCOORD,
    float3 normal : NORMAL0,
    float4 tangent : TANGENT)
{
    float3 baseColour;
    float roughness;
    float alphaRoughness;
    float metallic;
    SampleMetallicRoughness(InstanceMaterial, BaseColorTex,
                            MetallicRoughnessTex, texCoord,
                            baseColour, roughness, alphaRoughness, metallic);

    float3 T = normalize(tangent.xyz);
    float3 N = normalize(normal);
    float3 B = normalize(cross(N, T) * tangent.w);
    N = SampleNormal(InstanceMaterial, NormalTex, texCoord, N, T, B);

    float3 emissive = SampleEmissive(InstanceMaterial, EmissiveTex, texCoord).rgb;

    float diffuseAO = 1.0;
    float NdotV = max(dot(N, normalize(CameraPosition - worldPos)), 0.0);
    float NdotR = 0.0;
    float specAO = 1.0;
    SampleAmbientOcclusion(InstanceMaterial, OcclusionTex, texCoord,
                           NdotV, NdotR, roughness, diffuseAO, specAO);
    
    PSOutput o;
    
    o.albedo = float4(baseColour, metallic);
    
    o.normalMR = float4(N * 0.5 + 0.5, roughness);

    o.emissiveAO = float4(emissive, diffuseAO);

    return o;
}
