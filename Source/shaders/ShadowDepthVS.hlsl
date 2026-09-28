
cbuffer ShadowMVP : register(b0){
    float4x4 WorldLightViewProj;
};

struct VSOut {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOut main(float3 position : POSITION,
           float2 uv : TEXCOORD,
           float3 normal : NORMAL,
           float4 tangent : TANGENT){
    VSOut o;
    o.position = mul(float4(position, 1.0f), WorldLightViewProj);
    o.uv = uv;
    return o;
}
