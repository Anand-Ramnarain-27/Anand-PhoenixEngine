
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
    
    float4 diffuseColour;
    float4 specularColour;
    float Kd;
    float Ks;
    float shininess;
    bool hasDiffuseTex;
};


