#define MAX_DIR_LIGHTS   4
#define MAX_POINT_LIGHTS 32
#define MAX_SPOT_LIGHTS  16

#define PI 3.14159265359f

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

    uint hasMetalRoughMap;
    float3 emissiveFactor;

    uint samplerIndex;
};

Texture2D albedoTex : register(t0);
TextureCube irradianceMap : register(t1);
TextureCube prefilteredMap : register(t2);
Texture2D brdfLUT : register(t3);
Texture2D normalMapTex : register(t4);
Texture2D aoTex : register(t5);
Texture2D emissiveTex : register(t6);
Texture2D metalRoughTex : register(t7);

SamplerState samplers[4] : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 worldPos : POSITION;
    float3 nrm : NORMAL;
    float3 tangent : TANGENT;
};

static const float GAMMA = 2.2f;
static const float INV_GAMMA = 1.0f / GAMMA;

float3 linearToSRGB(float3 c)
{
    return pow(c, INV_GAMMA);
}
float3 sRGBToLinear(float3 c)
{
    return pow(c, GAMMA);
}

float3 PBRNeutralToneMapping(float3 color)
{
    const float startCompression = 0.8f - 0.04f;
    const float desaturation = 0.15f;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression)
        return color;

    const float d = 1.0f - startCompression;
    float newPeak = 1.0f - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0f - 1.0f / (desaturation * (peak - newPeak) + 1.0f);
    return lerp(color, newPeak, g);
}

#define VARIANCE  0.3f
#define THRESHOLD 0.2f

float getGeometricSpecularAA(float3 N, float roughness)
{
    float3 ndx = ddx(N);
    float3 ndy = ddy(N);

    float curvature = max(dot(ndx, ndx), dot(ndy, ndy));
    float geomRoughnessOffset = pow(curvature, 0.333f) * VARIANCE;
    geomRoughnessOffset = min(geomRoughnessOffset, THRESHOLD);

    return saturate(roughness + geomRoughnessOffset);
}

float3 F_Schlick(float3 f0, float3 f90, float cosTheta)
{
    return f0 + (f90 - f0) * pow(1.0f - cosTheta, 5.0f);
}

float F_Schlick(float f0, float f90, float cosTheta)
{
    return f0 + (f90 - f0) * pow(1.0f - cosTheta, 5.0f);
}

float D_GGX(float alphaRoughness, float NdotH)
{
    float a2 = alphaRoughness * alphaRoughness;
    float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return (denom > 0.0f) ? a2 / (PI * denom * denom) : 0.0f;
}

float V_GGX(float NdotV, float NdotL, float alphaRoughness)
{
    float a2 = alphaRoughness * alphaRoughness;
    float gV = NdotL * sqrt(NdotV * NdotV * (1.0f - a2) + a2);
    float gL = NdotV * sqrt(NdotL * NdotL * (1.0f - a2) + a2);
    float denom = gV + gL;
    return (denom > 0.0f) ? 0.5f / denom : 0.0f;
}

float3 applyNormalMap(float3 N, float3 T, float2 uv)
{
    float3 tn = normalMapTex.Sample(samplers[0], uv).rgb * 2.0f - 1.0f;
    tn.xy *= normalStrength;
    tn = normalize(tn);

    float3 B = normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(tn, TBN));
}

float computeSpecularAO(float NdotV, float ao, float rough)
{
    return saturate(pow(NdotV + ao, exp2(-16.0f * rough - 1.0f)) - 1.0f + ao);
}

float3 getDiffuseAmbientLight(float3 N, float3 baseColour)
{
    return irradianceMap.Sample(samplers[2], N).rgb * baseColour;
}

void getSpecularAmbientLightNoFresnel(float3 R, float NdotV, float rough,
                                      out float3 firstTerm, out float3 secondTerm)
{
    float mip = rough * (numRoughnessLevels - 1.0f);
    float3 radiance = prefilteredMap.SampleLevel(samplers[2], R, mip).rgb;
    float2 fab = brdfLUT.Sample(samplers[2], float2(NdotV, rough)).rg;
    
    firstTerm = radiance * fab.x; 
    secondTerm = radiance * fab.y; 
}

float3 computeIBL(float3 N, float3 V, float3 R,
                  float3 baseColour, float metal, float rough,
                  float diffuseAO, float specularAO)
{
    float NdotV = saturate(dot(N, V));

    float3 diffuse = getDiffuseAmbientLight(N, baseColour) * diffuseAO;

    float3 firstTerm, secondTerm;
    getSpecularAmbientLightNoFresnel(R, NdotV, rough, firstTerm, secondTerm);
    
    float3 metalF0 = baseColour;
    float3 dielectricF0 = float3(0.04f, 0.04f, 0.04f);

    float3 metalSpecular = (metalF0 * firstTerm + secondTerm) * specularAO;
    float3 dielectricSpecular = (dielectricF0 * firstTerm + secondTerm) * specularAO;

    return lerp(diffuse + dielectricSpecular, metalSpecular, metal);
}

float3 cookTorranceGGX(float3 N, float3 L, float3 V,
                        float3 albedo, float metal, float alphaRough,
                        float3 lightColor, float lightIntensity)
{
    float3 H = normalize(L + V);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    if (NdotL <= 0.0f)
        return 0.0f;

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metal);
    float3 metalF = F_Schlick(F0, 1.0f, VdotH);
    float dielecF = F_Schlick(0.04f, 1.0f, VdotH);

    float D = D_GGX(alphaRough, NdotH);
    float Vis = V_GGX(NdotV, NdotL, alphaRough);

    float3 spec = D * Vis * NdotL * lightColor * lightIntensity;
    float3 diff = (albedo / PI) * NdotL * lightColor * lightIntensity;

    float3 dielectric = diff * (1.0f - dielecF) + spec * dielecF;
    float3 metallic = spec * metalF;

    return lerp(dielectric, metallic, metal);
}

float4 main(PSInput input) : SV_TARGET
{
    float3 albedo = baseColor.rgb;
    if (hasBaseColorTexture)
        albedo *= sRGBToLinear(albedoTex.Sample(samplers[0], input.uv).rgb);
    
    float metal = metallic;
    float rough = roughness;
    if (hasMetalRoughMap)
    {
        float2 mr = metalRoughTex.Sample(samplers[0], input.uv).gb;
        rough *= mr.x;
        metal *= mr.y;
    }
    rough = max(rough, 0.04f);
    
    float3 Ngeom = normalize(input.nrm);
    rough = getGeometricSpecularAA(Ngeom, rough);
    float alphaRough = rough * rough;
    
    float3 N = Ngeom;
    if (hasNormalMap)
        N = applyNormalMap(normalize(input.nrm), normalize(input.tangent), input.uv);

    float3 V = normalize(viewPos - input.worldPos);
    float3 R = reflect(-V, N);
    float NdotV = saturate(dot(N, V));
    float NdotR = saturate(dot(N, R));
    
    float diffuseAO = 1.0f;
    float specularAO = 1.0f;
    if (hasAOMap)
    {
        diffuseAO = lerp(1.0f, aoTex.Sample(samplers[0], input.uv).r, aoStrength);
        specularAO = computeSpecularAO(NdotV, diffuseAO, rough);
        specularAO *= max(1.0f + NdotR, 1.0f);
    }
    
    float3 directLight = 0.0f;

    for (uint i = 0; i < numDirLights; i++)
    {
        float3 L = normalize(-dirLights[i].direction);
        directLight += cookTorranceGGX(N, L, V, albedo, metal, alphaRough,
                                       dirLights[i].color, dirLights[i].intensity);
    }

    for (uint i = 0; i < numPointLights; i++)
    {
        float3 toLight = pointLights[i].position - input.worldPos;
        float distSq = dot(toLight, toLight);
        float num = max(1.0f - (distSq * distSq) / (pointLights[i].sqRadius * pointLights[i].sqRadius), 0.0f);
        float atten = (num * num) / (distSq + 1.0f);
        float3 L = normalize(toLight);
        directLight += cookTorranceGGX(N, L, V, albedo, metal, alphaRough,
                                       pointLights[i].color, pointLights[i].intensity) * atten;
    }

    for (uint i = 0; i < numSpotLights; i++)
    {
        float3 toLight = spotLights[i].position - input.worldPos;
        float3 L = normalize(toLight);
        float dist = dot(-toLight, spotLights[i].direction);
        float distSq = dist * dist;
        float num = max(1.0f - (distSq * distSq) / (spotLights[i].sqRadius * spotLights[i].sqRadius), 0.0f);
        float atten = (num * num) / (distSq + 1.0f);
        float cosAng = dot(-L, normalize(spotLights[i].direction));
        float angleAtt = saturate((cosAng - spotLights[i].outerCos) /
                                   (spotLights[i].innerCos - spotLights[i].outerCos));
        atten *= angleAtt * angleAtt;
        directLight += cookTorranceGGX(N, L, V, albedo, metal, alphaRough,
                                       spotLights[i].color, spotLights[i].intensity) * atten;
    }
    
    float3 ambient = 0.0f;
    if (iblEnabled)
    {
        ambient = computeIBL(N, V, R, albedo, metal, rough, diffuseAO, specularAO);
    }
    else
    {
        ambient = ambientColor * ambientIntensity * albedo;
        ambient = max(ambient, albedo * 0.03f);
    }
    
    float3 emissive = 0.0f;
    if (hasEmissiveMap)
        emissive = sRGBToLinear(emissiveTex.Sample(samplers[0], input.uv).rgb) * emissiveFactor;

    float3 colour = emissive + ambient + directLight;

    float3 ldr = PBRNeutralToneMapping(colour);
    return float4(linearToSRGB(ldr), baseColor.a);
}
