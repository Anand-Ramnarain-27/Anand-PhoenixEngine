
cbuffer ShadowMVP : register(b0){
    float4x4 WorldLightViewProj;
};

float4 main(float3 position : POSITION,
            float2 uv : TEXCOORD,
            float3 normal : NORMAL,
            float4 tangent : TANGENT) : SV_POSITION {
    return mul(float4(position, 1.0f), WorldLightViewProj);
}
