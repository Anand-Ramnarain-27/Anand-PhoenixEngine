// MeshPS.hlsl

cbuffer MaterialCB : register(b2)
{
    float4 baseColor;
    float metallic;
    float roughness;
    float2 padding;
};

Texture2D baseColorTexture : register(t0);
SamplerState textureSampler : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 nrm : NORMAL;
};

float4 main(PSInput input) : SV_TARGET
{
    // Normalize the normal
    float3 N = normalize(input.nrm);
    
    // Simple directional light
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float NdotL = max(dot(N, -lightDir), 0.0f);
    
    // Ambient + diffuse lighting
    float3 ambient = float3(0.3f, 0.3f, 0.3f);
    float3 diffuse = float3(0.7f, 0.7f, 0.7f) * NdotL;
    
    // Sample texture (or use white if no texture)
    float4 albedo = float4(1.0f, 1.0f, 1.0f, 1.0f);
    // TODO: Add texture sampling when we load textures
    // albedo = baseColorTexture.Sample(textureSampler, input.uv);
    
    // Combine lighting
    float3 finalColor = albedo.rgb * (ambient + diffuse);
    
    return float4(finalColor, 1.0f);
}