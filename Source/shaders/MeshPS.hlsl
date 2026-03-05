#define MAX_DIR_LIGHTS 4
#define MAX_POINT_LIGHTS 32
#define MAX_SPOT_LIGHTS 16

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
    float numRoughnessLevels;
    float2 pad;
};

cbuffer LightCB : register(b2)
{
    float3 ambientColor;
    float ambientIntensity;

    float3 viewPos;
    float pad0;

    uint numDirLights;
    uint numPointLights;
    uint numSpotLights;
    uint iblEnabled;

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
    uint samplerIndex;
};

Texture2D baseColorTexture : register(t0);
TextureCube irradianceMap : register(t1);
TextureCube prefilteredMap : register(t2);
Texture2D brdfLUT : register(t3);

SamplerState textureSampler[4] : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 worldPos : POSITION;
    float3 nrm : NORMAL;
};

#define PI 3.14159265359f

float luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 cookTorrance(float3 N, float3 L, float3 V,
                    float3 albedoColor, float metal, float rough,
                    float3 lightColor, float lightIntensity)
{
    float3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if (NdotL <= 0)
        return 0;

    float alpha = max(rough * rough, 0.001);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedoColor, metal);

    float a2 = alpha * alpha;
    float d = NdotH * NdotH * (a2 - 1) + 1;
    float D = a2 / (PI * d * d);

    float k = (alpha + 1) * (alpha + 1) / 8;
    float gV = NdotV / (NdotV * (1 - k) + k);
    float gL = NdotL / (NdotL * (1 - k) + k);
    float Vis = gV * gL;

    float3 F = F0 + (1 - F0) * pow(1 - VdotH, 5);

    float3 spec = D * Vis * F;

    float3 kd = (1 - F) * (1 - metal);
    float3 diff = kd * albedoColor / PI;

    return (diff + spec) * lightColor * lightIntensity * NdotL;
}

float4 main(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.nrm);
    float3 V = normalize(viewPos - input.worldPos);

    float4 albedo = baseColor;
    if (hasBaseColorTexture)
        albedo = baseColorTexture.Sample(textureSampler[samplerIndex], input.uv);

    float3 albedoLinear = albedo.rgb;

    float metal = metallic;
    float rough = max(roughness, 0.04);

    float3 directLight = 0;

    for (uint i = 0; i < numDirLights; i++)
    {
        float3 L = normalize(-dirLights[i].direction);

        directLight += cookTorrance(
            N, L, V, albedoLinear, metal, rough,
            dirLights[i].color, dirLights[i].intensity
        );
    }

    for (uint i = 0; i < numPointLights; i++)
    {
        float3 toLight = pointLights[i].position - input.worldPos;
        float distSq = dot(toLight, toLight);

        float atten = max(0, 1 - distSq / max(pointLights[i].sqRadius, 1e-6));

        float3 L = normalize(toLight);

        directLight += cookTorrance(
            N, L, V, albedoLinear, metal, rough,
            pointLights[i].color, pointLights[i].intensity
        ) * atten;
    }

    for (uint i = 0; i < numSpotLights; i++)
    {
        float3 toLight = spotLights[i].position - input.worldPos;
        float distSq = dot(toLight, toLight);

        float atten = max(0, 1 - distSq / max(spotLights[i].sqRadius, 1e-6));

        float3 L = normalize(toLight);

        float cosAng = dot(-L, normalize(spotLights[i].direction));
        float cone = smoothstep(spotLights[i].outerCos,
                                spotLights[i].innerCos,
                                cosAng);

        directLight += cookTorrance(
            N, L, V, albedoLinear, metal, rough,
            spotLights[i].color, spotLights[i].intensity
        ) * atten * cone;
    }

    float3 ambient = ambientColor * ambientIntensity * albedoLinear;

    float3 result = ambient + directLight;

    return float4(result, albedo.a);
}