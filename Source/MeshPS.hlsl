struct DirectionalLight
{
    float3 direction;
    float intensity;
    float3 color;
    float pad;
};

struct PointLight
{
    float3 position;
    float sqRadius;
    float3 color;
    float intensity;
};

struct SpotLight
{
    float3 position;
    float sqRadius;
    float3 direction;
    float innerCos;
    float3 color;
    float outerCos;
    float intensity;
    float3 pad;
};

#define MAX_DIR_LIGHTS   4
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS  8

cbuffer LightCB : register(b2)
{
    float3 ambientColor;
    float ambientIntensity;
    float3 viewPos;
    float pad0;
    uint numDirLights;
    uint numPointLights;
    uint numSpotLights;
    uint pad1;
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
};

cbuffer MaterialCB : register(b3)
{
    float4 baseColor;
    float metallic;
    float roughness;
    uint hasBaseColorTexture;
    uint padding;
};

Texture2D baseColorTexture : register(t0);
SamplerState textureSampler : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 worldPos : POSITION;
    float3 nrm : NORMAL;
};

float3 blinnPhong(float3 N, float3 L, float3 V, float3 color, float intensity)
{
    float NdotL = max(dot(N, L), 0.0f);
    float3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0f), 32.0f) * (1.0f - roughness);
    return color * intensity * (NdotL + spec * 0.3f);
}

float4 main(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.nrm);
    float3 V = normalize(viewPos - input.worldPos);

    float4 albedo = baseColor;
    if (hasBaseColorTexture)
        albedo = baseColorTexture.Sample(textureSampler, input.uv);

    float3 result = ambientColor * ambientIntensity * albedo.rgb;

    // --- Directional lights ---
    for (uint i = 0; i < numDirLights; ++i)
    {
        float3 L = normalize(-dirLights[i].direction);
        result += blinnPhong(N, L, V, dirLights[i].color, dirLights[i].intensity) * albedo.rgb;
    }

    // --- Point lights ---
    for (uint j = 0; j < numPointLights; ++j)
    {
        float3 toLight = pointLights[j].position - input.worldPos;
        float distSq = dot(toLight, toLight);
        float atten = max(0.0f, 1.0f - distSq / pointLights[j].sqRadius);
        float3 L = normalize(toLight);
        result += blinnPhong(N, L, V, pointLights[j].color, pointLights[j].intensity) * atten * albedo.rgb;
    }

    // --- Spot lights ---
    for (uint k = 0; k < numSpotLights; ++k)
    {
        float3 toLight = spotLights[k].position - input.worldPos;
        float distSq = dot(toLight, toLight);
        float atten = max(0.0f, 1.0f - distSq / spotLights[k].sqRadius);
        float3 L = normalize(toLight);
        float cosAngle = dot(-L, normalize(spotLights[k].direction));
        float cone = smoothstep(spotLights[k].outerCos, spotLights[k].innerCos, cosAngle);
        result += blinnPhong(N, L, V, spotLights[k].color, spotLights[k].intensity) * atten * cone * albedo.rgb;
    }

    return float4(result, albedo.a);
}