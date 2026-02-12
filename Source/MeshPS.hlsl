struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 nrm : NORMAL;
};

float4 main(PSInput input) : SV_TARGET
{
    // BRIGHT PINK — you CAN'T miss this
    return float4(1.0f, 0.0f, 1.0f, 1.0f);
}
