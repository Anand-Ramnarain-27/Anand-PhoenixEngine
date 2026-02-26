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
    DirectionalLight dirLights[2];
    PointLight pointLight;
    SpotLight spotLight;
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

float D_GGX(float NdotH, float alpha)
{
    float a2 = alpha * alpha;
    float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-7f);
}

float V_SmithGGX(float NdotV, float NdotL, float alpha)
{
    float k = (alpha + 1.0f) * (alpha + 1.0f) / 8.0f; 
    float gV = NdotV / max(NdotV * (1.0f - k) + k, 1e-6f);
    float gL = NdotL / max(NdotL * (1.0f - k) + k, 1e-6f);
    return gV * gL;
}

float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(max(1.0f - cosTheta, 0.0f), 5.0f);
}

float3 F_SchlickRoughness(float cosTheta, float3 F0, float rough)
{
    return F0 + (max(float3(1.0f - rough, 1.0f - rough, 1.0f - rough), F0) - F0)
              * pow(max(1.0f - cosTheta, 0.0f), 5.0f);
}

float3 cookTorrance(float3 N, float3 L, float3 V,
                    float3 albedoColor, float metal, float rough,
                    float3 lightColor, float lightIntensity)
{
    float3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0f);
    float NdotV = max(dot(N, V), 1e-4f);
    float NdotH = max(dot(N, H), 0.0f);
    float VdotH = max(dot(V, H), 0.0f);

    if (NdotL <= 0.0f)
        return 0.0f;

    float alpha = max(rough * rough, 0.001f);
    
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoColor, metal);

    float D = D_GGX(NdotH, alpha);
    float Vis = V_SmithGGX(NdotV, NdotL, alpha);
    float3 F = F_Schlick(VdotH, F0);
    
    float3 spec = D * Vis * F;
    
    float3 kd = (1.0f - F) * (1.0f - metal);
    float3 diff = kd * albedoColor / PI;

    return (diff + spec) * lightColor * lightIntensity * NdotL;
}

float3 iblAmbient(float3 N, float3 V, float3 albedoColor, float metal, float rough)
{
    float NdotV = max(dot(N, V), 1e-4f);
    float3 R = reflect(-V, N);

    float alpha = max(rough * rough, 0.001f);
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoColor, metal);
    
    float3 irradiance = irradianceMap.Sample(textureSampler[0], N).rgb;
    float3 F_diff = F_SchlickRoughness(NdotV, F0, rough);
    float3 kd_diff = (1.0f - F_diff) * (1.0f - metal);
    float3 diffuse = kd_diff * irradiance * albedoColor;
    
    float mipLevel = rough * (spotLight.numRoughnessLevels - 1.0f);
    float3 prefilter = prefilteredMap.SampleLevel(textureSampler[0], R, mipLevel).rgb;
    
    float2 fab = brdfLUT.SampleLevel(textureSampler[0], float2(NdotV, rough), 0).rg;
    float3 specular = prefilter * (F0 * fab.x + fab.y);

    float3 iblResult = diffuse + specular;
    float iblLum = luminance(iblResult);
    float3 neutralFloor = albedoColor * 0.15f; 
    float blendT = saturate(iblLum / 0.05f);
    return lerp(neutralFloor, iblResult, blendT);
    
    return diffuse + specular;
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
    float rough = max(roughness, 0.04f);
    
    float3 directLight = 0.0f;

    for (uint i = 0; i < numDirLights; ++i)
    {
        float3 L = normalize(-dirLights[i].direction);
        directLight += cookTorrance(N, L, V, albedoLinear, metal, rough,
                                    dirLights[i].color, dirLights[i].intensity);
    }

    if (numPointLights > 0)
    {
        float3 toLight = pointLight.position - input.worldPos;
        float distSq = dot(toLight, toLight);
        float atten = max(0.0f, 1.0f - distSq / max(pointLight.sqRadius, 1e-6f));
        float3 L = normalize(toLight);
        directLight += cookTorrance(N, L, V, albedoLinear, metal, rough,
                                    pointLight.color, pointLight.intensity) * atten;
    }

    if (numSpotLights > 0)
    {
        float3 toLight = spotLight.position - input.worldPos;
        float distSq = dot(toLight, toLight);
        float atten = max(0.0f, 1.0f - distSq / max(spotLight.sqRadius, 1e-6f));
        float3 L = normalize(toLight);
        float cosAng = dot(-L, normalize(spotLight.direction));
        float cone = smoothstep(spotLight.outerCos, spotLight.innerCos, cosAng);
        directLight += cookTorrance(N, L, V, albedoLinear, metal, rough,
                                    spotLight.color, spotLight.intensity) * atten * cone;
    }
    
    float3 ambient;

    if (iblEnabled)
    {
        ambient = iblAmbient(N, V, albedoLinear, metal, rough);
    }
    else
    {
        ambient = ambientColor * ambientIntensity * albedoLinear;
    }

    float3 result = ambient + directLight;

    return float4(result, albedo.a);
}
