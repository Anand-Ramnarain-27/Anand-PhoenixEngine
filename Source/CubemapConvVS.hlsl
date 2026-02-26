// CubemapConvVS.hlsl
// Vertex shader shared by the irradiance and pre-filter convolution passes.

cbuffer FaceCB : register(b0)
{
    float4x4 vp;
    int flipX;
    int flipZ;
    float roughness;
    float _pad;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0; // explicit index prevents linkage register mismatch
};

VSOut main(float3 pos : POSITION)
{
    VSOut o;

    o.direction = pos;
    if (flipX)
        o.direction.x = -o.direction.x;
    if (flipZ)
        o.direction.z = -o.direction.z;

    // .xyww forces depth = w/w = 1 (renders at far plane)
    float4 clip = mul(float4(pos, 1.0f), vp);
    o.position = clip.xyww;

    return o;
}
