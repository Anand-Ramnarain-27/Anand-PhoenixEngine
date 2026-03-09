#define MAX_DIR_LIGHTS   4
#define MAX_POINT_LIGHTS 32
#define MAX_SPOT_LIGHTS  16

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
    float pad0; 
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
    
    float numRoughnessLevels;
    float3 pad1;

    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
};

cbuffer MaterialCB : register(b3)
{
    float4 baseColor;
    float metallic;
    float roughness;
    float normalStrength; 
    float aoStrength;

    uint hasBaseColorTexture;
    uint hasNormalMap;
    uint hasAOMap; 
    uint hasEmissiveMap;

    float3 emissiveFactor;
    uint samplerIndex;
};

Texture2D baseColorTexture : register(t0);
TextureCube irradianceMap : register(t1);
TextureCube prefilteredMap : register(t2);
Texture2D brdfLUT : register(t3);
Texture2D normalMap : register(t4);
Texture2D aoMap : register(t5); 
Texture2D emissiveMap : register(t6); 

SamplerState textureSampler[4] : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 worldPos : POSITION;
    float3 nrm : NORMAL;
    float4 tangent : TANGENT; 
};

#define PI 3.14159265359f

float3 applyNormalMap(float3 worldNrm, float4 worldTangent, float2 uv, uint sampIdx)
{
    float3 tn = normalMap.Sample(textureSampler[sampIdx], uv).rgb;
    tn = tn * 2.0f - 1.0f;
    
    tn.xy *= normalStrength;
    tn = normalize(tn);

    float3 N = normalize(worldNrm);
    float3 T = normalize(worldTangent.xyz);
    float3 B = cross(N, T) * worldTangent.w; 
    
    return normalize(T * tn.x + B * tn.y + N * tn.z);
}

float computeSpecularAO(float NdotV, float diffuseAO, float rough)
{
    return saturate(pow(NdotV + diffuseAO, exp2(-16.0f * rough - 1.0f)) - 1.0f + diffuseAO);
}

float luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
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

    if (NdotL <= 0)
        return 0;

    float alpha = max(rough * rough, 0.001f);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoColor, metal);

    float a2 = alpha * alpha;
    float d = NdotH * NdotH * (a2 - 1) + 1;
    float D = a2 / (PI * d * d);

    float k = (alpha + 1) * (alpha + 1) / 8.0f;
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
    uint sampIdx = samplerIndex;
    
    float4 albedo = baseColor;
    if (hasBaseColorTexture)
        albedo *= baseColorTexture.Sample(textureSampler[sampIdx], input.uv);

    float3 albedoLinear = albedo.rgb;
    float metal = metallic;
    float rough = max(roughness, 0.04f);
    
    float3 N = normalize(input.nrm);
    if (hasNormalMap)
        N = applyNormalMap(input.nrm, input.tangent, input.uv, sampIdx);

    float3 V = normalize(viewPos - input.worldPos);
    float NdotV = max(dot(N, V), 1e-4f);
    
    float diffuseAO = 1.0f;
    float specularAO = 1.0f;
    if (hasAOMap)
    {
        diffuseAO = aoMap.Sample(textureSampler[sampIdx], input.uv).r;
        diffuseAO = lerp(1.0f, diffuseAO, aoStrength); 
        specularAO = computeSpecularAO(NdotV, diffuseAO, rough);
        
        float3 R = reflect(-V, N);
        float horizon = min(1.0f + dot(R, normalize(input.nrm)), 1.0f);
        specularAO *= horizon * horizon;
    }
    
    float3 directLight = 0;

    for (uint i = 0; i < numDirLights; i++)
    {
        float3 L = normalize(-dirLights[i].direction);
        directLight += cookTorrance(N, L, V, albedoLinear, metal, rough,
                                    dirLights[i].color, dirLights[i].intensity);
    }

    for (uint i = 0; i < numPointLights; i++)
    {
        float3 toLight = pointLights[i].position - input.worldPos;
        float distSq = dot(toLight, toLight);
        float atten = max(0.0f, 1.0f - distSq / max(pointLights[i].sqRadius, 1e-6f));
        float3 L = normalize(toLight);
        directLight += cookTorrance(N, L, V, albedoLinear, metal, rough,
                                    pointLights[i].color, pointLights[i].intensity) * atten;
    }

    for (uint i = 0; i < numSpotLights; i++)
    {
        float3 toLight = spotLights[i].position - input.worldPos;
        float distSq = dot(toLight, toLight);
        float atten = max(0.0f, 1.0f - distSq / max(spotLights[i].sqRadius, 1e-6f));
        float3 L = normalize(toLight);
        float cosAng = dot(-L, normalize(spotLights[i].direction));
        float cone = smoothstep(spotLights[i].outerCos, spotLights[i].innerCos, cosAng);
        directLight += cookTorrance(N, L, V, albedoLinear, metal, rough,
                                    spotLights[i].color, spotLights[i].intensity) * atten * cone;
    }
    
    float3 ambient = ambientColor * ambientIntensity * albedoLinear;

    if (iblEnabled)
    {
        float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoLinear, metal);
        float3 kS = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);
        float3 kD = (1.0f - kS) * (1.0f - metal);

        float3 irradiance = irradianceMap.Sample(textureSampler[0], N).rgb;
        float3 diffuseIBL = kD * albedoLinear * irradiance * diffuseAO;

        float3 R = reflect(-V, N);
        float mipLevel = rough * numRoughnessLevels;
        float3 prefiltered = prefilteredMap.SampleLevel(textureSampler[0], R, mipLevel).rgb;
        float2 brdf = brdfLUT.Sample(textureSampler[1], float2(NdotV, rough)).rg;
        float3 specularIBL = prefiltered * (F0 * brdf.x + brdf.y) * specularAO;
        
        float3 iblAmbient = diffuseIBL + specularIBL;
        ambient = max(ambient, iblAmbient); 
    }
    
    ambient = max(ambient, albedoLinear * 0.03f);

    float3 result = ambient + directLight;
    
    if (hasEmissiveMap)
    {
        float3 emissive = emissiveMap.Sample(textureSampler[sampIdx], input.uv).rgb;
        result += emissive * emissiveFactor;
    }

    return float4(result, albedo.a);
}
