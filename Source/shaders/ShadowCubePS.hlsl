
cbuffer CubeMVP : register(b0){
    float4x4 WorldLightViewProj;
    float4x4 World;
    float3 LightPos;
    float InvRange;
};

float main(float4 position : SV_POSITION, float3 worldPos : POSITION) : SV_TARGET {
    return length(worldPos - LightPos) * InvRange;
}
