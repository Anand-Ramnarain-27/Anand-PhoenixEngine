
cbuffer WorldCB : register(b0){
    float4x4 World;
};

cbuffer GpuVP : register(b1){
    row_major float4x4 GpuViewProj;
};

float4 main(float3 position : POSITION, float2 uv : TEXCOORD,
            float3 normal : NORMAL, float4 tangent : TANGENT) : SV_POSITION {
    float4 worldPos = mul(float4(position, 1.0f), World);
    return mul(worldPos, GpuViewProj);
}
