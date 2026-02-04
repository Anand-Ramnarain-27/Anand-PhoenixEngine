cbuffer PerFrame : register(b1)
{
    float3 L; 
    float3 Lc; 
    float3 Ac; 
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

static const float Kd = 0.8f;
static const float Ks = 0.5f;
static const float shininess = 32.0f; 