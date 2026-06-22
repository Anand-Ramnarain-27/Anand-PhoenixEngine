
cbuffer CubeMVP : register(b0){
    float4x4 WorldLightViewProj;
    float4x4 World;
    float3 LightPos;
    float InvRange;
};

struct VSOut {
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
};

VSOut main(float3 position : POSITION, float2 uv : TEXCOORD,
           float3 normal : NORMAL, float4 tangent : TANGENT){
    VSOut o;
    o.position = mul(float4(position, 1.0f), WorldLightViewProj);
    o.worldPos = mul(float4(position, 1.0f), World).xyz;
    return o;
}
