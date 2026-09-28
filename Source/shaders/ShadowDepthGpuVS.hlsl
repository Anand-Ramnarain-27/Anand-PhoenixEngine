
cbuffer WorldCB : register(b0){
    float4x4 World;
};

cbuffer GpuVP : register(b1){
    row_major float4x4 GpuViewProj;
};

struct VSOut {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOut main(float3 position : POSITION, float2 uv : TEXCOORD,
           float3 normal : NORMAL, float4 tangent : TANGENT){
    float4 worldPos = mul(float4(position, 1.0f), World);
    VSOut o;
    o.position = mul(worldPos, GpuViewProj);
    o.uv = uv;
    return o;
}
