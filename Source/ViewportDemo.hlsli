static const float PI = 3.14159265;

cbuffer PerFrame : register(b1)
{
    float3 L; // Light direction
    float3 Lc; // Light color
    float3 Ac; // Ambient color
    float3 viewPos;
};

cbuffer PerInstance : register(b2)
{
    float4x4 modelMat;
    float4x4 normalMat;
    
    float4 baseColour;
    bool hasColourTexture;
    float padding[11];
};

// PBR-inspired shading constants
static const float Kd = 0.8f; // Diffuse coefficient
static const float Ks = 0.5f; // Specular coefficient
static const float shininess = 32.0f; // Shininess factor
static const float3 specularColour = float3(0.04, 0.04, 0.04); // Fresnel reflectance at normal incidence